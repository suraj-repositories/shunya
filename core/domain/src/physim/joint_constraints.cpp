#include "shunya/physim/joint_constraints.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace shunya::physim {

namespace {

RigidBodyEntity* find_entity(std::vector<RigidBodyEntity>& bodies, const std::string& id) {
    for (auto& b : bodies) {
        if (b.id() == id) return &b;
    }
    return nullptr;
}

Matrix3x3 skew_symmetric(const Vector3d& r) noexcept {
    return Matrix3x3(
         0.0, -r.z,  r.y,
         r.z,  0.0, -r.x,
        -r.y,  r.x,  0.0
    );
}

Matrix3x3 compute_point_k_matrix(
    double inv_ma, const Matrix3x3& inv_Ia, const Vector3d& ra,
    double inv_mb, const Matrix3x3& inv_Ib, const Vector3d& rb) noexcept {

    Matrix3x3 K = Matrix3x3::diagonal(inv_ma + inv_mb, inv_ma + inv_mb, inv_ma + inv_mb);
    if (inv_ma > 0.0) {
        const Matrix3x3 Ra = skew_symmetric(ra);
        const Matrix3x3 term_a = Ra.transpose() * inv_Ia * Ra;
        for (size_t i = 0; i < 9; ++i) K.m[i] += term_a.m[i];
    }
    if (inv_mb > 0.0) {
        const Matrix3x3 Rb = skew_symmetric(rb);
        const Matrix3x3 term_b = Rb.transpose() * inv_Ib * Rb;
        for (size_t i = 0; i < 9; ++i) K.m[i] += term_b.m[i];
    }
    return K;
}

} // namespace

// =============================================================================
// Revolute Joint (PH4-09.03.001)
// =============================================================================

void JointConstraintSolver::solve_revolute_joints(
    std::vector<RigidBodyEntity>& bodies,
    std::vector<RevoluteJointSpec>& joints,
    double dt_s,
    int iterations) noexcept {

    if (dt_s <= 0.0) return;

    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& j : joints) {
            auto* ent_a = find_entity(bodies, j.body_a_id);
            auto* ent_b = find_entity(bodies, j.body_b_id);
            if (!ent_a || !ent_b) continue;

            auto& sa = ent_a->state();
            auto& sb = ent_b->state();

            if (sa.inv_mass <= 0.0 && sb.inv_mass <= 0.0) continue;

            const Vector3d pa = sa.local_to_world_point(j.local_anchor_a);
            const Vector3d pb = sb.local_to_world_point(j.local_anchor_b);
            const Vector3d ra = pa - sa.position;
            const Vector3d rb = pb - sb.position;

            // 1. Point Anchor Linear Constraint: eliminate relative linear velocity & drift
            const Vector3d va = sa.linear_velocity() + sa.angular_velocity().cross(ra);
            const Vector3d vb = sb.linear_velocity() + sb.angular_velocity().cross(rb);
            const Vector3d v_rel = va - vb;

            const Vector3d pos_err = pa - pb;
            const Vector3d bias_lin = pos_err * (0.2 / dt_s);

            const Matrix3x3 K_lin = compute_point_k_matrix(
                sa.inv_mass, sa.inv_world_inertia_tensor(), ra,
                sb.inv_mass, sb.inv_world_inertia_tensor(), rb);
            const Matrix3x3 inv_K_lin = K_lin.inverse();

            const Vector3d impulse_lin = inv_K_lin * ((-v_rel) - bias_lin);
            sa.apply_impulse_at_world_point(impulse_lin, pa);
            sb.apply_impulse_at_world_point(impulse_lin * (-1.0), pb);

            // 2. Axis Alignment Angular Constraint: lock perpendicular rotation DOFs
            const Vector3d axis_a = sa.local_to_world_vector(j.local_axis_a).normalized();
            const Vector3d axis_b = sb.local_to_world_vector(j.local_axis_b).normalized();

            Vector3d u1{0.0, 0.0, 0.0};
            if (std::abs(axis_a.x) < 0.8) {
                u1 = axis_a.cross({1.0, 0.0, 0.0}).normalized();
            } else {
                u1 = axis_a.cross({0.0, 1.0, 0.0}).normalized();
            }
            const Vector3d u2 = axis_a.cross(u1).normalized();

            const Vector3d w_rel = sa.angular_velocity() - sb.angular_velocity();
            const Matrix3x3 inv_I_sum = sa.inv_world_inertia_tensor() * (sa.inv_mass > 0 ? 1.0 : 0.0) +
                                        sb.inv_world_inertia_tensor() * (sb.inv_mass > 0 ? 1.0 : 0.0);

            for (const auto& u : {u1, u2}) {
                const double w_perp = w_rel.dot(u);
                const double axis_err = axis_a.cross(axis_b).dot(u);
                const double bias_ang = axis_err * (0.2 / dt_s);

                const double eff_i = u.dot(inv_I_sum * u);
                if (eff_i > 1e-12) {
                    const double d_lambda = (-w_perp - bias_ang) / eff_i;
                    sa.apply_angular_impulse(u * d_lambda);
                    sb.apply_angular_impulse(u * (-d_lambda));
                }
            }

            // 3. Motor & Joint Friction
            if (j.enable_motor || j.joint_friction_torque > 0.0 || j.joint_damping > 0.0) {
                const double w_hinge = (sb.angular_velocity() - sa.angular_velocity()).dot(axis_a);
                const double eff_i_hinge = axis_a.dot(inv_I_sum * axis_a);

                if (eff_i_hinge > 1e-12) {
                    if (j.enable_motor) {
                        const double w_err = j.motor_target_velocity_rad_s - w_hinge;
                        const double d_lambda = w_err / eff_i_hinge;
                        const double max_motor_imp = j.motor_max_torque_nm * dt_s;
                        const double clamped_imp = std::clamp(d_lambda, -max_motor_imp, max_motor_imp);
                        sb.apply_angular_impulse(axis_a * clamped_imp);
                        sa.apply_angular_impulse(axis_a * (-clamped_imp));
                    }

                    // Damping and dry friction
                    const double tau_resist = -j.joint_damping * w_hinge;
                    const double damp_imp = (tau_resist * dt_s) / eff_i_hinge;
                    sb.apply_angular_impulse(axis_a * damp_imp);
                    sa.apply_angular_impulse(axis_a * (-damp_imp));
                }
            }
        }
    }
}

// =============================================================================
// Prismatic Joint (PH4-09.03.002)
// =============================================================================

void JointConstraintSolver::solve_prismatic_joints(
    std::vector<RigidBodyEntity>& bodies,
    std::vector<PrismaticJointSpec>& joints,
    double dt_s,
    int iterations) noexcept {

    if (dt_s <= 0.0) return;

    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& j : joints) {
            auto* ent_a = find_entity(bodies, j.body_a_id);
            auto* ent_b = find_entity(bodies, j.body_b_id);
            if (!ent_a || !ent_b) continue;

            auto& sa = ent_a->state();
            auto& sb = ent_b->state();

            if (sa.inv_mass <= 0.0 && sb.inv_mass <= 0.0) continue;

            const Vector3d pa = sa.local_to_world_point(j.local_anchor_a);
            const Vector3d pb = sb.local_to_world_point(j.local_anchor_b);
            const Vector3d ra = pa - sa.position;
            const Vector3d rb = pb - sb.position;

            const Vector3d axis_s = sa.local_to_world_vector(j.local_axis_a).normalized();

            Vector3d t1{0.0, 0.0, 0.0};
            if (std::abs(axis_s.x) < 0.8) {
                t1 = axis_s.cross({1.0, 0.0, 0.0}).normalized();
            } else {
                t1 = axis_s.cross({0.0, 1.0, 0.0}).normalized();
            }
            const Vector3d t2 = axis_s.cross(t1).normalized();

            // 1. Lock translation perpendicular to slider axis
            const Vector3d va = sa.linear_velocity() + sa.angular_velocity().cross(ra);
            const Vector3d vb = sb.linear_velocity() + sb.angular_velocity().cross(rb);
            const Vector3d v_rel = va - vb;
            const Vector3d d_pos = pa - pb;

            for (const auto& t : {t1, t2}) {
                const double vt = v_rel.dot(t);
                const double err_t = d_pos.dot(t);
                const double bias = err_t * (0.2 / dt_s);

                const double k_t = sa.inv_mass + sb.inv_mass +
                    (ra.cross(t)).dot(sa.inv_world_inertia_tensor() * ra.cross(t)) +
                    (rb.cross(t)).dot(sb.inv_world_inertia_tensor() * rb.cross(t));

                if (k_t > 1e-12) {
                    const double imp = (-vt - bias) / k_t;
                    sa.apply_impulse_at_world_point(t * imp, pa);
                    sb.apply_impulse_at_world_point(t * (-imp), pb);
                }
            }

            // 2. Lock all 3 rotational DOFs
            const Vector3d w_rel = sa.angular_velocity() - sb.angular_velocity();
            const Matrix3x3 inv_I_sum = sa.inv_world_inertia_tensor() + sb.inv_world_inertia_tensor();
            const Matrix3x3 I_eff = inv_I_sum.inverse();

            const Vector3d imp_rot = I_eff * (w_rel * (-1.0));
            sa.apply_angular_impulse(imp_rot);
            sb.apply_angular_impulse(imp_rot * (-1.0));

            // 3. Limits and Motor along slider axis
            const double displacement = (pb - pa).dot(axis_s);
            const double vs = (vb - va).dot(axis_s);

            if (j.enable_limits) {
                double limit_err = 0.0;
                if (displacement < j.lower_limit_m) {
                    limit_err = displacement - j.lower_limit_m;
                } else if (displacement > j.upper_limit_m) {
                    limit_err = displacement - j.upper_limit_m;
                }

                if (std::abs(limit_err) > 1e-4) {
                    const double k_s = sa.inv_mass + sb.inv_mass;
                    if (k_s > 1e-12) {
                        const double imp_limit = (-vs - (limit_err * 0.2 / dt_s)) / k_s;
                        sa.apply_impulse_at_world_point(axis_s * (-imp_limit), pa);
                        sb.apply_impulse_at_world_point(axis_s * imp_limit, pb);
                    }
                }
            }
        }
    }
}

// =============================================================================
// Ball-and-Socket Joint (PH4-09.03.003)
// =============================================================================

void JointConstraintSolver::solve_ball_joints(
    std::vector<RigidBodyEntity>& bodies,
    std::vector<BallAndSocketJointSpec>& joints,
    double dt_s,
    int iterations) noexcept {

    if (dt_s <= 0.0) return;

    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& j : joints) {
            auto* ent_a = find_entity(bodies, j.body_a_id);
            auto* ent_b = find_entity(bodies, j.body_b_id);
            if (!ent_a || !ent_b) continue;

            auto& sa = ent_a->state();
            auto& sb = ent_b->state();

            if (sa.inv_mass <= 0.0 && sb.inv_mass <= 0.0) continue;

            const Vector3d pa = sa.local_to_world_point(j.local_anchor_a);
            const Vector3d pb = sb.local_to_world_point(j.local_anchor_b);
            const Vector3d ra = pa - sa.position;
            const Vector3d rb = pb - sb.position;

            // 3D Anchor Constraint
            const Vector3d va = sa.linear_velocity() + sa.angular_velocity().cross(ra);
            const Vector3d vb = sb.linear_velocity() + sb.angular_velocity().cross(rb);
            const Vector3d v_rel = va - vb;

            const Vector3d pos_err = pa - pb;
            const Vector3d bias = pos_err * (0.2 / dt_s);

            const Matrix3x3 K = compute_point_k_matrix(
                sa.inv_mass, sa.inv_world_inertia_tensor(), ra,
                sb.inv_mass, sb.inv_world_inertia_tensor(), rb);
            const Matrix3x3 inv_K = K.inverse();

            const Vector3d imp = inv_K * ((-v_rel) - bias);
            sa.apply_impulse_at_world_point(imp, pa);
            sb.apply_impulse_at_world_point(imp * (-1.0), pb);

            // Cone Limit
            if (j.enable_cone_limit) {
                const Vector3d u_a = sa.local_to_world_vector(j.primary_axis_a).normalized();
                const Vector3d u_b = sb.local_to_world_vector(j.primary_axis_b).normalized();
                const double cos_theta = std::clamp(u_a.dot(u_b), -1.0, 1.0);
                const double theta = std::acos(cos_theta);

                if (theta > j.max_cone_angle_rad) {
                    const Vector3d rot_axis = u_a.cross(u_b).normalized();
                    const double cone_err = theta - j.max_cone_angle_rad;
                    const double w_rot = (sa.angular_velocity() - sb.angular_velocity()).dot(rot_axis);

                    const Matrix3x3 inv_I_sum = sa.inv_world_inertia_tensor() + sb.inv_world_inertia_tensor();
                    const double eff_i = rot_axis.dot(inv_I_sum * rot_axis);
                    if (eff_i > 1e-12) {
                        const double d_lambda = (-w_rot - (cone_err * 0.2 / dt_s)) / eff_i;
                        sa.apply_angular_impulse(rot_axis * d_lambda);
                        sb.apply_angular_impulse(rot_axis * (-d_lambda));
                    }
                }
            }
        }
    }
}

// =============================================================================
// Wheel Axle Joint & Suspension (PH4-09.03.004)
// =============================================================================

void JointConstraintSolver::solve_wheel_suspension(
    std::vector<RigidBodyEntity>& bodies,
    std::vector<WheelAxleJointSpec>& joints,
    double dt_s) noexcept {

    for (auto& j : joints) {
        auto* chassis = find_entity(bodies, j.chassis_body_id);
        auto* wheel = find_entity(bodies, j.wheel_body_id);
        if (!chassis || !wheel) continue;

        auto& sc = chassis->state();
        auto& sw = wheel->state();

        const Vector3d p_chassis = sc.local_to_world_point(j.local_anchor_chassis);
        const Vector3d p_wheel = sw.local_to_world_point(j.local_anchor_wheel);
        const Vector3d susp_axis = sc.local_to_world_vector(j.suspension_axis_local).normalized();

        // Suspension displacement and velocity
        const Vector3d delta_pos = p_chassis - p_wheel;
        const double d_proj = delta_pos.dot(susp_axis);
        const double travel = std::clamp(d_proj - j.suspension_rest_length_m,
                                         j.suspension_min_travel_m, j.suspension_max_travel_m);
        j.current_travel_m = travel;

        const Vector3d v_rel = sc.linear_velocity() - sw.linear_velocity();
        const double v_susp = v_rel.dot(susp_axis);

        // Spring-damper force F = -k * x - c * v
        const double f_spring = -j.suspension_stiffness_n_m * travel - j.suspension_damping_n_s_m * v_susp;
        j.spring_force_n = f_spring;

        const Vector3d force_vec = susp_axis * f_spring;
        sc.apply_force_at_world_point(force_vec, p_chassis);
        sw.apply_force_at_world_point(force_vec * (-1.0), p_wheel);

        // Keep wheel aligned on suspension axis (remove lateral drift)
        const Vector3d lat_drift = delta_pos - susp_axis * d_proj;
        if (lat_drift.length() > 1e-4) {
            const double k_lat = sc.inv_mass + sw.inv_mass;
            if (k_lat > 1e-12) {
                const Vector3d corr = lat_drift * (-0.2 / (k_lat * dt_s));
                sw.apply_impulse_at_world_point(corr * sw.inv_mass, p_wheel);
                sc.apply_impulse_at_world_point(corr * (-sc.inv_mass), p_chassis);
            }
        }
    }
}

// =============================================================================
// Mechanical Gear Constraint (PH4-09.03.005)
// =============================================================================

void JointConstraintSolver::solve_gear_constraints(
    std::vector<RigidBodyEntity>& bodies,
    const std::vector<GearConstraintSpec>& gears,
    double /*dt_s*/) noexcept {

    for (const auto& g : gears) {
        auto* g1 = find_entity(bodies, g.gear1_body_id);
        auto* g2 = find_entity(bodies, g.gear2_body_id);
        if (!g1 || !g2) continue;

        auto& s1 = g1->state();
        auto& s2 = g2->state();

        const Vector3d a1 = s1.local_to_world_vector(g.gear1_axis_local).normalized();
        const Vector3d a2 = s2.local_to_world_vector(g.gear2_axis_local).normalized();

        const double w1 = s1.angular_velocity().dot(a1);
        const double w2 = s2.angular_velocity().dot(a2);

        // Gear constraint: w1 + R * w2 = 0
        const double R = g.gear_ratio();
        const double v_gear = w1 + R * w2;

        const double i1_inv = a1.dot(s1.inv_world_inertia_tensor() * a1);
        const double i2_inv = a2.dot(s2.inv_world_inertia_tensor() * a2);
        const double eff_i = i1_inv + (R * R) * i2_inv;

        if (eff_i > 1e-12) {
            const double lambda = -v_gear / eff_i;
            s1.apply_angular_impulse(a1 * lambda);
            s2.apply_angular_impulse(a2 * (lambda * R));
        }
    }
}

// =============================================================================
// Pulley and Belt Constraint (PH4-09.03.006)
// =============================================================================

void JointConstraintSolver::solve_pulley_belt_constraints(
    std::vector<RigidBodyEntity>& bodies,
    const std::vector<PulleyBeltConstraintSpec>& belts,
    double dt_s) noexcept {

    for (const auto& b : belts) {
        auto* p1 = find_entity(bodies, b.pulley1_body_id);
        auto* p2 = find_entity(bodies, b.pulley2_body_id);
        if (!p1 || !p2) continue;

        auto& s1 = p1->state();
        auto& s2 = p2->state();

        const Vector3d a1 = s1.local_to_world_vector(b.pulley1_axis_local).normalized();
        const Vector3d a2 = s2.local_to_world_vector(b.pulley2_axis_local).normalized();

        const double w1 = s1.angular_velocity().dot(a1);
        const double w2 = s2.angular_velocity().dot(a2);

        const double v_surf1 = w1 * b.pulley1_radius_m;
        const double sign = b.is_crossed_belt ? -1.0 : 1.0;
        const double v_surf2 = sign * w2 * b.pulley2_radius_m;

        const double v_slip = v_surf1 - v_surf2;

        const double i1_inv = a1.dot(s1.inv_world_inertia_tensor() * a1);
        const double i2_inv = a2.dot(s2.inv_world_inertia_tensor() * a2);
        const double eff_mass = (b.pulley1_radius_m * b.pulley1_radius_m) * i1_inv +
                                (b.pulley2_radius_m * b.pulley2_radius_m) * i2_inv;

        if (eff_mass > 1e-12) {
            const double lambda_belt = -v_slip / eff_mass;
            const double max_belt_imp = (b.max_torque_capacity_nm / b.pulley1_radius_m) * dt_s;
            const double clamped_imp = std::clamp(lambda_belt, -max_belt_imp, max_belt_imp);

            s1.apply_angular_impulse(a1 * (clamped_imp * b.pulley1_radius_m));
            s2.apply_angular_impulse(a2 * (-sign * clamped_imp * b.pulley2_radius_m));
        }
    }
}

// =============================================================================
// FourBarLinkageAssembly (PH4-09.03.007)
// =============================================================================

GrashofClass FourBarLinkageAssembly::classify_grashof() const noexcept {
    std::array<std::pair<double, int>, 4> links = {
        std::make_pair(ground_length_m, 1),
        std::make_pair(crank_length_m, 2),
        std::make_pair(coupler_length_m, 3),
        std::make_pair(rocker_length_m, 4)
    };
    std::sort(links.begin(), links.end());

    const double s = links[0].first;
    const double p = links[1].first;
    const double q = links[2].first;
    const double l = links[3].first;

    if (s + l > p + q + 1e-9) {
        return GrashofClass::NonGrashof;
    }

    const int shortest_id = links[0].second;
    if (shortest_id == 2) {
        return GrashofClass::CrankRocker;
    } else if (shortest_id == 1) {
        return GrashofClass::DoubleCrank;
    } else if (shortest_id == 3) {
        return GrashofClass::DoubleRocker;
    }
    return GrashofClass::CrankRocker;
}

bool FourBarLinkageAssembly::satisfies_grashof() const noexcept {
    return classify_grashof() != GrashofClass::NonGrashof;
}

FourBarKinematicsResult FourBarLinkageAssembly::solve_forward_kinematics(
    double theta2, bool open_branch) const noexcept {

    FourBarKinematicsResult res;
    // Crank tip A
    const double xa = crank_length_m * std::cos(theta2);
    const double ya = crank_length_m * std::sin(theta2);

    // Distance from A to ground rocker pivot D at (ground_length, 0)
    const double xd = ground_length_m;
    const double yd = 0.0;

    const double delta_x = xd - xa;
    const double delta_y = yd - ya;
    const double s = std::sqrt(delta_x * delta_x + delta_y * delta_y);

    // Triangle inequality for coupler (L3), rocker (L4), and distance s
    if (s > coupler_length_m + rocker_length_m || s < std::abs(coupler_length_m - rocker_length_m)) {
        res.has_solution = false;
        return res;
    }

    const double alpha = std::atan2(delta_y, delta_x);
    const double cos_beta = (rocker_length_m * rocker_length_m + s * s - coupler_length_m * coupler_length_m) /
                            (2.0 * rocker_length_m * s);
    const double beta = std::acos(std::clamp(cos_beta, -1.0, 1.0));

    const double theta4 = open_branch ? (alpha + beta) : (alpha - beta);
    res.rocker_angle_rad = theta4;

    // Rocker tip B
    const double xb = xd - rocker_length_m * std::cos(theta4);
    const double yb = yd - rocker_length_m * std::sin(theta4);

    res.coupler_angle_rad = std::atan2(yb - ya, xb - xa);

    // Loop closure error: || (A - O) + (B - A) - (B - D) - (D - O) ||
    const double err_x = xa + coupler_length_m * std::cos(res.coupler_angle_rad) -
                         (ground_length_m - rocker_length_m * std::cos(theta4));
    const double err_y = ya + coupler_length_m * std::sin(res.coupler_angle_rad) -
                         (-rocker_length_m * std::sin(theta4));
    res.loop_closure_error_m = std::sqrt(err_x * err_x + err_y * err_y);
    res.has_solution = true;

    return res;
}

double JointConstraintSolver::compute_max_joint_anchor_error(
    const std::vector<RigidBodyEntity>& bodies,
    const std::vector<RevoluteJointSpec>& revolute_joints,
    const std::vector<BallAndSocketJointSpec>& ball_joints) noexcept {

    double max_err = 0.0;
    for (const auto& j : revolute_joints) {
        const auto* a = find_entity(const_cast<std::vector<RigidBodyEntity>&>(bodies), j.body_a_id);
        const auto* b = find_entity(const_cast<std::vector<RigidBodyEntity>&>(bodies), j.body_b_id);
        if (a && b) {
            const double d = (a->state().local_to_world_point(j.local_anchor_a) -
                              b->state().local_to_world_point(j.local_anchor_b)).length();
            max_err = std::max(max_err, d);
        }
    }
    for (const auto& j : ball_joints) {
        const auto* a = find_entity(const_cast<std::vector<RigidBodyEntity>&>(bodies), j.body_a_id);
        const auto* b = find_entity(const_cast<std::vector<RigidBodyEntity>&>(bodies), j.body_b_id);
        if (a && b) {
            const double d = (a->state().local_to_world_point(j.local_anchor_a) -
                              b->state().local_to_world_point(j.local_anchor_b)).length();
            max_err = std::max(max_err, d);
        }
    }
    return max_err;
}

} // namespace shunya::physim
