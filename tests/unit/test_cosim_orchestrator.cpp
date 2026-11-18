#include "tests/test_framework.hpp"

#include "shunya/cosim/master_coordinator.hpp"

#include <thread>
#include <atomic>

using namespace shunya::cosim;
using namespace shunya::physim;

TEST_CASE("PH4-10.01.001: MasterCoSimulationCoordinator coordinating circuit solver and 3D physics solver") {
    CoSimConfig cfg;
    cfg.macro_time_step_s = 1e-3; // 1 ms physics
    cfg.micro_time_step_s = 1e-5; // 10 us circuit
    MasterCoSimulationCoordinator coordinator(cfg);

    CHECK(coordinator.current_time_s() == 0.0);
    CHECK(coordinator.current_macro_dt_s() == 1e-3);
    CHECK(coordinator.total_rollbacks() == 0);

    // Setup a dynamic body representing motor inertia
    RigidBodyEntity rotor("motor_rotor", BodyType::Dynamic);
    rotor.state().set_mass(0.5);
    rotor.state().position = {0.0, 0.0, 0.0};
    rotor.state().set_angular_velocity({0.0, 100.0, 0.0}); // 100 rad/s
    coordinator.physics_controller().add_body(rotor);

    // Initial telemetry write
    CoSimTelemetryPacket init_packet;
    init_packet.shaft_angular_velocity_rad_s = 100.0;
    coordinator.telemetry_pipe().write(init_packet);

    // Step the co-simulation coordinator
    const CoSimStepResult res = coordinator.step();
    CHECK(res.success);
    CHECK(!res.rollback_triggered);
    CHECK(std::abs(res.advanced_time_s - 1e-3) < 1e-9);
    CHECK(res.circuit_micro_steps == 100); // 1 ms / 10 us = 100 steps
    CHECK(coordinator.current_time_s() > 0.0);

    // Verify telemetry updated with electro-mechanical back-EMF
    CoSimTelemetryPacket out_telemetry;
    const bool read_ok = coordinator.telemetry_pipe().read(out_telemetry);
    CHECK(read_ok);
    CHECK(out_telemetry.motor_back_emf_v > 0.0);
    CHECK(out_telemetry.motor_armature_current_a > 0.0);
    CHECK(out_telemetry.motor_mechanical_torque_nm > 0.0);
}

TEST_CASE("PH4-10.01.002: Sub-cycling time synchronization between solvers") {
    CoSimConfig cfg;
    cfg.macro_time_step_s = 2e-3; // 2 ms
    cfg.micro_time_step_s = 2e-5; // 20 us -> ratio = 100
    MasterCoSimulationCoordinator coordinator(cfg);

    CHECK(coordinator.sub_cycle_ratio() == 100);

    // Dynamic configuration change
    cfg.micro_time_step_s = 1e-5; // 10 us -> ratio = 200
    coordinator.set_config(cfg);
    CHECK(coordinator.sub_cycle_ratio() == 200);

    // Verify sub-cycling runs exact number of micro steps
    const auto res = coordinator.step();
    CHECK(res.success);
    CHECK(res.circuit_micro_steps == 200);
}

TEST_CASE("PH4-10.01.003: State rollback and adaptive time-step negotiation on solver convergence failure") {
    CoSimConfig cfg;
    cfg.macro_time_step_s = 1e-3;
    cfg.micro_time_step_s = 1e-5;
    cfg.collision_impulse_rollback_threshold = 50.0; // threshold for rollback
    MasterCoSimulationCoordinator coordinator(cfg);

    // Add a normal dynamic body
    RigidBodyEntity projectile("bullet", BodyType::Dynamic);
    projectile.state().set_mass(1.0);
    projectile.state().position = {0.0, 0.0, 0.0};
    projectile.state().set_linear_velocity({10.0, 0.0, 0.0});
    coordinator.physics_controller().add_body(projectile);

    // Step 1: Smooth step succeeds
    auto res1 = coordinator.step();
    CHECK(res1.success);
    CHECK(!res1.rollback_triggered);
    CHECK(coordinator.total_rollbacks() == 0);
    const double valid_time = coordinator.current_time_s();

    // Now inject extreme velocity triggering divergence / impulse limit
    auto* b = coordinator.physics_controller().find_body("bullet");
    REQUIRE(b != nullptr);
    b->state().set_linear_velocity({200.0, 0.0, 0.0}); // Exceeds 50.0 m/s threshold

    // Step 2: Divergence detected -> rollback triggered and macro dt halved
    auto res2 = coordinator.step();
    CHECK(!res2.success);
    CHECK(res2.rollback_triggered);
    CHECK(coordinator.total_rollbacks() == 1);
    CHECK(coordinator.current_time_s() == valid_time);
    CHECK(coordinator.current_macro_dt_s() == 5e-4); // Halved from 1e-3

    // Direct negotiation test
    coordinator.negotiate_step(false); // Expand
    CHECK(coordinator.current_macro_dt_s() == 5e-4 * 1.25);
    coordinator.negotiate_step(true);  // Contract
    CHECK(coordinator.current_macro_dt_s() == (5e-4 * 1.25) * 0.5);
}

TEST_CASE("PH4-10.01.004: Zero-copy, lock-free telemetry exchange across threads") {
    LockFreeTelemetryBuffer<CoSimTelemetryPacket> buffer;

    // Basic write / read test
    CoSimTelemetryPacket p1;
    p1.timestamp_s = 1.234;
    p1.motor_armature_current_a = 4.56;
    buffer.write(p1);

    CoSimTelemetryPacket p_out;
    const bool read_success = buffer.read(p_out);
    CHECK(read_success);
    CHECK(p_out.timestamp_s == 1.234);
    CHECK(p_out.motor_armature_current_a == 4.56);

    // Second read without new write returns false but keeps latest
    CoSimTelemetryPacket p_out2;
    const bool second_read = buffer.read(p_out2);
    CHECK(!second_read);
    CHECK(p_out2.timestamp_s == 1.234);

    // Multi-threaded producer-consumer concurrency test
    std::atomic<bool> producer_done{false};
    constexpr int iterations = 10000;

    std::thread producer([&]() {
        for (int i = 0; i < iterations; ++i) {
            CoSimTelemetryPacket p;
            p.timestamp_s = static_cast<double>(i);
            p.motor_mechanical_torque_nm = static_cast<double>(i) * 0.1;
            buffer.write(p);
        }
        producer_done.store(true, std::memory_order_release);
    });

    int reads_received = 0;
    double last_timestamp = -1.0;
    while (!producer_done.load(std::memory_order_acquire)) {
        CoSimTelemetryPacket p;
        if (buffer.read(p)) {
            CHECK(p.timestamp_s >= last_timestamp); // Strictly monotonic or newer
            last_timestamp = p.timestamp_s;
            ++reads_received;
        }
    }

    producer.join();

    // Read final packet
    const CoSimTelemetryPacket final_p = buffer.get_latest();
    CHECK(final_p.timestamp_s == static_cast<double>(iterations - 1));
}
