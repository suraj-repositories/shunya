#include "tests/test_framework.hpp"

#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/physim/integrator.hpp"
#include "shunya/physim/joint_constraints.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cmath>
#include <numbers>
#include <vector>

using namespace shunya::physim;
using namespace shunya::core::domain;

// =============================================================================
// PH4-13.01.004: Physics Dynamics Verification
// =============================================================================

TEST_CASE("PH4-13.01.004: Rigid Body Kinematics & Symplectic Integration") {
    RigidBodyEntity body("test_box", BodyType::Dynamic);
    body.state().set_mass(2.0); // 2 kg
    body.state().position = {0.0, 10.0, 0.0};
    body.state().set_local_inertia(Matrix3x3::diagonal(0.2, 0.2, 0.2));

    // Initial state
    CHECK(body.state().mass_kg == 2.0);
    CHECK(body.state().inv_mass == 0.5);

    // Apply gravity force and free fall for 1.0 second (100 steps dt = 0.01)
    const Vector3d gravity{0.0, -9.81, 0.0};
    const double dt = 0.01;
    for (int step = 0; step < 100; ++step) {
        body.state().apply_force_at_com(gravity * body.state().mass_kg);
        RigidBodyIntegrator::step_symplectic_euler(body.state(), dt);
    }

    // Kinematic expectations:
    // v_y = -9.81 * 1.0 = -9.81 m/s
    // y = 10.0 + 0.5 * (-9.81) * 1.0^2 = ~5.095 m
    CHECK(std::abs(body.state().linear_velocity().y - (-9.81)) < 0.15);
    CHECK(body.state().position.y < 5.2);
    CHECK(body.state().position.y > 4.9);

    // Kinetic energy: 0.5 * m * v^2 = 0.5 * 2.0 * 9.81^2 = ~96.2 J
    const auto vel = body.state().linear_velocity();
    const double v_sq = vel.dot(vel);
    const double ke = 0.5 * body.state().mass_kg * v_sq;
    CHECK(ke > 90.0);
    CHECK(ke < 102.0);
}

TEST_CASE("PH4-13.01.004: GJK / EPA Contact Resolution & Penetration Recovery") {
    // Two overlapping spheres
    ColliderShape sphere_a;
    sphere_a.kind = ShapeKind::Sphere;
    sphere_a.radius = 0.5;

    ColliderShape box;
    box.kind = ShapeKind::Box;
    box.half_extents = {1.0, 1.0, 1.0};

    const Vector3d pos_box{0.0, 0.0, 0.0};
    const Vector3d pos_sph_hit{0.0, 1.3, 0.0}; // penetr = 1.0 + 0.5 - 1.3 = 0.2 m
    const Quaterniond id_q{1.0, 0.0, 0.0, 0.0};

    const auto hit_res = GjkEpaNarrowphase::detect(sphere_a, pos_sph_hit, id_q, box, pos_box, id_q);
    CHECK(hit_res.intersecting);
    CHECK(std::abs(hit_res.penetration_depth - 0.2) < 0.02);
    CHECK(std::abs(hit_res.normal.y - 1.0) < 0.05);
}

TEST_CASE("PH4-13.01.004: Multi-Body Revolute & Prismatic Joint Constraints") {
    // Base static body and driven arm
    RigidBodyEntity base("ground_mount", BodyType::Static);
    base.state().position = {0.0, 0.0, 0.0};

    RigidBodyEntity link("arm_link", BodyType::Dynamic);
    link.state().set_mass(1.0);
    link.state().position = {0.5, 0.0, 0.0};
    link.state().set_local_inertia(Matrix3x3::diagonal(0.05, 0.05, 0.05));

    std::vector<RigidBodyEntity> bodies = {base, link};

    RevoluteJointSpec revolute;
    revolute.body_a_id = "ground_mount";
    revolute.body_b_id = "arm_link";
    revolute.local_anchor_a = {0.0, 0.0, 0.0};
    revolute.local_anchor_b = {-0.5, 0.0, 0.0};
    revolute.local_axis_a = {0.0, 0.0, 1.0};
    revolute.local_axis_b = {0.0, 0.0, 1.0};
    revolute.enable_motor = true;
    revolute.motor_target_velocity_rad_s = 5.0; // 5 rad/s around Z
    revolute.motor_max_torque_nm = 20.0;

    std::vector<RevoluteJointSpec> joints = {revolute};

    // Step joint constraint solver
    for (int i = 0; i < 10; ++i) {
        JointConstraintSolver::solve_revolute_joints(bodies, joints, 0.01, 15);
    }

    // Arm angular velocity around Z should be accelerated towards target
    CHECK(bodies[1].state().angular_velocity().z > 1.0);

    // Anchor error must stay sub-millimeter
    const double anchor_err = JointConstraintSolver::compute_max_joint_anchor_error(bodies, joints, {});
    CHECK(anchor_err < 1e-4);
}
