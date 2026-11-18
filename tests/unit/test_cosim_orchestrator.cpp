#include "tests/test_framework.hpp"

#include "shunya/cosim/master_coordinator.hpp"
#include "shunya/cosim/transducers.hpp"
#include "shunya/cosim/sensor_coupling.hpp"
#include "shunya/cosim/demonstrator_machines.hpp"

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

TEST_CASE("PH4-10.01.005: Support multiple independent electronic circuits mapped to distinct sub-assemblies") {
    MasterCoSimulationCoordinator coordinator;

    CHECK(coordinator.circuit_count() == 0);

    // Register independent circuits for separate mechatronic sub-assemblies
    coordinator.add_circuit("drive_motor_circuit", "rear_axle_assembly");
    coordinator.add_circuit("steering_servo_circuit", "front_steering_assembly");
    coordinator.add_circuit("lighting_circuit", "chassis_headlamps");

    CHECK(coordinator.circuit_count() == 3);
    CHECK(coordinator.has_circuit("drive_motor_circuit"));
    CHECK(coordinator.has_circuit("steering_servo_circuit"));
    CHECK(coordinator.has_circuit("lighting_circuit"));
    CHECK(!coordinator.has_circuit("nonexistent_circuit"));

    const auto ids = coordinator.circuit_ids();
    CHECK(ids.size() == 3);

    auto* drive_eng = coordinator.find_circuit("drive_motor_circuit");
    REQUIRE(drive_eng != nullptr);
    CHECK(coordinator.find_circuit("invalid_circuit") == nullptr);
}

TEST_CASE("PH4-10.01.006: Automated latency compensation and linear state interpolation between staggered solver intervals") {
    RigidBodyState s0;
    s0.position = {0.0, 0.0, 0.0};
    s0.set_mass(1.0);
    s0.set_linear_velocity({10.0, 0.0, 0.0});
    s0.set_angular_velocity({0.0, 20.0, 0.0});
    s0.orientation = Quaterniond::identity();

    RigidBodyState s1;
    s1.position = {1.0, 0.0, 0.0};
    s1.set_mass(1.0);
    s1.set_linear_velocity({12.0, 0.0, 0.0});
    s1.set_angular_velocity({0.0, 22.0, 0.0});
    s1.orientation = Quaterniond::from_axis_angle({0.0, 1.0, 0.0}, 0.5);

    // Test linear state interpolation at intermediate alpha = 0.5
    const auto inter = StateInterpolator::interpolate(s0, s1, 0.5);
    CHECK(std::abs(inter.position.x - 0.5) < 1e-9);
    CHECK(std::abs(inter.linear_velocity.x - 11.0) < 1e-9);
    CHECK(std::abs(inter.angular_velocity.y - 21.0) < 1e-9);

    // Alpha at boundary 0 and 1
    const auto inter0 = StateInterpolator::interpolate(s0, s1, 0.0);
    CHECK(std::abs(inter0.position.x - 0.0) < 1e-9);
    const auto inter1 = StateInterpolator::interpolate(s0, s1, 1.0);
    CHECK(std::abs(inter1.position.x - 1.0) < 1e-9);

    // Test latency compensation prediction: x_pred = x0 + v * latency
    constexpr double latency_s = 0.02; // 20 ms solver delay
    const auto comp = StateInterpolator::compensate_latency(s0, latency_s);
    CHECK(std::abs(comp.position.x - 0.2) < 1e-9); // 10 m/s * 0.02s = 0.2m
    CHECK(comp.orientation.w != 0.0);
}

TEST_CASE("PH4-10.02.001: DC Motor Transducer: convert armature current into mechanical torque") {
    DCMotorSpec spec;
    spec.Kt = 0.08;              // 0.08 N*m / A
    spec.viscous_damping = 1e-3;
    spec.coulomb_friction = 0.005;
    DCMotorTransducer motor(spec);

    // At zero speed, torque is purely electromagnetic
    const double tau_static = motor.compute_torque(2.5, 0.0);
    CHECK(std::abs(tau_static - (0.08 * 2.5)) < 1e-9); // 0.20 N*m

    // Under rotation, damping and friction oppose motion
    const double tau_spinning = motor.compute_torque(2.5, 50.0);
    // tau = 0.20 - 50*1e-3 - 0.005 = 0.20 - 0.05 - 0.005 = 0.145 N*m
    CHECK(std::abs(tau_spinning - 0.145) < 1e-9);

    // Apply torque directly to rigid body axle
    RigidBodyEntity flywheel("flywheel", BodyType::Dynamic);
    flywheel.state().set_mass(2.0);
    motor.apply_torque_to_body(flywheel, {0.0, 1.0, 0.0}, 2.5);
    CHECK(flywheel.state().torque_accumulator.y > 0.1);
}

TEST_CASE("PH4-10.02.002: Motor Back-EMF Feedback: convert angular velocity into counter-EMF") {
    constexpr double Ke = 0.08;      // 0.08 V / (rad/s)
    constexpr double Ra = 1.2;       // 1.2 Ohms
    constexpr double V_supply = 24.0;// 24 V DC

    // At standstill (omega = 0), back-EMF is 0 and current is stall current
    const double bemf_0 = MotorBackEmfCoupling::compute_back_emf(0.0, Ke);
    CHECK(bemf_0 == 0.0);
    const double i_stall = MotorBackEmfCoupling::compute_armature_current(V_supply, 0.0, Ra, Ke);
    CHECK(std::abs(i_stall - 20.0) < 1e-9); // 24 / 1.2 = 20 A

    // At 150 rad/s: V_bemf = 0.08 * 150 = 12.0 V, Current = (24 - 12) / 1.2 = 10.0 A
    const double bemf_150 = MotorBackEmfCoupling::compute_back_emf(150.0, Ke);
    CHECK(std::abs(bemf_150 - 12.0) < 1e-9);
    const double i_150 = MotorBackEmfCoupling::compute_armature_current(V_supply, 150.0, Ra, Ke);
    CHECK(std::abs(i_150 - 10.0) < 1e-9);

    // No-load speed where back-EMF cancels supply voltage
    const double omega_max = MotorBackEmfCoupling::no_load_speed(V_supply, Ke);
    CHECK(std::abs(omega_max - 300.0) < 1e-9); // 24 / 0.08 = 300 rad/s
    const double i_noload = MotorBackEmfCoupling::compute_armature_current(V_supply, omega_max, Ra, Ke);
    CHECK(std::abs(i_noload - 0.0) < 1e-9);
}

TEST_CASE("PH4-10.02.003: Solenoid & Linear Actuator Transducer: reluctance force on slider") {
    SolenoidSpec spec;
    spec.turns = 800;
    spec.pole_area_m2 = 4e-4; // 4 cm^2
    spec.residual_gap_m = 1e-3; // 1 mm residual air gap
    SolenoidActuatorTransducer solenoid(spec);

    // Air gap = 2 mm (effective gap = 3 mm)
    // Force F = (mu0 * N^2 * A / (2 * (x + x0)^2)) * I^2
    const double f_2mm = solenoid.compute_force(2.0, 2e-3);
    CHECK(f_2mm > 5.0); // Significant magnetic pull > 5 N

    // When air gap shrinks to 0.5 mm, reluctance drops and force increases inversely with (x+x0)^2
    const double f_closed = solenoid.compute_force(2.0, 0.5e-3);
    CHECK(f_closed > f_2mm * 3.0); // Much higher holding force

    // Variable inductance increases as air gap closes
    const double l_open = solenoid.compute_inductance(5e-3);
    const double l_closed = solenoid.compute_inductance(0.5e-3);
    CHECK(l_closed > l_open);

    // Apply to rigid body slider
    RigidBodyEntity plunger("plunger", BodyType::Dynamic);
    plunger.state().set_mass(0.2);
    solenoid.apply_force_to_body(plunger, {0.0, 0.0, -1.0}, 2.0, 2e-3);
    CHECK(plunger.state().force_accumulator.z < -5.0);
}

TEST_CASE("PH4-10.02.004: Piezoelectric Transducer: voltage-to-displacement and acoustic SPL") {
    PiezoelectricSpec spec;
    spec.d33_c_per_n = 400e-12; // 400 pm/V
    spec.stiffness_n_per_m = 2e7; // 20 kN/mm
    PiezoelectricTransducer piezo(spec);

    // 150 V pulse creates micro-scale mechanical displacement
    const double disp = piezo.compute_displacement(150.0);
    // 400e-12 * 150 = 60e-9 m = 60 nm
    CHECK(std::abs(disp - 6e-8) < 1e-12);

    // Blocking force: F = k * delta_x = 2e7 * 60e-9 = 1.2 N
    const double force = piezo.compute_blocking_force(150.0);
    CHECK(std::abs(force - 1.2) < 1e-9);

    // Acoustic sound alert generation (e.g. 2.4 kHz piezo buzzer at 0.1 m distance)
    const double spl_db = piezo.compute_acoustic_spl_db(150.0, 2400.0, 0.1);
    CHECK(spl_db > 60.0); // Audible alert > 60 dB SPL
    CHECK(spl_db < 120.0); // Safe audible range
}

TEST_CASE("PH4-10.02.005: Thermal Joule Heating Feedback: I^2 * R heating and linear expansion") {
    JouleHeatingSpec spec;
    spec.resistance_ohms = 2.0;          // 2 Ohms
    spec.thermal_capacitance_j_k = 5.0;  // 5 J/K
    spec.thermal_resistance_k_w = 10.0;  // 10 K/W
    spec.ambient_temperature_k = 298.15; // 25 C
    spec.thermal_expansion_coeff = 16.5e-6;// Copper (16.5 ppm/K)
    ThermalJouleHeatingCoupling thermal(spec);

    CHECK(std::abs(thermal.temperature_k() - 298.15) < 1e-9);

    // Apply 3 A current for 1.0 second: P = 3^2 * 2 = 18 W
    // Over 1 second, dT ~ P / C_th * dt = 18 / 5 = 3.6 K
    for (int i = 0; i < 100; ++i) {
        thermal.step_thermal(3.0, 0.01);
    }
    CHECK(thermal.temperature_k() > 298.15 + 3.0);

    // Thermal expansion of a 1.0 m busbar
    const double exp_m = thermal.compute_thermal_expansion(1.0);
    CHECK(exp_m > 0.0);
    CHECK(exp_m < 1e-3); // Sub-millimeter expansion for modest temp rise

    const double scale = thermal.compute_scale_factor();
    CHECK(scale > 1.0);
}

TEST_CASE("PH4-10.02.006: Automated energy balance validation (E_elec = W_mech + Q_therm)") {
    EnergyBalanceAuditor auditor;
    
    // Simulate motor driving load:
    // Voltage = 24 V, Current = 2 A -> P_in = 48 W
    // Torque = 0.18 N*m, Omega = 200 rad/s -> P_mech = 36 W
    // Resistance = 3.0 Ohms -> P_loss = 2^2 * 3.0 = 12 W
    // P_in (48 W) == P_mech (36 W) + P_loss (12 W) -> Strictly balanced!
    for (int i = 0; i < 50; ++i) {
        auditor.accumulate_step(
            24.0,   // V
            2.0,    // A
            0.18,   // Torque (N*m)
            200.0,  // Omega (rad/s)
            0.0,    // Force (N)
            0.0,    // Vel (m/s)
            3.0,    // R (Ohms)
            0.01    // dt (s)
        );
    }

    CHECK(std::abs(auditor.electrical_energy_j() - 24.0) < 1e-9); // 48 W * 0.5s = 24 J
    CHECK(std::abs(auditor.mechanical_work_j() - 18.0) < 1e-9);  // 36 W * 0.5s = 18 J
    CHECK(std::abs(auditor.thermal_losses_j() - 6.0) < 1e-9);    // 12 W * 0.5s = 6 J
    CHECK(std::abs(auditor.energy_residual_j()) < 1e-9);
    CHECK(auditor.is_energy_conserved(1e-6));
}

TEST_CASE("PH4-10.03.001: 3D Limit Switch Sensor: mechanical contact triggering dynamic resistance") {
    LimitSwitchSpec spec;
    spec.target_body_a = "sliding_carriage";
    spec.target_body_b = "end_stop";
    spec.trigger_distance_m = 0.005; // 5 mm
    spec.r_open_ohms = 1e8;
    spec.r_closed_ohms = 1e-3;
    spec.normally_closed = false; // Normally open
    LimitSwitchSensor sensor(spec);

    // Initial state: not triggered -> Open -> 100 MOhm
    CHECK(!sensor.is_closed());
    CHECK(sensor.dynamic_resistance_ohms() == 1e8);

    // Carriage at 10 mm distance -> still open
    sensor.update_from_distance(0.010);
    CHECK(!sensor.is_closed());
    CHECK(sensor.dynamic_resistance_ohms() == 1e8);

    // Carriage reaches 3 mm distance -> Triggered -> Closed -> 1 mOhm
    sensor.update_from_distance(0.003);
    CHECK(sensor.is_closed());
    CHECK(sensor.dynamic_resistance_ohms() == 1e-3);

    // Test contact manifold trigger
    shunya::physim::PhysimContactManifold manifold;
    manifold.body_a_id = "sliding_carriage";
    manifold.body_b_id = "end_stop";
    manifold.contacts.push_back({}); // 1 contact point
    sensor.update_from_contacts({manifold});
    CHECK(sensor.is_closed());
    CHECK(sensor.dynamic_resistance_ohms() == 1e-3);
}

TEST_CASE("PH4-10.03.002: Optical / LDR Sensor Coupling: 3D illuminance and dynamic photo-resistance") {
    LdrSpec spec;
    spec.r0_ohms = 1000.0;
    spec.e0_lux = 10.0;
    spec.gamma = 0.7;
    spec.ambient_lux = 5.0;
    OpticalLdrSensorCoupling ldr(spec);

    const shunya::physim::Vector3d sensor_pos{0.0, 0.0, 0.0};
    const shunya::physim::Vector3d sensor_normal{0.0, 1.0, 0.0}; // Pointing UP (+Y)
    const shunya::physim::Vector3d light_pos{0.0, 2.0, 0.0};     // Directly overhead 2m
    constexpr double light_lumens = 800.0;              // Standard 60W equivalent bulb

    // Direct illumination overhead: r = 2, r^2 = 4, cos(theta) = 1.0
    // E_direct = 800 / (4 * pi * 4) = 800 / (16 * pi) ~ 15.91 lux
    // Total lux = 5.0 + 15.91 ~ 20.91 lux
    const double direct_lux = ldr.compute_illuminance(sensor_pos, sensor_normal, light_pos, light_lumens, false);
    CHECK(direct_lux > 20.0);
    CHECK(direct_lux < 22.0);

    // Resistance in bright light should drop significantly below 1000 Ohms
    const double r_bright = ldr.compute_resistance(direct_lux);
    CHECK(r_bright < 1000.0);
    CHECK(r_bright > 100.0);

    // When an occluding body casts a shadow on the sensor
    const double shadow_lux = ldr.compute_illuminance(sensor_pos, sensor_normal, light_pos, light_lumens, true);
    CHECK(shadow_lux == 5.0); // Ambient only
    const double r_shadow = ldr.compute_resistance(shadow_lux);
    CHECK(r_shadow > r_bright); // Resistance increases in shadow

    // Pure darkness (0 lux)
    const double r_dark = ldr.compute_resistance(0.0);
    CHECK(r_dark == spec.dark_resistance_ohms); // 1 MOhm
}

TEST_CASE("PH4-10.03.003: Temperature Sensor / Thermistor Coupling: local temperature drives resistance") {
    ThermistorSpec spec;
    spec.r0_ohms = 10000.0; // 10 kOhms at 298.15 K (25 C)
    spec.t0_k = 298.15;
    spec.beta_k = 3950.0;
    TemperatureSensorCoupling ntc(spec);

    // At 25 C (298.15 K), resistance must equal R0
    const double r_25 = ntc.compute_resistance(298.15);
    CHECK(std::abs(r_25 - 10000.0) < 1e-6);

    // When heated to 85 C (358.15 K), NTC resistance drops sharply
    const double r_85 = ntc.compute_resistance(358.15);
    CHECK(r_85 < 1500.0); // ~1070 Ohms
    CHECK(r_85 > 800.0);

    // When cooled to 0 C (273.15 K), NTC resistance rises
    const double r_0 = ntc.compute_resistance(273.15);
    CHECK(r_0 > 25000.0); // ~32 kOhms

    // Round-trip temperature recovery from measured resistance
    const double t_recovered = ntc.compute_temperature_from_resistance(r_85);
    CHECK(std::abs(t_recovered - 358.15) < 1e-4);
}

TEST_CASE("PH4-10.03.004: Rotary Encoder / Tachometer Sensor: quadrature pulses and RPM measurement") {
    RotaryEncoderSpec spec;
    spec.pulses_per_rev = 256;
    spec.tach_volts_per_rad_s = 0.05; // 50 mV / (rad/s)
    RotaryEncoderSensor encoder(spec);

    // Initial state
    CHECK(encoder.state().total_pulse_count == 0);
    CHECK(!encoder.state().channel_a);
    CHECK(!encoder.state().channel_b);

    // Rotate 1 complete revolution (2*pi rad) in 0.1 s -> 10 rev/s = 600 RPM
    constexpr double two_pi = 2.0 * std::numbers::pi;
    const auto st = encoder.update(two_pi, 0.1);

    CHECK(st.total_pulse_count == 256);
    CHECK(std::abs(st.current_rpm - 600.0) < 1e-6);
    CHECK(st.clockwise);
    CHECK(st.tachometer_voltage_v > 3.0); // ~3.14 V

    // Reverse direction
    const auto st_rev = encoder.update(-0.5 * two_pi, 0.1);
    CHECK(st_rev.total_pulse_count == 128);
    CHECK(!st_rev.clockwise);
}

TEST_CASE("PH4-10.03.005: Battery Vibrational & Thermal Stress Modeling: mechanical shock and high temp degradation") {
    BatteryStressSpec spec;
    spec.nominal_internal_resistance = 0.025; // 25 mOhms
    spec.nominal_capacity_ah = 2.5;           // 2.5 Ah
    spec.shock_threshold_g = 15.0;            // 15 G
    BatteryStressCoupling battery(spec);

    CHECK(battery.internal_resistance_ohms() == 0.025);
    CHECK(battery.available_capacity_ah() == 2.5);
    CHECK(battery.state().state_of_health == 1.0);

    // Small normal acceleration (e.g. 5 G) does not cause structural damage
    battery.apply_mechanical_shock({0.0, 5.0 * 9.80665, 0.0});
    CHECK(battery.internal_resistance_ohms() == 0.025);
    CHECK(battery.state().total_shock_events == 0);

    // Severe collision shock (40 G) micro-cracks electrodes -> increases R_int and lowers SOH
    battery.apply_mechanical_shock({35.0 * 9.80665, 20.0 * 9.80665, 0.0});
    CHECK(battery.internal_resistance_ohms() > 0.025);
    CHECK(battery.available_capacity_ah() < 2.5);
    CHECK(battery.state().state_of_health < 1.0);
    CHECK(battery.state().total_shock_events == 1);

    // High thermal stress (> 60 C / 333.15 K) accelerates capacity fade
    battery.step_thermal_degradation(353.15, 10.0); // 80 C for 10 s
    CHECK(battery.available_capacity_ah() < 2.49);
}

TEST_CASE("PH4-10.03.006: Multi-Circuit Power Distribution Bus: shared power rails and fuse blowout") {
    PowerRailSpec spec;
    spec.rail_name = "24V_Main_Bus";
    spec.nominal_voltage_v = 24.0;
    spec.internal_resistance_ohms = 0.05;
    spec.wiring_resistance_ohms = 0.05;
    spec.max_current_a = 25.0; // 25 A fuse
    PowerDistributionBus bus(spec);

    CHECK(bus.terminal_voltage_v() == 24.0);
    CHECK(!bus.is_fuse_tripped());

    // Register branches: Branch 1 = 5 A, Branch 2 = 10 A -> Total 15 A
    bus.register_branch("drive_motors", 10.0);
    bus.register_branch("logic_controllers", 5.0);

    CHECK(bus.total_current_draw_a() == 15.0);

    // Terminal voltage under load with R_total = 0.1 Ohm: V_term = 24 - 15 * 0.1 = 22.5 V
    const double v_term = bus.terminal_voltage_v();
    CHECK(std::abs(v_term - 22.5) < 1e-9);
    CHECK(!bus.is_fuse_tripped());

    // Overcurrent fault: short circuit on drive branch draws 35 A -> Exceeds 25 A fuse!
    bus.update_branch("drive_motors", 35.0);
    const double v_blown = bus.terminal_voltage_v();
    CHECK(v_blown == 0.0);
    CHECK(bus.is_fuse_tripped());

    // Resetting fuse restores power once load is cleared
    bus.update_branch("drive_motors", 0.0);
    bus.reset_fuse();
    CHECK(!bus.is_fuse_tripped());
    CHECK(bus.terminal_voltage_v() > 23.0);
}

TEST_CASE("PH4-10.04.001: RC Car Demonstrator System: battery, switch, traction, and obstacle collision") {
    MasterCoSimulationCoordinator coordinator;
    RCCarSystem car(coordinator);

    CHECK(car.forward_speed_m_s() == 0.0);
    CHECK(!car.is_switch_closed());
    CHECK(!car.has_collided());

    // Closing power switch starts current flow and motor torque
    car.set_power_switch(true);
    car.set_throttle(1.0);

    // Advance 50 steps of 20 ms (1.0 s total)
    for (int i = 0; i < 50; ++i) {
        car.step(0.02);
    }

    CHECK(car.forward_speed_m_s() > 1.0); // Accelerated forward
    CHECK(car.wheel_rpm() > 100.0);
    CHECK(car.motor_current_a() > 0.0);

    // Continue driving until collision with obstacle at X = 8.0 m
    for (int i = 0; i < 200; ++i) {
        car.step(0.02);
        if (car.has_collided()) break;
    }

    CHECK(car.has_collided());
    CHECK(car.forward_speed_m_s() == 0.0); // Stopped upon impact
}

TEST_CASE("PH4-10.04.002: Cooling Fan Demonstrator System: thermistor temperature trigger and aerodynamic thrust") {
    MasterCoSimulationCoordinator coordinator;
    CoolingFanSystem fan(coordinator);

    // At low room temperature (25 C / 298.15 K), fan is inactive
    fan.set_ambient_temperature_k(298.15);
    fan.step(0.02);
    CHECK(!fan.is_fan_active());
    CHECK(fan.fan_rpm() == 0.0);
    CHECK(fan.airflow_thrust_n() == 0.0);

    // When temperature rises to 65 C (338.15 K), thermistor resistance drops, activating fan
    fan.set_ambient_temperature_k(338.15);
    for (int i = 0; i < 50; ++i) {
        fan.step(0.02);
    }

    CHECK(fan.is_fan_active());
    CHECK(fan.fan_rpm() > 500.0); // Rotor spun up
    CHECK(fan.airflow_thrust_n() > 0.0); // Generates measurable aerodynamic thrust
}

TEST_CASE("PH4-10.04.003: Automatic Sliding Door System: proximity trigger, slider travel, hold timer, and reversal") {
    MasterCoSimulationCoordinator coordinator;
    AutomaticSlidingDoorSystem door(coordinator);

    CHECK(door.is_fully_closed());
    CHECK(!door.is_opening());

    // Object detected by proximity sensor -> Door starts opening
    door.trigger_proximity(true);
    door.step(0.05);
    CHECK(door.is_opening());

    // Advance until door reaches fully open position (1.2 m travel)
    for (int i = 0; i < 50; ++i) {
        door.step(0.05);
        if (door.is_fully_open()) break;
    }
    CHECK(door.is_fully_open());

    // Object moves away -> Hold timer runs for 2 seconds, then door starts closing
    door.trigger_proximity(false);
    for (int i = 0; i < 50; ++i) { // 2.5 seconds
        door.step(0.05);
    }
    CHECK(door.is_closing());

    // Safety reversal: object appears while closing -> Door reverses back to opening
    door.trigger_proximity(true);
    door.step(0.05);
    CHECK(door.is_opening());
}

TEST_CASE("PH4-10.04.004: LED Indicator Lighting System: push button, forward current, and photometric illumination") {
    MasterCoSimulationCoordinator coordinator;
    LedIndicatorLightingSystem lighting(coordinator);

    CHECK(!lighting.is_illuminated());
    CHECK(lighting.led_current_ma() == 0.0);
    CHECK(lighting.luminous_flux_lumens() == 0.0);

    // Press push button
    lighting.press_button(true);
    lighting.step(0.01);

    CHECK(lighting.is_illuminated());
    // I = (12 - 3.2) / 470 ~ 18.72 mA
    CHECK(lighting.led_current_ma() > 18.0);
    CHECK(lighting.led_current_ma() < 19.5);
    // Photometric flux ~ 100 to 120 lumens
    CHECK(lighting.luminous_flux_lumens() > 100.0);

    // Release push button
    lighting.press_button(false);
    lighting.step(0.01);
    CHECK(!lighting.is_illuminated());
    CHECK(lighting.led_current_ma() == 0.0);
}

TEST_CASE("PH4-10.04.005: Demonstrator Preset Manager: 1-click preset instantiation and queries") {
    MasterCoSimulationCoordinator coordinator;

    const auto presets = DemonstratorPresetManager::available_presets();
    CHECK(presets.size() == 4);

    auto car = DemonstratorPresetManager::create_rc_car(coordinator);
    REQUIRE(car != nullptr);

    auto fan = DemonstratorPresetManager::create_cooling_fan(coordinator);
    REQUIRE(fan != nullptr);

    auto door = DemonstratorPresetManager::create_sliding_door(coordinator);
    REQUIRE(door != nullptr);

    auto led = DemonstratorPresetManager::create_led_lighting(coordinator);
    REQUIRE(led != nullptr);
}

TEST_CASE("PH4-10.04.006: Automated End-to-End Integration Tests executing all 4 machines concurrently") {
    MasterCoSimulationCoordinator coordinator;

    auto car = DemonstratorPresetManager::create_rc_car(coordinator);
    auto fan = DemonstratorPresetManager::create_cooling_fan(coordinator);
    auto door = DemonstratorPresetManager::create_sliding_door(coordinator);
    auto led = DemonstratorPresetManager::create_led_lighting(coordinator);

    // Activate all machines concurrently:
    car->set_power_switch(true);
    fan->set_ambient_temperature_k(340.0); // Hot -> triggers fan
    door->trigger_proximity(true);         // Person present -> opens door
    led->press_button(true);               // Button down -> lights LED

    // Execute 15 synchronized co-simulation steps
    for (int step = 0; step < 15; ++step) {
        car->step(0.02);
        fan->step(0.02);
        door->step(0.02);
        led->step(0.02);
    }

    // Validate physical velocities and circuit responses across all 4 machines
    CHECK(car->forward_speed_m_s() > 0.5);
    CHECK(fan->fan_rpm() > 200.0);
    CHECK(door->door_position_m() > 0.2);
    CHECK(led->is_illuminated());
    CHECK(led->led_current_ma() > 15.0);
}


