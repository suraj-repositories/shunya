#include "tests/test_framework.hpp"
#include "shunya/core/domain/mechanics.hpp"
#include <cmath>
#include <numbers>

using namespace shunya::core::domain::mechanics;
using namespace shunya::core::base::math;

// =============================================================================
// MEC-01: Rigid Body State & Mass Properties
// =============================================================================

TEST_CASE("MEC-01: Rigid Body State, Inertia Tensors, and Transformations") {
    auto body = std::make_shared<RigidBody>("cube1", BodyType::Dynamic);
    body->set_position({1.0, 2.0, 3.0});
    body->set_linear_velocity({0.5, -1.0, 0.0});

    CHECK(body->id() == "cube1");
    CHECK(body->type() == BodyType::Dynamic);
    CHECK(body->position().x == 1.0);
    CHECK(body->position().y == 2.0);
    CHECK(body->position().z == 3.0);
    CHECK(body->linear_velocity().y == -1.0);

    // Solid box mass properties (mass = 12 kg, half extents = 1, 1, 1)
    // I_xx = (1/3)*m*(b^2 + c^2) = (1/3)*12*(1+1) = 8.0
    auto box_mass = MassProperties::create_solid_box(12.0, {1.0, 1.0, 1.0});
    CHECK(box_mass.mass == 12.0);
    CHECK(box_mass.inv_mass == 1.0 / 12.0);
    CHECK(approx_equal(box_mass.local_inertia.at(0, 0), 8.0));
    CHECK(approx_equal(box_mass.local_inertia.at(1, 1), 8.0));
    CHECK(approx_equal(box_mass.local_inertia.at(2, 2), 8.0));

    body->set_mass_properties(box_mass);

    // Coordinate transformation: local to world
    Vector3d local_pt{1.0, 0.0, 0.0};
    Vector3d world_pt = body->local_to_world_point(local_pt);
    CHECK(approx_equal(world_pt.x, 2.0));
    CHECK(approx_equal(world_pt.y, 2.0));
    CHECK(approx_equal(world_pt.z, 3.0));

    // World to local roundtrip
    Vector3d back_to_local = body->world_to_local_point(world_pt);
    CHECK(approx_equal(back_to_local.x, 1.0));
    CHECK(approx_equal(back_to_local.y, 0.0));
    CHECK(approx_equal(back_to_local.z, 0.0));
}

// =============================================================================
// MEC-02: Forces, Torques & Impulses
// =============================================================================

TEST_CASE("MEC-02: Forces, Torques and Impulses Application") {
    auto body = std::make_shared<RigidBody>("obj2", BodyType::Dynamic);
    body->set_position({0.0, 0.0, 0.0});
    body->set_mass_properties(MassProperties::create_solid_sphere(2.0, 1.0));

    // Center force
    body->apply_force_center({10.0, 0.0, 0.0});
    CHECK(body->force_accumulator().x == 10.0);
    CHECK(body->torque_accumulator().x == 0.0);
    body->clear_accumulators();

    // Off-center force generating torque: r = (0, 1, 0), F = (10, 0, 0) -> torque = r x F = (0, 0, -10)
    body->apply_force_at_world_point({10.0, 0.0, 0.0}, {0.0, 1.0, 0.0});
    CHECK(body->force_accumulator().x == 10.0);
    CHECK(approx_equal(body->torque_accumulator().z, -10.0));
    body->clear_accumulators();

    // Off-center impulse: Delta v = J / m, Delta w = I_inv * (r x J)
    body->apply_impulse_at_world_point({4.0, 0.0, 0.0}, {0.0, 1.0, 0.0});
    CHECK(approx_equal(body->linear_velocity().x, 2.0)); // 4.0 / 2.0 = 2.0
    CHECK(body->angular_velocity().z < 0.0); // induced negative z spin
}

// =============================================================================
// MEC-03: Collision Geometry & AABB
// =============================================================================

TEST_CASE("MEC-03: Collision Shapes and AABB Intersection") {
    AABB box1{{-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0}};
    AABB box2{{0.5, 0.5, 0.5}, {2.5, 2.5, 2.5}};
    AABB box3{{5.0, 5.0, 5.0}, {6.0, 6.0, 6.0}};

    CHECK(box1.intersects(box2));
    CHECK_FALSE(box1.intersects(box3));

    CHECK(box1.contains({0.0, 0.0, 0.0}));
    CHECK_FALSE(box1.contains({2.0, 0.0, 0.0}));

    box1.expand({3.0, 0.0, 0.0});
    CHECK(box1.max_point.x == 3.0);
    CHECK(box1.contains({2.0, 0.0, 0.0}));

    // Sphere shape bounding volume
    auto sphere_body = std::make_shared<RigidBody>("sphere", BodyType::Dynamic);
    sphere_body->set_shape(SphereShape{2.0});
    sphere_body->set_position({10.0, 5.0, 0.0});
    AABB s_aabb = sphere_body->compute_world_aabb();
    CHECK(approx_equal(s_aabb.min_point.x, 8.0));
    CHECK(approx_equal(s_aabb.max_point.x, 12.0));
}

// =============================================================================
// MEC-04: Contact and Friction Models
// =============================================================================

TEST_CASE("MEC-04: Contact Manifolds and Friction Integration") {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, -10.0, 0.0};
    PhysicsWorld world(cfg);

    // Ground plane at y = 0
    auto ground = std::make_shared<RigidBody>("ground", BodyType::Static);
    ground->set_shape(PlaneShape{Vector3d{0.0, 1.0, 0.0}, 0.0});
    ground->set_friction(0.6);
    world.add_body(ground);

    // Sphere dropping onto ground plane
    auto ball = std::make_shared<RigidBody>("ball", BodyType::Dynamic);
    ball->set_shape(SphereShape{1.0});
    ball->set_position({0.0, 0.9, 0.0}); // slightly penetrating: radius 1.0 at y=0.9 penetrates by 0.1
    ball->set_mass_properties(MassProperties::create_solid_sphere(1.0, 1.0));
    ball->set_restitution(0.0);
    ball->set_friction(0.6);
    world.add_body(ball);

    // Step world to generate contact and solve
    world.step(0.01);
    CHECK(world.active_contacts().size() == 1u);
    CHECK(world.active_contacts()[0].contacts[0].penetration_depth > 0.0);
}

// =============================================================================
// MEC-05: Joints and Constraints
// =============================================================================

TEST_CASE("MEC-05: Spring-Damper and Rigid Constraints") {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, 0.0, 0.0};
    PhysicsWorld world(cfg);

    auto b1 = std::make_shared<RigidBody>("anchor", BodyType::Static);
    b1->set_position({0.0, 0.0, 0.0});
    world.add_body(b1);

    auto b2 = std::make_shared<RigidBody>("mass", BodyType::Dynamic);
    b2->set_position({2.0, 0.0, 0.0});
    b2->set_mass_properties(MassProperties::create_solid_box(1.0, {0.1, 0.1, 0.1}));
    world.add_body(b2);

    JointDefinition spring;
    spring.joint_id = "j1";
    spring.type = JointType::SpringDamper;
    spring.body_a_id = "anchor";
    spring.body_b_id = "mass";
    spring.rest_length = 1.0;
    spring.spring_stiffness = 50.0;
    spring.spring_damping = 1.0;
    world.add_joint(spring);

    CHECK(world.joints().size() == 1u);
    world.step(0.01);

    // Mass at x=2.0 with rest length 1.0 experiences restorative force towards origin
    CHECK(b2->linear_velocity().x < 0.0);
}

// =============================================================================
// MEC-06: Kinematics and Motion Constraints
// =============================================================================

TEST_CASE("MEC-06: Kinematic Bodies and Point Velocity") {
    auto k_body = std::make_shared<RigidBody>("kinematic_platform", BodyType::Kinematic);
    k_body->set_position({0.0, 0.0, 0.0});
    k_body->set_linear_velocity({2.0, 0.0, 0.0});
    k_body->set_angular_velocity({0.0, 0.0, 1.0}); // 1 rad/s around Z axis

    // Velocity at point (0, 1, 0): v_p = v + w x r = (2, 0, 0) + (0, 0, 1) x (0, 1, 0) = (2, 0, 0) + (-1, 0, 0) = (1, 0, 0)
    Vector3d p_vel = k_body->point_velocity({0.0, 1.0, 0.0});
    CHECK(approx_equal(p_vel.x, 1.0));
    CHECK(approx_equal(p_vel.y, 0.0));
}

// =============================================================================
// MEC-07: Energy and Momentum Diagnostics
// =============================================================================

TEST_CASE("MEC-07: Mechanical Energy and Momentum Diagnostics") {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, -9.8, 0.0};
    PhysicsWorld world(cfg);

    auto body = std::make_shared<RigidBody>("ball", BodyType::Dynamic);
    body->set_position({0.0, 10.0, 0.0});
    body->set_linear_velocity({3.0, 4.0, 0.0});
    body->set_mass_properties(MassProperties::create_solid_sphere(2.0, 0.5));
    world.add_body(body);

    auto diag = world.compute_diagnostics();

    // Linear momentum: p = m * v = 2 * (3, 4, 0) = (6, 8, 0)
    CHECK(approx_equal(diag.total_linear_momentum.x, 6.0));
    CHECK(approx_equal(diag.total_linear_momentum.y, 8.0));

    // Kinetic energy: (1/2)*m*v^2 = 0.5 * 2 * (9 + 16) = 25 J
    CHECK(approx_equal(diag.kinetic_energy_translational, 25.0));

    // Potential energy: m * g * h = 2 * 9.8 * 10 = 196 J
    CHECK(approx_equal(diag.potential_energy_gravity, 196.0));

    // Total mechanical energy = 25 + 196 = 221 J
    CHECK(approx_equal(diag.total_mechanical_energy, 221.0));
}

// =============================================================================
// MEC-08: Mechanics Visualisation and Telemetry
// =============================================================================

TEST_CASE("MEC-08: Contact Visualizers and Trajectory Telemetry") {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, -10.0, 0.0};
    PhysicsWorld world(cfg);

    auto body = std::make_shared<RigidBody>("falling_obj", BodyType::Dynamic);
    body->set_position({0.0, 10.0, 0.0});
    body->set_mass_properties(MassProperties::create_solid_sphere(1.0, 0.5));
    world.add_body(body);

    for (int i = 0; i < 5; ++i) {
        world.step(0.02);
    }

    const auto& history = world.body_telemetry("falling_obj");
    CHECK(history.size() == 5u);
    CHECK(history.back().timestamp > 0.09);
    CHECK(history.back().position.y < 10.0);
}

// =============================================================================
// MEC-09: Validation Benchmark Suite
// =============================================================================

TEST_CASE("MEC-09: Free Fall and Elastic Collision Benchmarks") {
    // 1. Analytical Free Fall Benchmark: y(t) = y_0 - 0.5*g*t^2, v(t) = -g*t
    auto free_fall_world = MechanicsBenchmarkFactory::create_free_fall_benchmark(100.0, 2.0);
    const double dt = 0.005;
    const double total_t = 1.0;
    const int steps = static_cast<int>(total_t / dt);

    for (int i = 0; i < steps; ++i) {
        free_fall_world.step(dt);
    }

    auto faller = free_fall_world.get_body("faller");
    REQUIRE(faller != nullptr);

    const double expected_v = -9.80665 * total_t;
    const double expected_y = 100.0 - 0.5 * 9.80665 * total_t * total_t;

    CHECK(approx_equal(faller->linear_velocity().y, expected_v, 0.05));
    CHECK(approx_equal(faller->position().y, expected_y, 0.1));

    // 2. Elastic Collision Benchmark: m1=1, v1=2, m2=1, v2=0 -> v1'=0, v2'=2
    auto collision_world = MechanicsBenchmarkFactory::create_elastic_collision_benchmark(1.0, 2.0, 1.0, 0.0);
    auto b1 = collision_world.get_body("ball1");
    auto b2 = collision_world.get_body("ball2");

    // Advance until they collide and separate
    for (int i = 0; i < 300; ++i) {
        collision_world.step(0.01);
    }

    // Total momentum should be conserved at ~2.0
    auto diag = collision_world.compute_diagnostics();
    CHECK(approx_equal(diag.total_linear_momentum.x, 2.0, 0.05));
}

// =============================================================================
// MEC-10: Sleeping Bodies and Deterministic Replay
// =============================================================================

TEST_CASE("MEC-10: Sleeping Bodies and Deterministic Replay") {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, 0.0, 0.0}; // zero gravity to test pure velocity sleeping
    cfg.enable_sleeping = true;
    cfg.sleep_linear_threshold = 0.1;
    cfg.sleep_steps_to_deactivate = 5;

    PhysicsWorld world(cfg);
    auto body = std::make_shared<RigidBody>("slow_body", BodyType::Dynamic);
    body->set_linear_velocity({0.01, 0.0, 0.0});
    body->set_angular_velocity({0.0, 0.0, 0.0});
    world.add_body(body);

    for (int i = 0; i < 10; ++i) {
        world.step(0.01);
    }

    CHECK(body->is_sleeping());

    // Wake up via impulse
    body->apply_impulse_center({5.0, 0.0, 0.0});
    CHECK_FALSE(body->is_sleeping());
    CHECK(body->linear_velocity().x > 1.0);

    // Deterministic replay verification: two distinct worlds with identical config & inputs produce identical states
    PhysicsWorldConfig replay_cfg;
    replay_cfg.gravity = {0.0, -9.80665, 0.0};
    replay_cfg.enable_sleeping = false;

    PhysicsWorld w1(replay_cfg);
    PhysicsWorld w2(replay_cfg);

    auto b1 = std::make_shared<RigidBody>("ball", BodyType::Dynamic);
    b1->set_position({1.0, 5.0, -2.0});
    b1->set_linear_velocity({2.5, 1.0, 0.0});
    w1.add_body(b1);

    auto b2 = std::make_shared<RigidBody>("ball", BodyType::Dynamic);
    b2->set_position({1.0, 5.0, -2.0});
    b2->set_linear_velocity({2.5, 1.0, 0.0});
    w2.add_body(b2);

    for (int i = 0; i < 50; ++i) {
        w1.step(0.01);
        w2.step(0.01);
    }

    CHECK(b1->position().x == b2->position().x);
    CHECK(b1->position().y == b2->position().y);
    CHECK(b1->position().z == b2->position().z);
    CHECK(b1->linear_velocity().y == b2->linear_velocity().y);
}
