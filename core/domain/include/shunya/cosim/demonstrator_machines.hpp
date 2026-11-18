#pragma once

#include "shunya/cosim/master_coordinator.hpp"
#include "shunya/cosim/transducers.hpp"
#include "shunya/cosim/sensor_coupling.hpp"

#include <memory>
#include <string>
#include <vector>

namespace shunya::cosim {

// =============================================================================
// 1. RC Car Demonstrator System (PH4-10.04.001)
// =============================================================================
class RCCarSystem {
public:
    explicit RCCarSystem(MasterCoSimulationCoordinator& coordinator);

    void set_power_switch(bool closed) noexcept { switch_closed_ = closed; }
    void set_steering_angle(double rad) noexcept { target_steering_angle_rad_ = rad; }
    void set_throttle(double factor_0_to_1) noexcept { throttle_ = std::clamp(factor_0_to_1, 0.0, 1.0); }

    void step(double dt_s);

    [[nodiscard]] double forward_speed_m_s() const noexcept;
    [[nodiscard]] double wheel_rpm() const noexcept;
    [[nodiscard]] double motor_current_a() const noexcept { return motor_current_a_; }
    [[nodiscard]] double battery_voltage_v() const noexcept { return battery_voltage_v_; }
    [[nodiscard]] bool is_switch_closed() const noexcept { return switch_closed_; }
    [[nodiscard]] bool has_collided() const noexcept { return collided_; }

private:
    MasterCoSimulationCoordinator& coordinator_;
    bool switch_closed_{false};
    double target_steering_angle_rad_{0.0};
    double throttle_{1.0};
    double motor_current_a_{0.0};
    double battery_voltage_v_{7.4};
    bool collided_{false};
    DCMotorTransducer motor_;
};

// =============================================================================
// 2. Cooling Fan Demonstrator System (PH4-10.04.002)
// =============================================================================
class CoolingFanSystem {
public:
    explicit CoolingFanSystem(MasterCoSimulationCoordinator& coordinator);

    void set_ambient_temperature_k(double temp_k) noexcept { ambient_temp_k_ = temp_k; }
    void step(double dt_s);

    [[nodiscard]] double fan_rpm() const noexcept;
    [[nodiscard]] double thermistor_resistance_ohms() const noexcept { return thermistor_r_ohms_; }
    [[nodiscard]] double airflow_thrust_n() const noexcept;
    [[nodiscard]] bool is_fan_active() const noexcept { return fan_active_; }

private:
    MasterCoSimulationCoordinator& coordinator_;
    double ambient_temp_k_{298.15}; // 25 C nominal
    double thermistor_r_ohms_{10000.0};
    bool fan_active_{false};
    double motor_current_a_{0.0};
    TemperatureSensorCoupling temp_sensor_;
    DCMotorTransducer fan_motor_;
};

// =============================================================================
// 3. Automatic Sliding Door System (PH4-10.04.003)
// =============================================================================
class AutomaticSlidingDoorSystem {
public:
    explicit AutomaticSlidingDoorSystem(MasterCoSimulationCoordinator& coordinator);

    void trigger_proximity(bool object_present) noexcept { object_detected_ = object_present; }
    void step(double dt_s);

    [[nodiscard]] double door_position_m() const noexcept;
    [[nodiscard]] bool is_opening() const noexcept { return state_ == DoorState::Opening; }
    [[nodiscard]] bool is_closing() const noexcept { return state_ == DoorState::Closing; }
    [[nodiscard]] bool is_fully_open() const noexcept { return door_position_m() >= 1.15; }
    [[nodiscard]] bool is_fully_closed() const noexcept { return door_position_m() <= 0.05; }

private:
    enum class DoorState { Closed, Opening, OpenHold, Closing };

    MasterCoSimulationCoordinator& coordinator_;
    DoorState state_{DoorState::Closed};
    bool object_detected_{false};
    double hold_timer_s_{0.0};
    constexpr static double kHoldDuration_s{2.0};
    constexpr static double kMaxTravel_m{1.2};
    DCMotorTransducer door_motor_;
    LimitSwitchSensor open_limit_switch_;
};

// =============================================================================
// 4. LED Indicator Lighting System (PH4-10.04.004)
// =============================================================================
class LedIndicatorLightingSystem {
public:
    explicit LedIndicatorLightingSystem(MasterCoSimulationCoordinator& coordinator);

    void press_button(bool pressed) noexcept { button_pressed_ = pressed; }
    void step(double dt_s);

    [[nodiscard]] bool is_illuminated() const noexcept { return is_lit_; }
    [[nodiscard]] double led_current_ma() const noexcept { return led_current_ma_; }
    [[nodiscard]] double luminous_flux_lumens() const noexcept { return luminous_flux_lumens_; }

private:
    MasterCoSimulationCoordinator& coordinator_;
    bool button_pressed_{false};
    bool is_lit_{false};
    double led_current_ma_{0.0};
    double luminous_flux_lumens_{0.0};
};

// =============================================================================
// 5. Demonstrator Preset Manager (PH4-10.04.005 & PH4-10.04.006)
// =============================================================================
class DemonstratorPresetManager {
public:
    [[nodiscard]] static std::unique_ptr<RCCarSystem> create_rc_car(
        MasterCoSimulationCoordinator& coordinator);

    [[nodiscard]] static std::unique_ptr<CoolingFanSystem> create_cooling_fan(
        MasterCoSimulationCoordinator& coordinator);

    [[nodiscard]] static std::unique_ptr<AutomaticSlidingDoorSystem> create_sliding_door(
        MasterCoSimulationCoordinator& coordinator);

    [[nodiscard]] static std::unique_ptr<LedIndicatorLightingSystem> create_led_lighting(
        MasterCoSimulationCoordinator& coordinator);

    [[nodiscard]] static std::vector<std::string> available_presets();
};

} // namespace shunya::cosim
