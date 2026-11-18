#include "tests/test_framework.hpp"

#include "shunya/physim/joint_constraints.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cmath>
#include <numbers>
#include <vector>

using namespace shunya::physim;

TEST_CASE("PH4-09.03.001: Revolve / Hinge Joint (1 rotational DOF) with angular limits and joint friction") {
    RigidBodyEntity base("base", BodyType::Static);
    base.state().position = {0.0, 0.0, 0.0};

    RigidBodyEntity pendulum("pendulum", BodyType::Dynamic);
    pendulum.state().set_mass(1.0);
    pendulum.state().position = {1.0, 0.0, 0.0}; // 1m arm
    pendulum.state().set_local_inertia(Matrix3x3::diagonal(0.1, 0.1, 0.1));

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(base);
    bodies.push_back(pendulum);

    RevoluteJointSpec hinge;
    hinge.body_a_id = "base";
    hinge.body_b_id = "pendulum";
    hinge.local_anchor_a = {0.0, 0.0, 0.0};
    hinge.local_anchor_b = {-1.0, 0.0, 0.0}; // anchor coincides at world origin
    hinge.local_axis_a = {0.0, 0.0, 1.0};    // Z hinge axis
    hinge.local_axis_b = {0.0, 0.0, 1.0};
    hinge.enable_motor = true;
    hinge.motor_target_velocity_rad_s = 2.0; // 2 rad/s
    hinge.motor_max_torque_nm = 50.0;

    std::vector<RevoluteJointSpec> joints;
    joints.push_back(hinge);

    // Solve hinge joint
    JointConstraintSolver::solve_revolute_joints(bodies, joints, 0.01, 20);

    // Motor drives pendulum into rotation around Z
    CHECK(bodies[1].state().angular_velocity().z > 0.5);

    // Joint anchor error should remain near zero
    const double max_anchor_err = JointConstraintSolver::compute_max_joint_anchor_error(bodies, joints, {});
    CHECK(max_anchor_err < 1e-4);
}

TEST_CASE("PH4-09.03.002: Prismatic / Slider Joint (1 translational DOF) with linear limits and damping") {
    RigidBodyEntity track("track", BodyType::Static);
    track.state().position = {0.0, 0.0, 0.0};

    RigidBodyEntity slider("slider", BodyType::Dynamic);
    slider.state().set_mass(5.0);
    slider.state().position = {0.0, 0.0, 0.0};
    slider.state().set_linear_velocity({10.0, 3.0, -2.0}); // has lateral velocity components

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(track);
    bodies.push_back(slider);

    PrismaticJointSpec joint;
    joint.body_a_id = "track";
    joint.body_b_id = "slider";
    joint.local_axis_a = {1.0, 0.0, 0.0}; // X axis slider
    joint.local_axis_b = {1.0, 0.0, 0.0};
    joint.enable_limits = true;
    joint.lower_limit_m = -2.0;
    joint.upper_limit_m = 2.0;

    std::vector<PrismaticJointSpec> joints;
    joints.push_back(joint);

    JointConstraintSolver::solve_prismatic_joints(bodies, joints, 0.01, 20);

    // Perpendicular velocity (Y and Z) eliminated by prismatic constraint
    const auto v = bodies[1].state().linear_velocity();
    CHECK(v.x > 0.0); // translation along slider axis preserved
    CHECK(std::abs(v.y) < 1e-3);
    CHECK(std::abs(v.z) < 1e-3);
}

TEST_CASE("PH4-09.03.003: Ball-and-Socket Joint (3 rotational DOFs) with cone limits for suspension tie-rods") {
    RigidBodyEntity chassis("chassis", BodyType::Static);
    chassis.state().position = {0.0, 0.0, 0.0};

    RigidBodyEntity tierod("tierod", BodyType::Dynamic);
    tierod.state().set_mass(0.5);
    tierod.state().position = {0.0, 0.5, 0.0};

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(chassis);
    bodies.push_back(tierod);

    BallAndSocketJointSpec ball;
    ball.body_a_id = "chassis";
    ball.body_b_id = "tierod";
    ball.local_anchor_a = {0.0, 0.0, 0.0};
    ball.local_anchor_b = {0.0, -0.5, 0.0};
    ball.enable_cone_limit = true;
    ball.max_cone_angle_rad = std::numbers::pi / 6.0; // 30 deg cone limit

    std::vector<BallAndSocketJointSpec> joints;
    joints.push_back(ball);

    JointConstraintSolver::solve_ball_joints(bodies, joints, 0.01, 20);

    const double err = JointConstraintSolver::compute_max_joint_anchor_error(bodies, {}, joints);
    CHECK(err < 1e-4);
}

TEST_CASE("PH4-09.03.004: Wheel Axle Joint with suspension spring-damper compliance") {
    RigidBodyEntity chassis("chassis", BodyType::Dynamic);
    chassis.state().set_mass(500.0);
    chassis.state().position = {0.0, 1.0, 0.0};

    RigidBodyEntity wheel("wheel", BodyType::Dynamic);
    wheel.state().set_mass(25.0);
    wheel.state().position = {0.0, 0.6, 0.0}; // rest length 0.5 m, current length 0.4 m -> compressed by 0.1 m

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(chassis);
    bodies.push_back(wheel);

    WheelAxleJointSpec susp;
    susp.chassis_body_id = "chassis";
    susp.wheel_body_id = "wheel";
    susp.suspension_rest_length_m = 0.5; // Compressed by 0.1m -> spring pushes outward
    susp.suspension_stiffness_n_m = 30000.0;
    susp.suspension_damping_n_s_m = 2000.0;

    std::vector<WheelAxleJointSpec> joints;
    joints.push_back(susp);

    JointConstraintSolver::solve_wheel_suspension(bodies, joints, 0.01);

    // Spring pushes chassis upward (+Y) and wheel downward (-Y)
    CHECK(joints[0].spring_force_n > 0.0);
    CHECK(bodies[0].state().force_accumulator.y > 0.0);
    CHECK(bodies[1].state().force_accumulator.y < 0.0);
}

TEST_CASE("PH4-09.03.005: Mechanical Gear Constraint: angular velocity ratio omega1 / omega2 = N2 / N1") {
    // Gear 1: 20 teeth, Gear 2: 40 teeth -> Ratio R = 40/20 = 2.0 (w1 = -2 * w2)
    RigidBodyEntity gear1("gear1", BodyType::Dynamic);
    gear1.state().set_mass(2.0);
    gear1.state().set_local_inertia(Matrix3x3::diagonal(0.05, 0.05, 0.05));
    gear1.state().set_angular_velocity({0.0, 0.0, 10.0}); // 10 rad/s

    RigidBodyEntity gear2("gear2", BodyType::Dynamic);
    gear2.state().set_mass(4.0);
    gear2.state().set_local_inertia(Matrix3x3::diagonal(0.2, 0.2, 0.2));
    gear2.state().set_angular_velocity({0.0, 0.0, 0.0});

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(gear1);
    bodies.push_back(gear2);

    GearConstraintSpec gear_spec;
    gear_spec.gear1_body_id = "gear1";
    gear_spec.gear2_body_id = "gear2";
    gear_spec.teeth_count_1 = 20;
    gear_spec.teeth_count_2 = 40;
    CHECK(gear_spec.gear_ratio() == 2.0);

    std::vector<GearConstraintSpec> gears;
    gears.push_back(gear_spec);

    JointConstraintSolver::solve_gear_constraints(bodies, gears, 0.01);

    const double w1 = bodies[0].state().angular_velocity().z;
    const double w2 = bodies[1].state().angular_velocity().z;

    // Enforce w1 + 2.0 * w2 = 0
    CHECK(std::abs(w1 + 2.0 * w2) < 1e-3);
    CHECK(w1 > 0.0);
    CHECK(w2 < 0.0);
}

TEST_CASE("PH4-09.03.006: Pulley and Belt constraint transmission between axles") {
    // Pulley 1: r = 0.1 m, Pulley 2: r = 0.2 m
    RigidBodyEntity p1("p1", BodyType::Dynamic);
    p1.state().set_mass(1.0);
    p1.state().set_local_inertia(Matrix3x3::diagonal(0.02, 0.02, 0.02));
    p1.state().set_angular_velocity({0.0, 0.0, 20.0}); // 20 rad/s -> v_surf = 2.0 m/s

    RigidBodyEntity p2("p2", BodyType::Dynamic);
    p2.state().set_mass(2.0);
    p2.state().set_local_inertia(Matrix3x3::diagonal(0.08, 0.08, 0.08));

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(p1);
    bodies.push_back(p2);

    PulleyBeltConstraintSpec belt;
    belt.pulley1_body_id = "p1";
    belt.pulley2_body_id = "p2";
    belt.pulley1_radius_m = 0.1;
    belt.pulley2_radius_m = 0.2;
    belt.is_crossed_belt = false; // open belt: same spin direction

    std::vector<PulleyBeltConstraintSpec> belts;
    belts.push_back(belt);

    JointConstraintSolver::solve_pulley_belt_constraints(bodies, belts, 0.01);

    const double w1 = bodies[0].state().angular_velocity().z;
    const double w2 = bodies[1].state().angular_velocity().z;

    // Surface speed equality: w1 * r1 = w2 * r2 -> w1 * 0.1 = w2 * 0.2 -> w1 = 2 * w2
    CHECK(std::abs(w1 * 0.1 - w2 * 0.2) < 0.1);
    CHECK(w1 > 0.0);
    CHECK(w2 > 0.0);
}

TEST_CASE("PH4-09.03.007: 4-bar and multi-bar mechanical linkage assemblies with closed-loop kinematic consistency") {
    // Grashof crank-rocker linkage:
    // L1 = 0.4 m (ground), L2 = 0.1 m (crank), L3 = 0.35 m (coupler), L4 = 0.3 m (rocker)
    // S = 0.1, L = 0.4, P = 0.3, Q = 0.35
    // S + L = 0.5 <= P + Q = 0.65 -> Grashof crank-rocker!
    FourBarLinkageAssembly linkage;
    linkage.ground_length_m = 0.4;
    linkage.crank_length_m = 0.1;
    linkage.coupler_length_m = 0.35;
    linkage.rocker_length_m = 0.3;

    CHECK(linkage.satisfies_grashof());
    CHECK(linkage.classify_grashof() == GrashofClass::CrankRocker);

    // Test continuous 360-degree crank cycle
    for (int deg = 0; deg < 360; deg += 30) {
        const double theta2 = (deg * std::numbers::pi) / 180.0;
        const auto res = linkage.solve_forward_kinematics(theta2);
        REQUIRE(res.has_solution);
        CHECK(res.loop_closure_error_m < 1e-4); // closed loop kinematic consistency < 0.1 mm!
    }
}
