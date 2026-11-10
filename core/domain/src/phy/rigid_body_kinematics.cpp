#include "shunya/phy/rigid_body_kinematics.hpp"

namespace shunya::phy {

using namespace core::domain::mechanics;

Vector3d RigidBodyKinematics::world_to_body(const Quaterniond& q, const Vector3d& v_world) noexcept {
    return q.conjugate().rotate(v_world);
}

Vector3d RigidBodyKinematics::body_to_world(const Quaterniond& q, const Vector3d& v_body) noexcept {
    return q.rotate(v_body);
}

Vector3d RigidBodyKinematics::angular_momentum_world(const RotationalBodyState& state) noexcept {
    const Vector3d w_body = world_to_body(state.orientation, state.angular_velocity_world);
    const Vector3d L_body = state.local_inertia * w_body;
    return body_to_world(state.orientation, L_body);
}

double RigidBodyKinematics::rotational_kinetic_energy(const RotationalBodyState& state) noexcept {
    const Vector3d w_body = world_to_body(state.orientation, state.angular_velocity_world);
    const Vector3d L_body = state.local_inertia * w_body;
    return 0.5 * w_body.dot(L_body);
}

Quaterniond RigidBodyKinematics::quaternion_derivative(const Quaterniond& q, const Vector3d& omega_world) noexcept {
    const Quaterniond wq{0.0, omega_world.x, omega_world.y, omega_world.z};
    const Quaterniond dq = wq * q;
    return {0.5 * dq.w, 0.5 * dq.x, 0.5 * dq.y, 0.5 * dq.z};
}

void RigidBodyKinematics::step_rk4(
    RotationalBodyState& state,
    double dt_s,
    const Vector3d& force_world,
    const Vector3d& torque_world) noexcept {

    const double inv_m = (state.mass_kg > 0.0) ? (1.0 / state.mass_kg) : 0.0;
    const Vector3d a_lin = force_world * inv_m;

    struct Deriv {
        Quaterniond dq;
        Vector3d domega_body;
    };

    auto eval_deriv = [&](const Quaterniond& q, const Vector3d& w_body) -> Deriv {
        const Quaterniond qn = q.normalized();
        const Vector3d w_world = body_to_world(qn, w_body);
        const Vector3d tau_body = world_to_body(qn, torque_world);
        const Vector3d Lw = state.local_inertia * w_body;
        // Euler equation in principal/body frame: I * d(omega_b)/dt = tau_b - omega_b x (I * omega_b)
        const Vector3d dw_body = state.inv_local_inertia * (tau_body - w_body.cross(Lw));
        return {quaternion_derivative(qn, w_world), dw_body};
    };

    const Quaterniond q0 = state.orientation.normalized();
    const Vector3d w0 = world_to_body(q0, state.angular_velocity_world);

    const Deriv k1 = eval_deriv(q0, w0);

    auto q_add = [](const Quaterniond& a, const Quaterniond& b, double s) -> Quaterniond {
        return {a.w + s * b.w, a.x + s * b.x, a.y + s * b.y, a.z + s * b.z};
    };

    const Deriv k2 = eval_deriv(q_add(q0, k1.dq, 0.5 * dt_s), w0 + k1.domega_body * (0.5 * dt_s));
    const Deriv k3 = eval_deriv(q_add(q0, k2.dq, 0.5 * dt_s), w0 + k2.domega_body * (0.5 * dt_s));
    const Deriv k4 = eval_deriv(q_add(q0, k3.dq, dt_s), w0 + k3.domega_body * dt_s);

    const double h6 = dt_s / 6.0;
    Quaterniond q_next{
        q0.w + h6 * (k1.dq.w + 2.0 * k2.dq.w + 2.0 * k3.dq.w + k4.dq.w),
        q0.x + h6 * (k1.dq.x + 2.0 * k2.dq.x + 2.0 * k3.dq.x + k4.dq.x),
        q0.y + h6 * (k1.dq.y + 2.0 * k2.dq.y + 2.0 * k3.dq.y + k4.dq.y),
        q0.z + h6 * (k1.dq.z + 2.0 * k2.dq.z + 2.0 * k3.dq.z + k4.dq.z)
    };
    q_next = q_next.normalized();

    const Vector3d w_next_body = w0 + (k1.domega_body + k2.domega_body * 2.0 + k3.domega_body * 2.0 + k4.domega_body) * h6;

    state.orientation = q_next;
    state.angular_velocity_world = body_to_world(q_next, w_next_body);
    state.position += state.linear_velocity * dt_s + a_lin * (0.5 * dt_s * dt_s);
    state.linear_velocity += a_lin * dt_s;
}

core::base::Result<RotationalBodyState> RigidBodyKinematics::from_closed_mesh(
    const core::domain::TriangleMesh& mesh,
    double density_kg_m3) {
    auto props = InertiaTensorCalculator::compute_from_closed_mesh(mesh, density_kg_m3);
    if (!props.is_ok()) {
        return core::base::Result<RotationalBodyState>::err(props.error());
    }
    RotationalBodyState s;
    s.position = props.value().center_of_mass;
    s.mass_kg = props.value().mass_kg;
    s.set_local_inertia(props.value().inertia_body_com);
    return core::base::Result<RotationalBodyState>::ok(s);
}

} // namespace shunya::phy
