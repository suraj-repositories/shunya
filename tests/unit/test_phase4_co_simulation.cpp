#include "tests/test_framework.hpp"

#include "shunya/cosim/master_coordinator.hpp"
#include "shunya/cosim/sensor_coupling.hpp"
#include "shunya/cosim/transducers.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cmath>
#include <numbers>
#include <vector>

using namespace shunya::cosim;
using namespace shunya::physim;

// =============================================================================
// PH4-13.01.005: Co-Simulation Engine & Transducers Verification
// =============================================================================

TEST_CASE("PH4-13.01.005: Electro-Mechanical Motor Transducer & Back-EMF Coupling") {
    DCMotorSpec spec;
    spec.Kt = 0.05; // 0.05 N*m/A
    spec.Ra = 1.2;  // 1.2 Ohm
    spec.viscous_damping = 1e-4;
    spec.coulomb_friction = 1e-3;

    DCMotorTransducer motor(spec);

    // Dynamic rotor body
    RigidBodyEntity rotor("rotor", BodyType::Dynamic);
    rotor.state().set_mass(0.2);
    rotor.state().set_local_inertia(Matrix3x3::diagonal(1e-4, 1e-4, 1e-4));

    // When 2.0 A current flows at zero angular velocity:
    // Torque = Kt * I = 0.05 * 2.0 = 0.10 N*m (minus coulomb if moving)
    const double initial_torque = motor.compute_torque(2.0, 0.0);
    CHECK(std::abs(initial_torque - 0.10) < 1e-4);

    // Apply torque along Z axis
    motor.apply_torque_to_body(rotor, {0.0, 0.0, 1.0}, 2.0);

    // Back-EMF calculation: E_b = Ke * omega
    const double omega = 100.0; // 100 rad/s
    const double back_emf = MotorBackEmfCoupling::compute_back_emf(omega, spec.Kt);
    CHECK(std::abs(back_emf - 5.0) < 1e-4); // 0.05 * 100 = 5.0 V
}

TEST_CASE("PH4-13.01.005: Sensor Feedback: Limit Switch & Rotary Encoder") {
    // 1. Rotary Encoder Sensor
    RotaryEncoderSpec enc_spec;
    enc_spec.pulses_per_rev = 360;
    enc_spec.tach_volts_per_rad_s = 0.05; // 50 mV per rad/s

    RotaryEncoderSensor encoder(enc_spec);

    // Rotate 1 complete revolution (2*pi rad) in 0.1s -> 10 rev/s = 600 RPM
    constexpr double two_pi = 2.0 * std::numbers::pi;
    const double dt = 0.01;
    const double delta_angle = two_pi * 0.1; // 10 steps of 0.1 rev

    EncoderState state;
    for (int i = 0; i < 10; ++i) {
        state = encoder.update(delta_angle, dt);
    }

    CHECK(state.total_pulse_count == 360);
    CHECK(std::abs(state.current_rpm - 600.0) < 1.0);
    CHECK(std::abs(state.tachometer_voltage_v - (two_pi * 10.0 * 0.05)) < 0.1);
    CHECK(state.clockwise == true);

    // 2. Limit Switch Sensor
    LimitSwitchSpec sw_spec;
    sw_spec.target_body_a = "carriage";
    sw_spec.target_body_b = "endstop";
    sw_spec.trigger_distance_m = 0.01; // 10 mm
    sw_spec.r_closed_ohms = 0.05;
    sw_spec.r_open_ohms = 1e6;

    LimitSwitchSensor limit_switch(sw_spec);
    CHECK_FALSE(limit_switch.is_closed());
    CHECK(limit_switch.dynamic_resistance_ohms() >= 1e6);

    // Move carriage into proximity
    limit_switch.update_from_distance(0.005); // 5 mm <= 10 mm
    CHECK(limit_switch.is_closed());
    CHECK(limit_switch.dynamic_resistance_ohms() <= 0.1);
}

TEST_CASE("PH4-13.01.005: Global Energy Conservation Audit Across Domains") {
    EnergyBalanceAuditor auditor;

    // Simulate 100 time steps of an electric motor accelerating a flywheel
    const double dt = 0.001; // 1 ms
    const double voltage = 12.0; // 12V
    const double resistance = 2.0; // 2 Ohms

    double omega = 0.0;
    const double Kt = 0.05;
    const double inertia = 1e-3;

    for (int step = 0; step < 100; ++step) {
        const double back_emf = Kt * omega;
        const double current = (voltage - back_emf) / resistance;
        const double torque = Kt * current;

        // Flywheel acceleration
        const double alpha = torque / inertia;
        omega += alpha * dt;

        auditor.accumulate_step(
            voltage,
            current,
            torque,
            omega,
            0.0, // linear force
            0.0, // linear vel
            resistance,
            dt
        );
    }

    CHECK(auditor.electrical_energy_j() > 0.0);
    CHECK(auditor.mechanical_work_j() > 0.0);
    CHECK(auditor.thermal_losses_j() > 0.0);

    // Assert that Energy In == Mechanical Work + Thermal Dissipation
    CHECK(auditor.is_energy_conserved(0.05));
    CHECK(std::abs(auditor.energy_residual_j()) < 0.05);
}
