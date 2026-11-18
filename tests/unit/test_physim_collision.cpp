#include "tests/test_framework.hpp"

#include "shunya/physim/collision_pipeline.hpp"

#include <cmath>
#include <random>
#include <vector>

using namespace shunya::physim;
using AABB = shunya::core::domain::mechanics::AABB;

TEST_CASE("PH4-09.02.001: Dynamic AABB BVH and Spatial Hash Grid broad-phase collision detection") {
    // Construct 20 colliders along X axis: odd indices overlap, even indices separated
    std::vector<AABB> boxes;
    for (int i = 0; i < 20; ++i) {
        AABB b;
        const double x = (i / 2) * 3.0 + (i % 2) * 0.5; // (0, 0.5), (3, 3.5), (6, 6.5)...
        b.min_point = {x - 0.6, -0.5, -0.5};
        b.max_point = {x + 0.6,  0.5,  0.5};
        boxes.push_back(b);
    }

    // Dynamic BVH tree
    DynamicAabbBvh bvh;
    bvh.build(boxes);
    const auto bvh_pairs = bvh.query_overlapping_pairs();

    // 10 overlapping pairs: (0, 1), (2, 3), (4, 5), (6, 7), (8, 9), ...
    REQUIRE(bvh_pairs.size() == 10);
    for (size_t k = 0; k < 10; ++k) {
        CHECK(bvh_pairs[k].first == 2 * k);
        CHECK(bvh_pairs[k].second == 2 * k + 1);
    }

    // Spatial Hash Grid
    SpatialHashGrid grid(2.0); // 2 m cell size
    for (size_t i = 0; i < boxes.size(); ++i) {
        grid.insert(i, boxes[i]);
    }
    const auto grid_pairs = grid.query_overlapping_pairs();
    CHECK(grid_pairs.size() >= 10);
    for (size_t k = 0; k < 10; ++k) {
        const auto it = std::find(grid_pairs.begin(), grid_pairs.end(),
                                  std::make_pair(2 * k, 2 * k + 1));
        CHECK(it != grid_pairs.end());
    }
}

TEST_CASE("PH4-09.02.002: Gilbert-Johnson-Keerthi (GJK) and EPA narrow-phase for convex shapes") {
    // 1. Two separated spheres
    ColliderShape sphere_a;
    sphere_a.kind = ShapeKind::Sphere;
    sphere_a.radius = 0.5;

    ColliderShape sphere_b;
    sphere_b.kind = ShapeKind::Sphere;
    sphere_b.radius = 0.5;

    const Vector3d pos_a{0.0, 2.0, 0.0};
    const Vector3d pos_b{0.0, 0.0, 0.0};
    const Quaterniond id_q{1.0, 0.0, 0.0, 0.0};

    const auto sep_res = GjkEpaNarrowphase::detect(sphere_a, pos_a, id_q, sphere_b, pos_b, id_q);
    CHECK(!sep_res.intersecting);
    CHECK(std::abs(sep_res.distance - 1.0) < 0.02); // 2.0 - 0.5 - 0.5 = 1.0 m

    // 2. Overlapping sphere and box with EPA penetration
    ColliderShape box;
    box.kind = ShapeKind::Box;
    box.half_extents = {1.0, 1.0, 1.0};

    const Vector3d pos_box{0.0, 0.0, 0.0};
    const Vector3d pos_sph_hit{0.0, 1.3, 0.0}; // penetr = 1.0 + 0.5 - 1.3 = 0.2 m

    const auto hit_res = GjkEpaNarrowphase::detect(sphere_a, pos_sph_hit, id_q, box, pos_box, id_q);
    CHECK(hit_res.intersecting);
    CHECK(std::abs(hit_res.penetration_depth - 0.2) < 0.05);
    CHECK(hit_res.normal.y > 0.9); // Normal from box to sphere points +Y

    // 3. Rotated box vs Capsule
    ColliderShape capsule;
    capsule.kind = ShapeKind::Capsule;
    capsule.radius = 0.3;
    capsule.half_height = 0.5;

    const Quaterniond rot_45 = Quaterniond::from_axis_angle({0.0, 0.0, 1.0}, 0.785398);
    // At Y = 2.0: capsule bottom is at 2.0 - 0.5 * cos(45) - 0.3 = 1.3465 m, separated from box top at Y = 1.0
    const auto cap_res = GjkEpaNarrowphase::detect(capsule, {0.0, 2.0, 0.0}, rot_45, box, pos_box, id_q);
    CHECK(!cap_res.intersecting);
    CHECK(cap_res.distance > 0.0);
}

TEST_CASE("PH4-09.02.003: Continuous Collision Detection (CCD) conservative advancement preventing tunneling") {
    // 1 mm thin barrier plate at X = 0
    ColliderShape plate;
    plate.kind = ShapeKind::Box;
    plate.half_extents = {0.0005, 5.0, 5.0}; // 1 mm total thickness

    MotionTransformSweep static_plate;
    static_plate.start_position = {0.0, 0.0, 0.0};
    static_plate.end_position = {0.0, 0.0, 0.0};

    // Fast moving projectile (sphere radius 0.02 m) traveling at 500 m/s (from X = -5 to X = +5 in 1 step)
    ColliderShape bullet;
    bullet.kind = ShapeKind::Sphere;
    bullet.radius = 0.02;

    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> y_dist(-2.0, 2.0);
    std::uniform_real_distribution<double> z_dist(-2.0, 2.0);

    // Test 100 high-speed swept trajectories across the 1 mm barrier
    for (int i = 0; i < 100; ++i) {
        const double py = y_dist(rng);
        const double pz = z_dist(rng);

        MotionTransformSweep bullet_sweep;
        bullet_sweep.start_position = {-5.0, py, pz};
        bullet_sweep.end_position = {5.0, py, pz};
        bullet_sweep.linear_velocity = {500.0, 0.0, 0.0};

        const auto ccd = ContinuousCollisionDetector::compute_toi(
            bullet, bullet_sweep, plate, static_plate, 1e-3);

        REQUIRE(ccd.hit); // Zero tunneling permitted!
        CHECK(ccd.toi > 0.45);
        CHECK(ccd.toi < 0.55);
        // Contact normal points from plate to bullet (toward -X)
        CHECK(ccd.hit_normal.x < -0.9);
        CHECK(std::abs(ccd.hit_point.x) < 0.05);
    }
}

TEST_CASE("PH4-09.02.004: Contact manifold generation and temporal coherence caching") {
    RigidBodyState state_a;
    state_a.position = {0.0, 1.8, 0.0};
    ColliderShape box_a;
    box_a.kind = ShapeKind::Box;
    box_a.half_extents = {0.5, 0.5, 0.5};

    RigidBodyState state_b;
    state_b.position = {0.0, 1.0, 0.0};
    ColliderShape box_b;
    box_b.kind = ShapeKind::Box;
    box_b.half_extents = {1.0, 0.5, 1.0}; // table / platform

    // Generate contact manifold: box A resting on box B with 0.2 m penetration
    auto manifold = ContactManifoldGenerator::generate_manifold(
        "box_a", box_a, state_a, "box_b", box_b, state_b, 0.25, 0.6, 0.4);

    REQUIRE(!manifold.contacts.empty());
    CHECK(manifold.contacts.size() == 4); // 4-point patch generated for stable box contact!
    CHECK(manifold.restitution == 0.25);
    CHECK(manifold.static_friction == 0.6);
    CHECK(manifold.dynamic_friction == 0.4);

    for (const auto& cp : manifold.contacts) {
        CHECK(cp.normal.y > 0.95); // Upward normal from table to box
        CHECK(std::abs(cp.penetration_depth - 0.2) < 0.05);
        CHECK(cp.local_anchor_a.length() > 0.0);
        CHECK(cp.local_anchor_b.length() > 0.0);
    }

    // Test temporal coherence refresh across small positional shift
    state_a.position += Vector3d{0.005, 0.001, 0.0};
    ContactManifoldGenerator::update_manifold_coherence(manifold, state_a, state_b, 0.05);

    // All 4 points retained within 5 cm drift tolerance
    CHECK(manifold.contacts.size() == 4);
}
