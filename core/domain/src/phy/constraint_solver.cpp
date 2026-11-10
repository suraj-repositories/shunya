#include "shunya/phy/constraint_solver.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

using namespace core::domain::mechanics;

namespace {

Matrix3x3 skew_symmetric(const Vector3d& r) noexcept {
    return Matrix3x3(
         0.0, -r.z,  r.y,
         r.z,  0.0, -r.x,
        -r.y,  r.x,  0.0
    );
}

Matrix3x3 world_inv_inertia(const RotationalBodyState& b) noexcept {
    const Matrix3x3 R = Matrix3x3::from_quaternion(b.orientation);
    return R * b.inv_local_inertia * R.transpose();
}

void add_matrices(Matrix3x3& A, const Matrix3x3& B) noexcept {
    for (size_t i = 0; i < 9; ++i) A.m[i] += B.m[i];
}

// Compute 3x3 effective mass inverse J * M^-1 * J^T for point-to-point anchor constraint
Matrix3x3 compute_point_k_matrix(
    double inv_ma, const Matrix3x3& inv_Ia, const Vector3d& ra,
    double inv_mb, const Matrix3x3& inv_Ib, const Vector3d& rb) noexcept {
    Matrix3x3 K = Matrix3x3::diagonal(inv_ma + inv_mb, inv_ma + inv_mb, inv_ma + inv_mb);
    if (inv_ma > 0.0) {
        const Matrix3x3 Ra = skew_symmetric(ra);
        // -Ra * inv_Ia * Ra = Ra^T * inv_Ia * Ra (symmetric PSD)
        add_matrices(K, Ra.transpose() * inv_Ia * Ra);
    }
    if (inv_mb > 0.0) {
        const Matrix3x3 Rb = skew_symmetric(rb);
        add_matrices(K, Rb.transpose() * inv_Ib * Rb);
    }
    return K;
}

void apply_pos_correction(
    RotationalBodyState& b,
    const Vector3d& corr_impulse,
    const Vector3d& r,
    double inv_m,
    const Matrix3x3& inv_I,
    double sign) noexcept {
    if (inv_m <= 0.0) return;
    b.position += corr_impulse * (sign * inv_m);
    const Vector3d dtheta = inv_I * r.cross(corr_impulse * sign);
    const double angle = dtheta.length();
    if (angle > 1e-14) {
        const Quaterniond dq = Quaterniond::from_axis_angle(dtheta / angle, angle);
        b.orientation = (dq * b.orientation).normalized();
    }
}

void apply_vel_impulse(
    RotationalBodyState& b,
    const Vector3d& impulse,
    const Vector3d& r,
    double inv_m,
    const Matrix3x3& inv_I,
    double sign) noexcept {
    if (inv_m <= 0.0) return;
    b.linear_velocity += impulse * (sign * inv_m);
    b.angular_velocity_world += inv_I * r.cross(impulse * sign);
}

} // namespace

void ConstraintSolver::step_world(
    std::vector<RotationalBodyState>& bodies,
    const std::vector<ArticulatedJoint>& joints,
    const std::vector<ContactConstraintSpec>& contacts,
    const Vector3d& gravity_mps2,
    double dt_s,
    const SolverOptions& opts) {

    const int substeps = std::max(1, opts.substeps);
    const double sub_dt = dt_s / static_cast<double>(substeps);

    for (int sub = 0; sub < substeps; ++sub) {
        // 1) Gravity & free-motion torque-free Euler angular momentum update
        for (auto& b : bodies) {
            if (b.mass_kg > 0.0) {
                b.linear_velocity += gravity_mps2 * sub_dt;
            }
        }

        // Precompute world inverse inertia and anchor offsets for this substep
        std::vector<Matrix3x3> inv_I_list(bodies.size());
        for (size_t i = 0; i < bodies.size(); ++i) {
            inv_I_list[i] = (bodies[i].mass_kg > 0.0) ? world_inv_inertia(bodies[i]) : Matrix3x3::zero();
        }

        // Accumulators for clamped contact normal + friction impulses
        std::vector<double> contact_norm_accum(contacts.size(), 0.0);

        // 2) Projected Gauss-Seidel Velocity Solver
        const int vel_iters = (joints.size() > 6) ? std::max(opts.pga_iterations, 100) : opts.pga_iterations;
        for (int iter = 0; iter < vel_iters; ++iter) {
            for (const auto& j : joints) {
                auto& ba = bodies[j.body_a];
                const double inv_ma = (ba.mass_kg > 0.0) ? (1.0 / ba.mass_kg) : 0.0;
                const Vector3d ra = ba.orientation.rotate(j.local_anchor_a);
                const Vector3d pa_w = ba.position + ra;

                Vector3d rb{0, 0, 0};
                Vector3d pb_w = j.local_anchor_b;
                double inv_mb = 0.0;
                Matrix3x3 inv_Ib = Matrix3x3::zero();
                Vector3d vb_rel{0, 0, 0};

                if (!j.static_world_anchor) {
                    const auto& bb = bodies[j.body_b];
                    inv_mb = (bb.mass_kg > 0.0) ? (1.0 / bb.mass_kg) : 0.0;
                    inv_Ib = inv_I_list[j.body_b];
                    rb = bb.orientation.rotate(j.local_anchor_b);
                    pb_w = bb.position + rb;
                    vb_rel = bb.linear_velocity + bb.angular_velocity_world.cross(rb);
                }

                const Vector3d va_rel = ba.linear_velocity + ba.angular_velocity_world.cross(ra);
                const Vector3d pos_err = pa_w - pb_w;
                const Vector3d bias = pos_err * (opts.baumgarte_beta / sub_dt);
                const Matrix3x3 K = compute_point_k_matrix(inv_ma, inv_I_list[j.body_a], ra, inv_mb, inv_Ib, rb);
                const Vector3d lambda = K.inverse() * (-(va_rel - vb_rel) - bias);

                apply_vel_impulse(ba, lambda, ra, inv_ma, inv_I_list[j.body_a], 1.0);
                if (!j.static_world_anchor) {
                    apply_vel_impulse(bodies[j.body_b], lambda, rb, inv_mb, inv_Ib, -1.0);
                }

                // Revolute hinge angular alignment lock (removes 2 perpendicular angular DOFs)
                if (j.type == JointType::Revolute && !j.static_world_anchor) {
                    const auto& bb = bodies[j.body_b];
                    const Vector3d ax_a = ba.orientation.rotate(j.local_axis_a).normalized();
                    const Vector3d ax_b = bb.orientation.rotate(j.local_axis_b).normalized();
                    const Vector3d ang_err = ax_a.cross(ax_b);
                    const Vector3d dw = ba.angular_velocity_world - bb.angular_velocity_world;
                    // Project out components orthogonal to average hinge axis
                    const Vector3d rel_spin = dw + ang_err * (opts.baumgarte_beta * 0.5 / sub_dt);
                    const Vector3d hinge_perp = rel_spin - ax_a * rel_spin.dot(ax_a);
                    const double k_ang = (inv_I_list[j.body_a].at(0, 0) + inv_Ib.at(0, 0) +
                                          inv_I_list[j.body_a].at(1, 1) + inv_Ib.at(1, 1) +
                                          inv_I_list[j.body_a].at(2, 2) + inv_Ib.at(2, 2)) / 3.0;
                    if (k_ang > 1e-12) {
                        const Vector3d ang_imp = hinge_perp * (-0.5 / k_ang);
                        ba.angular_velocity_world += inv_I_list[j.body_a] * ang_imp;
                        bodies[j.body_b].angular_velocity_world -= inv_Ib * ang_imp;
                    }
                }
            }

            // Contact & Coulomb Friction inequality constraints
            for (size_t ci = 0; ci < contacts.size(); ++ci) {
                const auto& c = contacts[ci];
                auto& ba = bodies[c.body_a];
                const double inv_ma = (ba.mass_kg > 0.0) ? (1.0 / ba.mass_kg) : 0.0;
                const Vector3d ra = c.world_point - ba.position;

                Vector3d rb{0, 0, 0};
                double inv_mb = 0.0;
                Matrix3x3 inv_Ib = Matrix3x3::zero();
                Vector3d vb_pt{0, 0, 0};
                if (!c.body_b_static) {
                    const auto& bb = bodies[c.body_b];
                    inv_mb = (bb.mass_kg > 0.0) ? (1.0 / bb.mass_kg) : 0.0;
                    inv_Ib = inv_I_list[c.body_b];
                    rb = c.world_point - bb.position;
                    vb_pt = bb.linear_velocity + bb.angular_velocity_world.cross(rb);
                }

                const Vector3d va_pt = ba.linear_velocity + ba.angular_velocity_world.cross(ra);
                const Vector3d vrel = va_pt - vb_pt;
                const double vn = vrel.dot(c.normal);
                const double eff_mass_inv =
                    inv_ma + inv_mb +
                    c.normal.dot((inv_I_list[c.body_a] * ra.cross(c.normal)).cross(ra)) +
                    c.normal.dot((inv_Ib * rb.cross(c.normal)).cross(rb));

                if (eff_mass_inv <= 1e-12) continue;

                const double pen_bias = std::max(0.0, c.penetration_depth - opts.allowed_penetration) *
                                        (opts.baumgarte_beta / sub_dt);
                const double j_n_raw = (-vn + pen_bias) / eff_mass_inv;
                const double old_accum = contact_norm_accum[ci];
                contact_norm_accum[ci] = std::max(0.0, old_accum + j_n_raw);
                const double dj_n = contact_norm_accum[ci] - old_accum;

                const Vector3d pn = c.normal * dj_n;
                apply_vel_impulse(ba, pn, ra, inv_ma, inv_I_list[c.body_a], 1.0);
                if (!c.body_b_static) {
                    apply_vel_impulse(bodies[c.body_b], pn, rb, inv_mb, inv_Ib, -1.0);
                }

                // Coulomb friction tangent impulse
                const Vector3d vt = vrel - c.normal * vn;
                const double vt_len = vt.length();
                if (vt_len > 1e-9 && c.friction > 0.0) {
                    const Vector3d tdir = vt / vt_len;
                    const double t_mass_inv =
                        inv_ma + inv_mb +
                        tdir.dot((inv_I_list[c.body_a] * ra.cross(tdir)).cross(ra)) +
                        tdir.dot((inv_Ib * rb.cross(tdir)).cross(rb));
                    if (t_mass_inv > 1e-12) {
                        const double jt = std::clamp(-vt_len / t_mass_inv,
                                                     -c.friction * contact_norm_accum[ci],
                                                      c.friction * contact_norm_accum[ci]);
                        const Vector3d pt = tdir * jt;
                        apply_vel_impulse(ba, pt, ra, inv_ma, inv_I_list[c.body_a], 1.0);
                        if (!c.body_b_static) {
                            apply_vel_impulse(bodies[c.body_b], pt, rb, inv_mb, inv_Ib, -1.0);
                        }
                    }
                }
            }
        }

        // 3) Advance pose & orientation
        for (auto& b : bodies) {
            RigidBodyKinematics::step_rk4(b, sub_dt);
        }

        // 4) Non-linear Symmetric Gauss-Seidel position projection enforcing < 1e-4 m across long articulated chains
        const size_t n_joints = joints.size();
        const int max_pos_iters = (n_joints > 6) ? 200 : 40;
        constexpr double sor_omega = 1.0;
        for (int piter = 0; piter < max_pos_iters && n_joints > 0; ++piter) {
            const bool rev = (piter % 2) == 1;
            for (size_t step_i = 0; step_i < n_joints; ++step_i) {
                const size_t ji = rev ? (n_joints - 1 - step_i) : step_i;
                const auto& j = joints[ji];
                auto& ba = bodies[j.body_a];
                const double inv_ma = (ba.mass_kg > 0.0) ? (1.0 / ba.mass_kg) : 0.0;
                const Matrix3x3 inv_Ia = (inv_ma > 0.0) ? world_inv_inertia(ba) : Matrix3x3::zero();
                const Vector3d ra = ba.orientation.rotate(j.local_anchor_a);
                const Vector3d pa_w = ba.position + ra;

                Vector3d rb{0, 0, 0};
                Vector3d pb_w = j.local_anchor_b;
                double inv_mb = 0.0;
                Matrix3x3 inv_Ib = Matrix3x3::zero();
                if (!j.static_world_anchor) {
                    const auto& bb = bodies[j.body_b];
                    inv_mb = (bb.mass_kg > 0.0) ? (1.0 / bb.mass_kg) : 0.0;
                    inv_Ib = (inv_mb > 0.0) ? world_inv_inertia(bb) : Matrix3x3::zero();
                    rb = bb.orientation.rotate(j.local_anchor_b);
                    pb_w = bb.position + rb;
                }

                const Vector3d err = pa_w - pb_w;
                if (err.length_squared() < 1e-16) continue;
                const Matrix3x3 K = compute_point_k_matrix(inv_ma, inv_Ia, ra, inv_mb, inv_Ib, rb);
                const Vector3d dlambda = (K.inverse() * (-err)) * sor_omega;
                apply_pos_correction(ba, dlambda, ra, inv_ma, inv_Ia, 1.0);
                if (!j.static_world_anchor) {
                    apply_pos_correction(bodies[j.body_b], dlambda, rb, inv_mb, inv_Ib, -1.0);
                }
            }
            if ((piter % 4 == 3) && max_joint_position_error(bodies, joints) < 1e-6) {
                break;
            }
        }
    }
}

double ConstraintSolver::max_joint_position_error(
    const std::vector<RotationalBodyState>& bodies,
    const std::vector<ArticulatedJoint>& joints) noexcept {
    double max_err = 0.0;
    for (const auto& j : joints) {
        const auto& ba = bodies[j.body_a];
        const Vector3d pa_w = ba.position + ba.orientation.rotate(j.local_anchor_a);
        Vector3d pb_w = j.local_anchor_b;
        if (!j.static_world_anchor) {
            const auto& bb = bodies[j.body_b];
            pb_w = bb.position + bb.orientation.rotate(j.local_anchor_b);
        }
        max_err = std::max(max_err, (pa_w - pb_w).length());
    }
    return max_err;
}

} // namespace shunya::phy
