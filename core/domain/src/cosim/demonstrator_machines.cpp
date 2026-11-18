#include "shunya/cosim/demonstrator_machines.hpp"

#include <cmath>

namespace shunya::cosim {

// =============================================================================
// 1. RC Car Demonstrator System (PH4-10.04.001)
// =============================================================================
RCCarSystem::RCCarSystem(MasterCoSimulationCoordinator& coordinator)
    : coordinator_(coordinator) {
    // 1. Create 3D Chassis body
    physim::RigidBodyEntity chassis("rc_car_chassis", physim::BodyType::Dynamic);
    chassis.state().set_mass(1.2);
    chassis.state().position = {0.0, 0.1, 0.0};
    chassis.aero().enable_drag = false;
    coordinator_.physics_controller().add_body(chassis);

    // 2. Create Drive Wheel Axle
    physim::RigidBodyEntity rear_axle("rc_car_rear_axle", physim::BodyType::Dynamic);
    rear_axle.state().set_mass(0.2);
    rear_axle.state().set_local_inertia(physim::Matrix3x3::diagonal(1e-4, 1e-4, 1e-4));
    rear_axle.state().position = {-0.2, 0.05, 0.0};
    rear_axle.aero().enable_drag = false;
    coordinator_.physics_controller().add_body(rear_axle);

    // 3. Configure DC Motor parameters
    DCMotorSpec mspec;
    mspec.Kt = 0.05;
    mspec.Ra = 0.8;
    motor_.set_spec(mspec);
}

void RCCarSystem::step(double dt_s) {
    auto* chassis = coordinator_.physics_controller().find_body("rc_car_chassis");
    auto* axle = coordinator_.physics_controller().find_body("rc_car_rear_axle");
    if (!chassis || !axle) return;

    if (switch_closed_) {
        constexpr double Ke = 0.05;
        constexpr double wheel_radius_m = 0.05;
        // Wheel roll coupled to car speed via ground contact
        const double v_chassis = std::max(0.0, chassis->state().linear_velocity().x);
        const double omega = v_chassis / wheel_radius_m;

        // Armature current with back-EMF: I = (V_batt - Ke * omega) / Ra
        const double v_bemf = Ke * omega;
        motor_current_a_ = std::max(0.0, (battery_voltage_v_ - v_bemf) / motor_.spec().Ra) * throttle_;

        // Electromagnetic torque on rear axle
        const double tau = motor_.compute_torque(motor_current_a_, omega);

        // Forward traction force: F_traction = tau / r_wheel
        const double f_traction = (tau / wheel_radius_m);
        chassis->state().apply_force_at_com({f_traction, 0.0, 0.0});

        // Sync axle velocity and rolling angular speed with chassis
        axle->state().position = chassis->state().position + physim::Vector3d{-0.2, -0.05, 0.0};
        axle->state().set_linear_velocity(chassis->state().linear_velocity());
        axle->state().set_angular_velocity({0.0, 0.0, omega});
    } else {
        motor_current_a_ = 0.0;
    }

    // Collision check against virtual obstacle at X = 8.0 m
    if (chassis->state().position.x >= 8.0) {
        collided_ = true;
        switch_closed_ = false;
        motor_current_a_ = 0.0;
        chassis->state().set_linear_velocity({0.0, 0.0, 0.0});
        axle->state().set_linear_velocity({0.0, 0.0, 0.0});
        axle->state().set_angular_velocity({0.0, 0.0, 0.0});
    }

    coordinator_.physics_controller().step_forward(dt_s);

    if (collided_) {
        chassis->state().set_linear_velocity({0.0, 0.0, 0.0});
        axle->state().set_linear_velocity({0.0, 0.0, 0.0});
        axle->state().set_angular_velocity({0.0, 0.0, 0.0});
    }
}

double RCCarSystem::forward_speed_m_s() const noexcept {
    const auto* chassis = coordinator_.physics_controller().find_body("rc_car_chassis");
    return chassis ? chassis->state().linear_velocity().x : 0.0;
}

double RCCarSystem::wheel_rpm() const noexcept {
    const auto* axle = coordinator_.physics_controller().find_body("rc_car_rear_axle");
    if (!axle) return 0.0;
    const double omega = axle->state().angular_velocity().length();
    return (omega * 60.0) / (2.0 * std::numbers::pi);
}

// =============================================================================
// 2. Cooling Fan Demonstrator System (PH4-10.04.002)
// =============================================================================
CoolingFanSystem::CoolingFanSystem(MasterCoSimulationCoordinator& coordinator)
    : coordinator_(coordinator) {
    // Shroud (Static)
    physim::RigidBodyEntity shroud("fan_shroud", physim::BodyType::Static);
    shroud.state().position = {0.0, 0.5, 0.0};
    coordinator_.physics_controller().add_body(shroud);

    // Fan Rotor (Dynamic)
    physim::RigidBodyEntity rotor("fan_rotor", physim::BodyType::Dynamic);
    rotor.state().set_mass(0.08);
    rotor.state().set_local_inertia(physim::Matrix3x3::diagonal(1e-4, 1e-4, 1e-4));
    rotor.state().position = {0.0, 0.5, 0.0};
    rotor.aero().enable_drag = false;
    coordinator_.physics_controller().add_body(rotor);

    // Motor spec
    DCMotorSpec mspec;
    mspec.Kt = 0.02;
    mspec.Ra = 1.5;
    fan_motor_.set_spec(mspec);
}

void CoolingFanSystem::step(double dt_s) {
    auto* rotor = coordinator_.physics_controller().find_body("fan_rotor");
    if (!rotor) return;

    // Thermistor resistance under current temperature
    thermistor_r_ohms_ = temp_sensor_.compute_resistance(ambient_temp_k_);

    // Threshold comparator: if R < 5000 Ohms (temp > ~45 C / 318 K), fan activates
    if (thermistor_r_ohms_ < 5000.0) {
        fan_active_ = true;
        constexpr double V_fan = 12.0;
        constexpr double Ke = 0.02;
        const double omega = rotor->state().angular_velocity().length();
        const double bemf = Ke * omega;
        motor_current_a_ = std::max(0.0, (V_fan - bemf) / fan_motor_.spec().Ra);

        fan_motor_.apply_torque_to_body(*rotor, {0.0, 0.0, 1.0}, motor_current_a_);
    } else {
        fan_active_ = false;
        motor_current_a_ = 0.0;
        // Damping slows rotor down
        const auto v_ang = rotor->state().angular_velocity();
        rotor->state().apply_torque(v_ang * -0.05);
    }

    coordinator_.physics_controller().step_forward(dt_s);
}

double CoolingFanSystem::fan_rpm() const noexcept {
    const auto* rotor = coordinator_.physics_controller().find_body("fan_rotor");
    if (!rotor) return 0.0;
    const double omega = rotor->state().angular_velocity().length();
    return (omega * 60.0) / (2.0 * std::numbers::pi);
}

double CoolingFanSystem::airflow_thrust_n() const noexcept {
    const double rpm = fan_rpm();
    // Empirical thrust: F = k * (RPM / 1000)^2
    return 0.025 * std::pow(rpm / 1000.0, 2.0);
}

// =============================================================================
// 3. Automatic Sliding Door System (PH4-10.04.003)
// =============================================================================
AutomaticSlidingDoorSystem::AutomaticSlidingDoorSystem(MasterCoSimulationCoordinator& coordinator)
    : coordinator_(coordinator) {
    // Door Leaf Body
    physim::RigidBodyEntity leaf("sliding_door_leaf", physim::BodyType::Dynamic);
    leaf.state().set_mass(15.0);
    leaf.state().position = {0.0, 1.0, 0.0};
    leaf.aero().enable_drag = false;
    coordinator_.physics_controller().add_body(leaf);
}

void AutomaticSlidingDoorSystem::step(double dt_s) {
    auto* leaf = coordinator_.physics_controller().find_body("sliding_door_leaf");
    if (!leaf) return;

    const double pos = leaf->state().position.x;
    constexpr double drive_speed = 0.6; // 0.6 m/s

    switch (state_) {
        case DoorState::Closed:
            leaf->state().set_linear_velocity({0.0, 0.0, 0.0});
            if (object_detected_) {
                state_ = DoorState::Opening;
            }
            break;

        case DoorState::Opening:
            leaf->state().set_linear_velocity({drive_speed, 0.0, 0.0});
            if (pos >= kMaxTravel_m) {
                leaf->state().position.x = kMaxTravel_m;
                leaf->state().set_linear_velocity({0.0, 0.0, 0.0});
                state_ = DoorState::OpenHold;
                hold_timer_s_ = 0.0;
            }
            break;

        case DoorState::OpenHold:
            leaf->state().set_linear_velocity({0.0, 0.0, 0.0});
            if (object_detected_) {
                hold_timer_s_ = 0.0; // Reset timer while person is present
            } else {
                hold_timer_s_ += dt_s;
                if (hold_timer_s_ >= kHoldDuration_s) {
                    state_ = DoorState::Closing;
                }
            }
            break;

        case DoorState::Closing:
            if (object_detected_) {
                // Safety reversal!
                state_ = DoorState::Opening;
            } else {
                leaf->state().set_linear_velocity({-drive_speed, 0.0, 0.0});
                if (pos <= 0.0) {
                    leaf->state().position.x = 0.0;
                    leaf->state().set_linear_velocity({0.0, 0.0, 0.0});
                    state_ = DoorState::Closed;
                }
            }
            break;
    }

    coordinator_.physics_controller().step_forward(dt_s);
}

double AutomaticSlidingDoorSystem::door_position_m() const noexcept {
    const auto* leaf = coordinator_.physics_controller().find_body("sliding_door_leaf");
    return leaf ? leaf->state().position.x : 0.0;
}

// =============================================================================
// 4. LED Indicator Lighting System (PH4-10.04.004)
// =============================================================================
LedIndicatorLightingSystem::LedIndicatorLightingSystem(MasterCoSimulationCoordinator& coordinator)
    : coordinator_(coordinator) {
    physim::RigidBodyEntity lamp("headlamp_fixture", physim::BodyType::Static);
    lamp.state().position = {0.0, 1.2, 0.0};
    coordinator_.physics_controller().add_body(lamp);
}

void LedIndicatorLightingSystem::step(double /*dt_s*/) {
    if (button_pressed_) {
        constexpr double V_batt = 12.0;
        constexpr double V_led = 3.2;
        constexpr double R_series = 470.0;

        const double current_a = (V_batt - V_led) / R_series;
        led_current_ma_ = current_a * 1000.0; // ~18.72 mA
        luminous_flux_lumens_ = 120.0 * (led_current_ma_ / 20.0); // ~112.3 lumens
        is_lit_ = true;
    } else {
        led_current_ma_ = 0.0;
        luminous_flux_lumens_ = 0.0;
        is_lit_ = false;
    }
}

// =============================================================================
// 5. Demonstrator Preset Manager (PH4-10.04.005 & PH4-10.04.006)
// =============================================================================
std::unique_ptr<RCCarSystem> DemonstratorPresetManager::create_rc_car(
    MasterCoSimulationCoordinator& coordinator) {
    return std::make_unique<RCCarSystem>(coordinator);
}

std::unique_ptr<CoolingFanSystem> DemonstratorPresetManager::create_cooling_fan(
    MasterCoSimulationCoordinator& coordinator) {
    return std::make_unique<CoolingFanSystem>(coordinator);
}

std::unique_ptr<AutomaticSlidingDoorSystem> DemonstratorPresetManager::create_sliding_door(
    MasterCoSimulationCoordinator& coordinator) {
    return std::make_unique<AutomaticSlidingDoorSystem>(coordinator);
}

std::unique_ptr<LedIndicatorLightingSystem> DemonstratorPresetManager::create_led_lighting(
    MasterCoSimulationCoordinator& coordinator) {
    return std::make_unique<LedIndicatorLightingSystem>(coordinator);
}

std::vector<std::string> DemonstratorPresetManager::available_presets() {
    return {
        "RC Car Demonstrator",
        "Cooling Fan Demonstrator",
        "Automatic Sliding Door Demonstrator",
        "LED Indicator Lighting Demonstrator"
    };
}

} // namespace shunya::cosim
