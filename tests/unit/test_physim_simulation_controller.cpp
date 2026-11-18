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
