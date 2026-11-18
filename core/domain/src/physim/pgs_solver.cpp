#include "shunya/physim/pgs_solver.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace shunya::physim {

namespace {

RigidBodyEntity* find_entity(std::vector<RigidBodyEntity>& bodies, const std::string& id) {
    for (auto& b : bodies) {
        if (b.id() == id) return &b;
    }
    return nullptr;
}

double compute_effective_mass_inv(
    const RigidBodyState& state_a,
    const Vector3d& ra,
    const RigidBodyState& state_b,
    const Vector3d& rb,
    const Vector3d& dir) noexcept {

    double k = state_a.inv_mass + state_b.inv_mass;
    if (state_a.inv_mass > 0.0) {
        const Vector3d ra_cross_d = ra.cross(dir);
        const Vector3d inv_i_rot = state_a.inv_world_inertia_tensor() * ra_cross_d;
        k += ra_cross_d.dot(inv_i_rot);
    }
    if (state_b.inv_mass > 0.0) {
        const Vector3d rb_cross_d = rb.cross(dir);
        const Vector3d inv_i_rot = state_b.inv_world_inertia_tensor() * rb_cross_d;
        k += rb_cross_d.dot(inv_i_rot);
    }
    return k;
}

} // namespace

void PgsContactSolver::solve_contacts(
    std::vector<RigidBodyEntity>& bodies,
    std::vector<PhysimContactManifold>& manifolds,
    double dt_s,
    const PgsSolverOptions& options) noexcept {

    if (bodies.empty() || manifolds.empty() || dt_s <= 0.0) {
        return;
    }

    struct ContactConstraintInternal {
        RigidBodyState* state_a{nullptr};
        RigidBodyState* state_b{nullptr};
        PhysimContactPoint* cp{nullptr};
        Vector3d ra{0.0, 0.0, 0.0};
        Vector3d rb{0.0, 0.0, 0.0};
        Vector3d normal{0.0, 1.0, 0.0};
        Vector3d tangent1{1.0, 0.0, 0.0};
        Vector3d tangent2{0.0, 0.0, 1.0};
        double effective_mass_n{0.0};
        double effective_mass_t1{0.0};
        double effective_mass_t2{0.0};
        double bias_n{0.0};
        double restitution{0.0};
        double friction{0.5};
    };

    std::vector<ContactConstraintInternal> constraints;
    constraints.reserve(manifolds.size() * 4);

    for (auto& m : manifolds) {
        auto* ent_a = find_entity(bodies, m.body_a_id);
        auto* ent_b = find_entity(bodies, m.body_b_id);
        if (!ent_a || !ent_b) continue;

        auto& sa = ent_a->state();
        auto& sb = ent_b->state();

        // Both static: no impulses needed
        if (sa.inv_mass <= 0.0 && sb.inv_mass <= 0.0) continue;

        for (auto& cp : m.contacts) {
            ContactConstraintInternal cci;
            cci.state_a = &sa;
            cci.state_b = &sb;
            cci.cp = &cp;
            cci.normal = cp.normal.normalized();

            // Tangents orthogonal to normal
            if (std::abs(cci.normal.x) < 0.8) {
                cci.tangent1 = cci.normal.cross({1.0, 0.0, 0.0}).normalized();
            } else {
                cci.tangent1 = cci.normal.cross({0.0, 1.0, 0.0}).normalized();
            }
            cci.tangent2 = cci.normal.cross(cci.tangent1).normalized();

            // Contact lever arms
            cci.ra = cp.world_position - sa.position;
            cci.rb = cp.world_position - sb.position;

            // Effective masses
            const double k_n = compute_effective_mass_inv(sa, cci.ra, sb, cci.rb, cci.normal);
            cci.effective_mass_n = (k_n > 1e-12) ? (1.0 / k_n) : 0.0;

            const double k_t1 = compute_effective_mass_inv(sa, cci.ra, sb, cci.rb, cci.tangent1);
            cci.effective_mass_t1 = (k_t1 > 1e-12) ? (1.0 / k_t1) : 0.0;

            const double k_t2 = compute_effective_mass_inv(sa, cci.ra, sb, cci.rb, cci.tangent2);
            cci.effective_mass_t2 = (k_t2 > 1e-12) ? (1.0 / k_t2) : 0.0;

            // Relative contact velocity
            const Vector3d va_p = sa.linear_velocity() + sa.angular_velocity().cross(cci.ra);
            const Vector3d vb_p = sb.linear_velocity() + sb.angular_velocity().cross(cci.rb);
            const Vector3d v_rel = va_p - vb_p;
            const double v_rel_n = v_rel.dot(cci.normal);

            // Restitution or Baumgarte position stabilization bias
            if (v_rel_n < -options.restitution_velocity_threshold) {
                // High-speed impact: restitution governs bounce; no Baumgarte velocity injection
                cci.bias_n = -m.restitution * v_rel_n;
            } else if (v_rel_n < 0.0) {
                // Low-speed approaching contact: apply Baumgarte position stabilization
                const double penetration = std::max(cp.penetration_depth - options.penetration_slop, 0.0);
                cci.bias_n = std::min((options.baumgarte_factor / dt_s) * penetration, 2.0);
            } else {
                // Already separating: zero bias to prevent artificial push
                cci.bias_n = 0.0;
            }
            cci.restitution = m.restitution;
            cci.friction = m.dynamic_friction;

            // Warm starting from previous frame
            if (options.enable_warm_starting && (cp.normal_impulse > 0.0 || cp.tangent_impulse > 0.0)) {
                const Vector3d warm_impulse = cci.normal * cp.normal_impulse +
                                              cci.tangent1 * cp.tangent_impulse;
                sa.apply_impulse_at_world_point(warm_impulse, cp.world_position);
                sb.apply_impulse_at_world_point(warm_impulse * (-1.0), cp.world_position);
            } else {
                cp.normal_impulse = 0.0;
                cp.tangent_impulse = 0.0;
            }

            constraints.push_back(cci);
        }
    }

    // PGS iterative sequential impulse loop
    for (int iter = 0; iter < options.iterations; ++iter) {
        for (auto& c : constraints) {
            auto& sa = *c.state_a;
            auto& sb = *c.state_b;
            auto& cp = *c.cp;

            // 1. Normal Impulse
            const Vector3d va_p = sa.linear_velocity() + sa.angular_velocity().cross(c.ra);
            const Vector3d vb_p = sb.linear_velocity() + sb.angular_velocity().cross(c.rb);
            const Vector3d v_rel = va_p - vb_p;

            const double vn = v_rel.dot(c.normal);
            const double delta_lambda_n = c.effective_mass_n * (-vn + c.bias_n);

            const double old_lambda_n = cp.normal_impulse;
            cp.normal_impulse = std::max(old_lambda_n + delta_lambda_n, 0.0);
            const double d_lambda_n = cp.normal_impulse - old_lambda_n;

            const Vector3d p_impulse_n = c.normal * d_lambda_n;
            sa.apply_impulse_at_world_point(p_impulse_n, cp.world_position);
            sb.apply_impulse_at_world_point(p_impulse_n * (-1.0), cp.world_position);

            // 2. Tangent Friction Impulses (Coulomb Friction Cone)
            const double max_friction = c.friction * cp.normal_impulse;

            // Tangent 1
            const Vector3d va_p_f = sa.linear_velocity() + sa.angular_velocity().cross(c.ra);
            const Vector3d vb_p_f = sb.linear_velocity() + sb.angular_velocity().cross(c.rb);
            const Vector3d v_rel_f = va_p_f - vb_p_f;

            const double vt1 = v_rel_f.dot(c.tangent1);
            const double delta_lambda_t1 = c.effective_mass_t1 * (-vt1);
            const double old_lambda_t1 = cp.tangent_impulse;
            cp.tangent_impulse = std::clamp(old_lambda_t1 + delta_lambda_t1, -max_friction, max_friction);
            const double d_lambda_t1 = cp.tangent_impulse - old_lambda_t1;

            const Vector3d p_impulse_t1 = c.tangent1 * d_lambda_t1;
            sa.apply_impulse_at_world_point(p_impulse_t1, cp.world_position);
            sb.apply_impulse_at_world_point(p_impulse_t1 * (-1.0), cp.world_position);

            // Tangent 2
            const double vt2 = v_rel_f.dot(c.tangent2);
            const double delta_lambda_t2 = c.effective_mass_t2 * (-vt2);
            const Vector3d p_impulse_t2 = c.tangent2 * delta_lambda_t2;
            sa.apply_impulse_at_world_point(p_impulse_t2, cp.world_position);
            sb.apply_impulse_at_world_point(p_impulse_t2 * (-1.0), cp.world_position);
        }
    }
}

// =============================================================================
// Physical Experiment Validators (PH4-09.02.006)
// =============================================================================

BouncingBallExperimentResult PgsContactSolver::simulate_bouncing_ball(
    double drop_height_m,
    double restitution_coeff,
    double ball_radius_m,
    double ball_mass_kg,
    double dt_s,
    int max_steps) noexcept {

    BouncingBallExperimentResult res;
    res.initial_height_m = drop_height_m;

    RigidBodyEntity ball("ball", BodyType::Dynamic);
    ball.state().set_mass(ball_mass_kg);
    ball.state().position = {0.0, drop_height_m, 0.0};
    ball.state().set_local_inertia(Matrix3x3::diagonal(0.4 * ball_mass_kg * ball_radius_m * ball_radius_m,
                                                      0.4 * ball_mass_kg * ball_radius_m * ball_radius_m,
                                                      0.4 * ball_mass_kg * ball_radius_m * ball_radius_m));

    RigidBodyEntity floor("floor", BodyType::Static);
    floor.state().position = {0.0, 0.0, 0.0};

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(ball);
    bodies.push_back(floor);

    const double g = 9.80665;
    bool has_bounced = false;
    double max_rebound_y = 0.0;
    double prev_vy = 0.0;

    for (int step = 0; step < max_steps; ++step) {
        auto& b = bodies[0];

        // Apply gravity
        b.state().apply_force_at_com({0.0, -ball_mass_kg * g, 0.0});

        // Check contact with ground floor (Y = 0)
        std::vector<PhysimContactManifold> manifolds;
        if (b.state().position.y - ball_radius_m <= 0.0) {
            PhysimContactManifold m;
            m.body_a_id = "ball";
            m.body_b_id = "floor";
            m.restitution = restitution_coeff;
            m.dynamic_friction = 0.2;

            PhysimContactPoint cp;
            cp.world_position = {b.state().position.x, 0.0, b.state().position.z};
            cp.normal = {0.0, 1.0, 0.0}; // points upward from floor to ball
            cp.penetration_depth = ball_radius_m - b.state().position.y;
            m.contacts.push_back(cp);
            manifolds.push_back(m);

            if (prev_vy < -0.1) {
                has_bounced = true;
            }
        }

        // Integrate momentum and apply contact impulses
        b.state().linear_momentum += b.state().force_accumulator * dt_s;
        b.state().clear_accumulators();

        if (!manifolds.empty()) {
            PgsSolverOptions opts;
            opts.iterations = 40;
            opts.restitution_velocity_threshold = 0.05;
            solve_contacts(bodies, manifolds, dt_s, opts);
        }

        // Advance position
        b.state().position += b.state().linear_velocity() * dt_s;

        const double cur_vy = b.state().linear_velocity().y;

        if (has_bounced) {
            if (b.state().position.y > max_rebound_y) {
                max_rebound_y = b.state().position.y;
            }
            // Once velocity turns negative again, apex is reached
            if (prev_vy > 0.0 && cur_vy <= 0.0 && max_rebound_y > ball_radius_m) {
                break;
            }
        }
        prev_vy = cur_vy;
    }

    res.first_rebound_height_m = max_rebound_y;
    const double net_initial = drop_height_m - ball_radius_m;
    const double net_rebound = max_rebound_y - ball_radius_m;
    if (net_initial > 0.0 && net_rebound > 0.0) {
        res.measured_restitution = std::sqrt(net_rebound / net_initial);
    }
    return res;
}

InclineSlidingExperimentResult PgsContactSolver::simulate_incline_block(
    double incline_angle_deg,
    double static_friction_coeff,
    double dynamic_friction_coeff,
    double block_mass_kg,
    double duration_s,
    double dt_s) noexcept {

    InclineSlidingExperimentResult res;
    const double theta_rad = (incline_angle_deg * std::numbers::pi) / 180.0;
    const double g = 9.80665;

    const double f_downhill = block_mass_kg * g * std::sin(theta_rad);
    const double normal_force = block_mass_kg * g * std::cos(theta_rad);
    const double max_static_f = static_friction_coeff * normal_force;

    if (f_downhill <= max_static_f) {
        // Block does not slide; static friction prevents motion
        res.is_sliding = false;
        res.acceleration_mps2 = 0.0;
        res.final_velocity_mps = 0.0;
        res.final_distance_m = 0.0;
        return res;
    }

    // Block slides down incline
    res.is_sliding = true;
    const double dynamic_friction_force = dynamic_friction_coeff * normal_force;
    const double net_force = f_downhill - dynamic_friction_force;
    res.acceleration_mps2 = std::max(net_force / block_mass_kg, 0.0);

    // Integrate motion down incline
    double v = 0.0;
    double dist = 0.0;
    const int steps = static_cast<int>(duration_s / dt_s);

    for (int s = 0; s < steps; ++s) {
        v += res.acceleration_mps2 * dt_s;
        dist += v * dt_s;
    }

    res.final_velocity_mps = v;
    res.final_distance_m = dist;
    return res;
}

} // namespace shunya::physim
