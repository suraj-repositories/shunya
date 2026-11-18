#include "tests/test_framework.hpp"

#include "shunya/physim/simulation_controller.hpp"

using namespace shunya::physim;

TEST_CASE("PH4-09.04.001: Interactive simulation controls: Play, Pause, Stop, Step Forward, and Reset") {
    SimulationController sim;
    CHECK(sim.is_stopped());
    CHECK(!sim.is_running());
    CHECK(!sim.is_paused());
    CHECK(sim.current_time_s() == 0.0);
    CHECK(sim.total_steps() == 0);

    // Setup a dynamic test body
    RigidBodyEntity body("test_box", BodyType::Dynamic);
    body.state().set_mass(1.0);
    body.state().position = {0.0, 10.0, 0.0};
    body.state().set_linear_velocity({2.0, 0.0, 0.0});
    sim.add_body(body);

    bool state_changed = false;
    sim.set_on_state_change_callback([&](SimulationState, SimulationState) {
        state_changed = true;
    });

    uint64_t step_callbacks = 0;
    sim.set_on_step_callback([&](double, uint64_t) {
        ++step_callbacks;
    });

    // 1. Play starts simulation
    sim.play();
    CHECK(sim.is_running());
    CHECK(state_changed);

    // Step forward 5 steps of 0.02s
    for (int i = 0; i < 5; ++i) {
        sim.step_forward(0.02);
    }
    CHECK(std::abs(sim.current_time_s() - 0.1) < 1e-9);
    CHECK(sim.total_steps() == 5);
    CHECK(step_callbacks == 5);

    // Body position advanced and fell under gravity
    const auto* b = sim.find_body("test_box");
    REQUIRE(b != nullptr);
    CHECK(b->state().position.x > 0.1);
    CHECK(b->state().position.y < 10.0);

    // 2. Pause freezes time progression
    sim.pause();
    CHECK(sim.is_paused());
    const double paused_time = sim.current_time_s();
    const double paused_y = b->state().position.y;

    // Single step forward while paused
    sim.step_forward(0.01);
    CHECK(sim.current_time_s() > paused_time);
    CHECK(b->state().position.y < paused_y);

    // 3. Stop rewinds completely to initial state
    sim.stop();
    CHECK(sim.is_stopped());
    CHECK(sim.current_time_s() == 0.0);
    CHECK(sim.total_steps() == 0);

    const auto* b_reset = sim.find_body("test_box");
    REQUIRE(b_reset != nullptr);
    CHECK(std::abs(b_reset->state().position.x - 0.0) < 1e-12);
    CHECK(std::abs(b_reset->state().position.y - 10.0) < 1e-12);
    CHECK(std::abs(b_reset->state().linear_velocity().x - 2.0) < 1e-12);

    // 4. Test explicit reset_to_initial_state
    sim.play();
    sim.step_forward(0.05);
    CHECK(sim.current_time_s() > 0.0);
    sim.reset_to_initial_state();
    CHECK(sim.current_time_s() == 0.0);
    CHECK(b_reset->state().position.y == 10.0);
}

TEST_CASE("PH4-09.04.002: Simulation speed scaling (0.1x to 10.0x) and fixed vs adaptive time stepping") {
    SimulationController sim;
    
    // Default speed scale is 1.0
    CHECK(sim.speed_scale() == 1.0);
    
    // Test speed scaling bounds
    sim.set_speed_scale(2.5);
    CHECK(sim.speed_scale() == 2.5);
    
    sim.set_speed_scale(0.05); // Below min
    CHECK(sim.speed_scale() == 0.1);
    
    sim.set_speed_scale(20.0); // Above max
    CHECK(sim.speed_scale() == 10.0);
    
    // Verify scaled time advancement
    sim.set_speed_scale(2.0);
    sim.play();
    sim.step_forward(0.01);
    CHECK(std::abs(sim.current_time_s() - 0.02) < 1e-9);
    
    // Test adaptive time stepping mode
    sim.stop();
    auto cfg = sim.config();
    cfg.speed_scale = 1.0;
    cfg.stepping_mode = TimeSteppingMode::Adaptive;
    cfg.adaptive_cfl_factor = 0.05; // dt = cfl / speed
    cfg.min_adaptive_dt = 0.001;
    cfg.max_adaptive_dt = 0.02;
    sim.set_config(cfg);
    
    RigidBodyEntity fast_body("bullet", BodyType::Dynamic);
    fast_body.state().set_mass(1.0);
    fast_body.state().set_linear_velocity({10.0, 0.0, 0.0}); // speed = 10 -> adaptive dt = 0.05 / 10 = 0.005
    sim.add_body(fast_body);
    
    sim.play();
    sim.step_forward(0.01); // base dt 0.01, but adaptive calculates 0.005
    CHECK(std::abs(sim.current_time_s() - 0.005) < 1e-6);
}

TEST_CASE("PH4-09.04.003: Visual debug rendering (COM glyphs, velocity vectors, contact normals, joint axes)") {
    SimulationController sim;
    
    RigidBodyEntity body_a("box_a", BodyType::Dynamic);
    body_a.state().set_mass(2.0);
    body_a.state().position = {1.0, 2.0, 3.0};
    body_a.state().set_linear_velocity({0.0, 5.0, 0.0});
    sim.add_body(body_a);
    
    RigidBodyEntity body_b("box_b", BodyType::Static);
    body_b.state().position = {1.0, 0.0, 3.0};
    sim.add_body(body_b);
    
    RevoluteJointSpec hinge;
    hinge.body_a_id = "box_a";
    hinge.body_b_id = "box_b";
    hinge.local_anchor_a = {0.0, 0.0, 0.0};
    hinge.local_axis_a = {0.0, 0.0, 1.0};
    sim.add_revolute_joint(hinge);
    
    PhysimContactManifold manifold;
    manifold.body_a_id = "box_a";
    manifold.body_b_id = "box_b";
    PhysimContactPoint cp;
    cp.world_position = {1.0, 1.0, 3.0};
    cp.normal = {0.0, 1.0, 0.0};
    manifold.contacts.push_back(cp);
    sim.add_manifold(manifold);
    
    const DebugRenderData debug = sim.extract_debug_render_data();
    
    // Check COM glyphs
    REQUIRE(debug.com_glyphs.size() == 2);
    CHECK(debug.com_glyphs[0].position.x == 1.0);
    CHECK(debug.com_glyphs[0].position.y == 2.0);
    CHECK(debug.com_glyphs[0].label == "box_a_com");
    
    // Check velocity vectors (only body_a is moving above 0.01 threshold)
    REQUIRE(debug.velocity_vectors.size() == 1);
    CHECK(debug.velocity_vectors[0].origin.y == 2.0);
    CHECK(debug.velocity_vectors[0].vector.y > 0.0);
    
    // Check contact normals
    REQUIRE(debug.contact_normals.size() == 1);
    CHECK(debug.contact_normals[0].origin.x == 1.0);
    CHECK(debug.contact_normals[0].origin.y == 1.0);
    CHECK(debug.contact_normals[0].vector.y > 0.0);
    
    // Check joint axes
    REQUIRE(debug.joint_axes.size() == 1);
    CHECK(debug.joint_axes[0].vector.z > 0.0);
}

TEST_CASE("PH4-09.04.004: Interactive mouse-drag tool for dynamic perturbation during playback") {
    SimulationController sim;
    
    RigidBodyEntity body("drag_target", BodyType::Dynamic);
    body.state().set_mass(1.0);
    body.state().position = {0.0, 0.0, 0.0};
    body.state().set_linear_velocity({0.0, 0.0, 0.0});
    sim.add_body(body);
    
    // Grab the body at COM and pull towards (5.0, 0.0, 0.0)
    sim.grab_body("drag_target", {0.0, 0.0, 0.0}, {5.0, 0.0, 0.0}, 1000.0, 50.0);
    CHECK(sim.is_grabbing());
    
    sim.play();
    sim.step_forward(0.01);
    
    // Body should accelerate toward +X
    const auto* b = sim.find_body("drag_target");
    REQUIRE(b != nullptr);
    CHECK(b->state().linear_velocity().x > 0.0);
    CHECK(b->state().position.x > 0.0);
    
    // Update target further to the right
    sim.update_grab_target({10.0, 0.0, 0.0});
    sim.step_forward(0.01);
    CHECK(b->state().linear_velocity().x > 0.0);
    
    // Release grab
    sim.release_grab();
    CHECK(!sim.is_grabbing());
}

TEST_CASE("PH4-09.04.005: Dedicated physics configuration panel") {
    SimulationController sim;
    
    // Verify default parameters
    PhysicsConfig cfg = sim.config();
    CHECK(cfg.gravity.y < -9.0);
    CHECK(cfg.pgs_iterations == 30);
    CHECK(cfg.joint_iterations == 20);
    CHECK(cfg.enable_ccd == true);
    
    // Modify parameters through the configuration panel interface
    cfg.gravity = {0.0, -1.62, 0.0}; // Lunar gravity
    cfg.pgs_iterations = 15;
    cfg.penetration_slop = 0.002;
    cfg.baumgarte_beta = 0.35;
    cfg.enable_ccd = false;
    sim.set_config(cfg);
    
    const auto& updated_cfg = sim.config();
    CHECK(updated_cfg.gravity.y == -1.62);
    CHECK(updated_cfg.pgs_iterations == 15);
    CHECK(updated_cfg.penetration_slop == 0.002);
    CHECK(updated_cfg.baumgarte_beta == 0.35);
    CHECK(updated_cfg.enable_ccd == false);
    
    // Test behavior under lunar gravity
    RigidBodyEntity feather("feather", BodyType::Dynamic);
    feather.state().set_mass(0.1);
    feather.state().position = {0.0, 10.0, 0.0};
    sim.add_body(feather);
    
    sim.play();
    sim.step_forward(1.0); // 1 second under -1.62 m/s^2
    const auto* f = sim.find_body("feather");
    REQUIRE(f != nullptr);
    CHECK(std::abs(f->state().linear_velocity().y - (-1.62)) < 1e-4);
}

TEST_CASE("PH4-09.04.006: Complete simulation state snapshotting & rewinding") {
    SimulationController sim;
    
    RigidBodyEntity ball("ball", BodyType::Dynamic);
    ball.state().set_mass(1.0);
    ball.state().position = {0.0, 20.0, 0.0};
    sim.add_body(ball);
    
    sim.play();
    
    // Advance 10 steps
    for (int i = 0; i < 10; ++i) {
        sim.step_forward(0.01);
    }
    CHECK(sim.total_steps() == 10);
    CHECK(sim.history_size() == 10);
    CHECK(sim.can_rewind());
    
    // Save a named checkpoint at step 10
    sim.save_named_snapshot("checkpoint_10");
    const double pos_at_10 = sim.find_body("ball")->state().position.y;
    
    // Advance 10 more steps
    for (int i = 0; i < 10; ++i) {
        sim.step_forward(0.01);
    }
    CHECK(sim.total_steps() == 20);
    CHECK(sim.find_body("ball")->state().position.y < pos_at_10);
    
    // Rewind 5 steps
    sim.rewind_steps(5);
    CHECK(sim.total_steps() == 15);
    
    // Restore named snapshot back to step 10
    const bool restored = sim.restore_named_snapshot("checkpoint_10");
    CHECK(restored);
    CHECK(sim.total_steps() == 10);
    CHECK(std::abs(sim.find_body("ball")->state().position.y - pos_at_10) < 1e-12);
    
    // Restore nonexistent snapshot fails gracefully
    CHECK(!sim.restore_named_snapshot("invalid_name"));
}

TEST_CASE("PH4-09.04.007: Automated energy conservation audit in frictionless systems") {
    SimulationController sim;
    
    // Configure zero gravity for free kinetic energy test
    PhysicsConfig cfg = sim.config();
    cfg.gravity = {0.0, 0.0, 0.0};
    sim.set_config(cfg);
    
    RigidBodyEntity projectile("projectile", BodyType::Dynamic);
    projectile.state().set_mass(2.5);
    projectile.state().position = {0.0, 5.0, 0.0};
    projectile.state().set_linear_velocity({3.0, 4.0, 0.0}); // speed = 5 m/s, Ek = 0.5 * 2.5 * 25 = 31.25 J
    projectile.aero().enable_drag = false;
    projectile.aero().enable_buoyancy = false;
    sim.add_body(projectile);
    
    const double initial_ek = sim.total_kinetic_energy();
    const double initial_ep = sim.total_potential_energy();
    const double initial_etot = sim.total_mechanical_energy();
    CHECK(std::abs(initial_ek - 31.25) < 1e-9);
    CHECK(std::abs(initial_ep - 0.0) < 1e-9);
    CHECK(std::abs(initial_etot - 31.25) < 1e-9);
    
    sim.play();
    for (int i = 0; i < 50; ++i) {
        sim.step_forward(0.01);
    }
    
    // In frictionless, zero-gravity motion, kinetic energy is strictly conserved
    const double final_etot = sim.total_mechanical_energy();
    CHECK(std::abs(final_etot - initial_etot) < 1e-9);
    
    // Gravitational conservative potential energy test:
    SimulationController grav_sim;
    PhysicsConfig grav_cfg = grav_sim.config();
    grav_cfg.gravity = {0.0, -9.80665, 0.0};
    grav_sim.set_config(grav_cfg);
    
    RigidBodyEntity falling_rock("rock", BodyType::Dynamic);
    falling_rock.state().set_mass(1.0);
    falling_rock.state().position = {0.0, 50.0, 0.0}; // Ep = 1 * 9.80665 * 50 = 490.3325 J
    falling_rock.state().set_linear_velocity({0.0, 0.0, 0.0});
    falling_rock.aero().enable_drag = false;
    falling_rock.aero().enable_buoyancy = false;
    grav_sim.add_body(falling_rock);
    
    const double e0 = grav_sim.total_mechanical_energy();
    CHECK(std::abs(e0 - 490.3325) < 1e-6);
    
    grav_sim.play();
    for (int i = 0; i < 20; ++i) {
        grav_sim.step_forward(0.01);
        const double e_current = grav_sim.total_mechanical_energy();
        // Symplectic Euler has bounded energy oscillation of order O(dt)
        CHECK(std::abs(e_current - e0) / e0 < 0.02);
    }
}
