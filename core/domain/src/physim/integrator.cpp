#include "shunya/physim/integrator.hpp"

#include <cmath>
#include <numbers>

namespace shunya::physim {

void RigidBodyIntegrator::step_symplectic_euler(
    RigidBodyState& state,
    double dt_s,
    const Vector3d& constant_force,
    const Vector3d& constant_torque) noexcept {
    if (state.inv_mass <= 0.0) return;

    // 1. Update linear and angular momentum via impulses
    const Vector3d total_force = state.force_accumulator + constant_force;
    const Vector3d total_torque = state.torque_accumulator + constant_torque;

    state.linear_momentum += total_force * dt_s;
    state.angular_momentum += total_torque * dt_s;

    // 2. Derive linear velocity from new momentum and advance position
    const Vector3d v = state.linear_velocity();
    state.position += v * dt_s;

    // 3. Derive angular velocity from new momentum and advance orientation
    const Vector3d omega = state.angular_velocity();
    const Quaterniond w_quat{0.0, omega.x, omega.y, omega.z};
    const Quaterniond dq = (w_quat * state.orientation) * (0.5 * dt_s);
    state.orientation = (state.orientation + dq).normalized();

    // 4. Reset accumulators for next step
    state.clear_accumulators();
}

namespace {

struct StateDeriv {
    Vector3d dx;
    Quaterniond dq;
    Vector3d dp;
    Vector3d dL;
};

StateDeriv evaluate_derivatives(
    const RigidBodyState& base_state,
    const Vector3d& pos,
    const Quaterniond& orient,
    const Vector3d& lin_mom,
    const Vector3d& ang_mom,
    const RigidBodyIntegrator::ForceTorqueCallback& force_torque_fn) noexcept {

    const Quaterniond q_norm = orient.normalized();
    const Matrix3x3 R = Matrix3x3::from_quaternion(q_norm);
    const Matrix3x3 inv_I_world = R * base_state.inv_local_inertia * R.transpose();

    const Vector3d v = lin_mom * base_state.inv_mass;
    const Vector3d omega = inv_I_world * ang_mom;

    const Quaterniond w_quat{0.0, omega.x, omega.y, omega.z};
    const Quaterniond dq = (w_quat * q_norm) * 0.5;

    Vector3d force = base_state.force_accumulator;
    Vector3d torque = base_state.torque_accumulator;

    if (force_torque_fn) {
        RigidBodyState temp = base_state;
        temp.position = pos;
        temp.orientation = q_norm;
        temp.linear_momentum = lin_mom;
        temp.angular_momentum = ang_mom;
        force_torque_fn(temp, force, torque);
    }

    return StateDeriv{v, dq, force, torque};
}

} // namespace

void RigidBodyIntegrator::step_rk4(
    RigidBodyState& state,
    double dt_s,
    const ForceTorqueCallback& force_torque_fn) noexcept {
    if (state.inv_mass <= 0.0) return;

    const Vector3d x0 = state.position;
    const Quaterniond q0 = state.orientation.normalized();
    const Vector3d p0 = state.linear_momentum;
    const Vector3d L0 = state.angular_momentum;

    // Stage 1
    const StateDeriv k1 = evaluate_derivatives(state, x0, q0, p0, L0, force_torque_fn);

    // Stage 2
    const Vector3d x1 = x0 + k1.dx * (0.5 * dt_s);
    const Quaterniond q1 = (q0 + k1.dq * (0.5 * dt_s)).normalized();
    const Vector3d p1 = p0 + k1.dp * (0.5 * dt_s);
    const Vector3d L1 = L0 + k1.dL * (0.5 * dt_s);
    const StateDeriv k2 = evaluate_derivatives(state, x1, q1, p1, L1, force_torque_fn);

    // Stage 3
    const Vector3d x2 = x0 + k2.dx * (0.5 * dt_s);
    const Quaterniond q2 = (q0 + k2.dq * (0.5 * dt_s)).normalized();
    const Vector3d p2 = p0 + k2.dp * (0.5 * dt_s);
    const Vector3d L2 = L0 + k2.dL * (0.5 * dt_s);
    const StateDeriv k3 = evaluate_derivatives(state, x2, q2, p2, L2, force_torque_fn);

    // Stage 4
    const Vector3d x3 = x0 + k3.dx * dt_s;
    const Quaterniond q3 = (q0 + k3.dq * dt_s).normalized();
    const Vector3d p3 = p0 + k3.dp * dt_s;
    const Vector3d L3 = L0 + k3.dL * dt_s;
    const StateDeriv k4 = evaluate_derivatives(state, x3, q3, p3, L3, force_torque_fn);

    // Combine RK4 increments
    const double sixth = dt_s / 6.0;
    state.position = x0 + (k1.dx + k2.dx * 2.0 + k3.dx * 2.0 + k4.dx) * sixth;
    const Quaterniond dq_net = (k1.dq + k2.dq * 2.0 + k3.dq * 2.0 + k4.dq) * sixth;
    state.orientation = (q0 + dq_net).normalized();
    state.linear_momentum = p0 + (k1.dp + k2.dp * 2.0 + k3.dp * 2.0 + k4.dp) * sixth;
    state.angular_momentum = L0 + (k1.dL + k2.dL * 2.0 + k3.dL * 2.0 + k4.dL) * sixth;

    state.clear_accumulators();
}

void RigidBodyIntegrator::step_system(
    std::vector<RigidBodyEntity>& bodies,
    double dt_s,
    IntegratorScheme scheme,
    const Vector3d& gravity_vector) noexcept {
    for (auto& body : bodies) {
        if (body.is_static()) {
            continue;
        }

        if (body.is_kinematic()) {
            // Kinematic bodies advance according to their prescribed velocity
            auto& st = body.state();
            st.position += st.linear_velocity() * dt_s;
            const Vector3d omega = st.angular_velocity();
            const Quaterniond w_quat{0.0, omega.x, omega.y, omega.z};
            const Quaterniond dq = (w_quat * st.orientation) * (0.5 * dt_s);
            st.orientation = (st.orientation + dq).normalized();
            continue;
        }

        // Dynamic bodies: apply environmental forces (gravity, buoyancy, drag)
        EnvironmentForces::apply_environmental_forces(body.state(), gravity_vector, body.aero());

        if (scheme == IntegratorScheme::RungeKutta4) {
            step_rk4(body.state(), dt_s);
        } else {
            step_symplectic_euler(body.state(), dt_s);
        }
    }
}

// =============================================================================
// Analytical Classical Mechanics Validators (PH4-09.01.006)
// =============================================================================

AnalyticalFreeFallResult RigidBodyIntegrator::analytical_free_fall(
    double y0, double v0, double g, double t_s) noexcept {
    AnalyticalFreeFallResult res;
    res.position_y = y0 + v0 * t_s - 0.5 * g * t_s * t_s;
    res.velocity_y = v0 - g * t_s;
    return res;
}

double RigidBodyIntegrator::analytical_terminal_velocity(
    double mass_kg, double g, double fluid_density, double cd, double area_m2) noexcept {
    const double denom = fluid_density * cd * area_m2;
    if (denom <= 0.0) return 0.0;
    return std::sqrt((2.0 * mass_kg * g) / denom);
}

AnalyticalProjectileResult RigidBodyIntegrator::analytical_projectile(
    const Vector3d& r0, const Vector3d& v0, double g, double t_s) noexcept {
    AnalyticalProjectileResult res;
    res.position = {
        r0.x + v0.x * t_s,
        r0.y + v0.y * t_s - 0.5 * g * t_s * t_s,
        r0.z + v0.z * t_s
    };
    res.velocity = {
        v0.x,
        v0.y - g * t_s,
        v0.z
    };

    res.max_height = r0.y + (v0.y * v0.y) / (2.0 * g);

    // Time of flight: roots of y0 + v0.y * t - 0.5 * g * t^2 = 0
    const double discr = v0.y * v0.y + 2.0 * g * r0.y;
    if (discr >= 0.0 && g > 0.0) {
        res.flight_time = (v0.y + std::sqrt(discr)) / g;
        const double v_horiz = std::sqrt(v0.x * v0.x + v0.z * v0.z);
        res.range = v_horiz * res.flight_time;
    }
    return res;
}

AnalyticalPrecessionResult RigidBodyIntegrator::analytical_gyroscopic_precession(
    double mass_kg, double g, double pivot_dist_m, double izz_kg_m2, double spin_rate_rad_s) noexcept {
    AnalyticalPrecessionResult res;
    const double tau = mass_kg * g * pivot_dist_m;
    const double ang_mom = izz_kg_m2 * spin_rate_rad_s;
    if (std::abs(ang_mom) > 1e-12) {
        res.precession_frequency_rad_s = tau / ang_mom;
        res.precession_period_s = (2.0 * std::numbers::pi) / res.precession_frequency_rad_s;
    }
    return res;
}

} // namespace shunya::physim
