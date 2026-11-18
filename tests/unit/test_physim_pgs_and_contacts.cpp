#include "tests/test_framework.hpp"

#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/physim/pgs_solver.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cmath>
#include <vector>

using namespace shunya::physim;

TEST_CASE("PH4-09.02.005: Projected Gauss-Seidel (PGS) impulse solver for contact restitution and Coulomb friction") {
    // Two dynamic bodies colliding head-on along X axis
    RigidBodyEntity body_a("body_a", BodyType::Dynamic);
    body_a.state().set_mass(2.0);
    body_a.state().position = {0.0, 0.0, 0.0};
    body_a.state().set_linear_velocity({4.0, 0.0, 0.0});

    RigidBodyEntity body_b("body_b", BodyType::Dynamic);
    body_b.state().set_mass(2.0);
    body_b.state().position = {0.8, 0.0, 0.0};
    body_b.state().set_linear_velocity({-4.0, 0.0, 0.0});

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(body_a);
    bodies.push_back(body_b);

    // Contact manifold with normal pointing from B to A (toward -X)
    PhysimContactManifold manifold;
    manifold.body_a_id = "body_a";
    manifold.body_b_id = "body_b";
    manifold.restitution = 0.5; // coefficient of restitution e = 0.5
    manifold.dynamic_friction = 0.3;

    PhysimContactPoint cp;
    cp.world_position = {0.4, 0.0, 0.0};
    cp.normal = {-1.0, 0.0, 0.0};
    cp.penetration_depth = 0.05;
    manifold.contacts.push_back(cp);

    std::vector<PhysimContactManifold> manifolds;
    manifolds.push_back(manifold);

    PgsSolverOptions opts;
    opts.iterations = 30;
    opts.penetration_slop = 0.001;

    PgsContactSolver::solve_contacts(bodies, manifolds, 0.01, opts);

    // Initial relative normal velocity: v_rel = va - vb = 4 - (-4) = 8 m/s
    // With e = 0.5, separation velocity after collision should be ~ e * 8 = 4 m/s (va ~ -2 m/s, vb ~ +2 m/s)
    const auto va = bodies[0].state().linear_velocity();
    const auto vb = bodies[1].state().linear_velocity();

    CHECK(va.x < 0.0);
    CHECK(vb.x > 0.0);
    const double sep_v = vb.x - va.x;
    CHECK(std::abs(sep_v - 4.0) < 0.2); // ~4.0 m/s separation velocity
    CHECK(manifolds[0].contacts[0].normal_impulse > 0.0);
}

TEST_CASE("PH4-09.02.006: Physical experiment validation: bouncing balls and incline sliding block") {
    // 1. Bouncing ball experiment: drop from 5.0 m, restitution 0.8
    // Theoretical rebound height: h1 = e^2 * h0 = 0.64 * 5.0 = 3.2 m (relative to contact)
    const auto ball_res = PgsContactSolver::simulate_bouncing_ball(5.0, 0.8, 0.1, 1.0, 0.001, 3000);
    CHECK(ball_res.first_rebound_height_m > 2.5);
    CHECK(ball_res.first_rebound_height_m < 3.5);
    CHECK(std::abs(ball_res.measured_restitution - 0.8) < 0.06);

    // 2. Sliding block on incline:
    // Case A: 20 deg incline with static friction 0.5 (tan(20 deg) ~ 0.364 < 0.5) -> must NOT slide
    const auto no_slide = PgsContactSolver::simulate_incline_block(20.0, 0.5, 0.4);
    CHECK(!no_slide.is_sliding);
    CHECK(no_slide.acceleration_mps2 == 0.0);
    CHECK(no_slide.final_velocity_mps == 0.0);
    CHECK(no_slide.final_distance_m == 0.0);

    // Case B: 45 deg incline with static friction 0.4, dynamic friction 0.3 (tan(45 deg) = 1.0 > 0.4) -> MUST slide!
    // Expected a = g * (sin(45) - 0.3 * cos(45)) = 9.80665 * 0.7071 * (1 - 0.3) = 4.854 m/s^2
    const auto slide = PgsContactSolver::simulate_incline_block(45.0, 0.4, 0.3, 2.0, 1.0, 0.002);
    CHECK(slide.is_sliding);
    CHECK(std::abs(slide.acceleration_mps2 - 4.854) < 0.05);
    CHECK(std::abs(slide.final_velocity_mps - 4.854) < 0.05); // v = a * t (t = 1s)
    CHECK(std::abs(slide.final_distance_m - (0.5 * 4.854)) < 0.05); // d = 0.5 * a * t^2
}
