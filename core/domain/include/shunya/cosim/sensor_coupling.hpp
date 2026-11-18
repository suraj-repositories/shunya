#pragma once

#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace shunya::cosim {

// =============================================================================
// 3D Limit Switch Sensor Coupling (PH4-10.03.001)
// =============================================================================
struct LimitSwitchSpec {
    std::string target_body_a;
    std::string target_body_b;
    double trigger_distance_m{0.005};    ///< Contact threshold distance (5 mm)
    double r_closed_ohms{1e-3};          ///< Closed contact resistance (1 mOhm)
    double r_open_ohms{1e8};             ///< Open air-gap resistance (100 MOhm)
    bool normally_closed{false};         ///< NC vs NO configuration
};

class LimitSwitchSensor {
public:
    explicit LimitSwitchSensor(LimitSwitchSpec spec = {}) : spec_(spec) {}

    void update_from_distance(double distance_m) noexcept {
        const bool physical_trigger = (distance_m <= spec_.trigger_distance_m);
        is_triggered_ = physical_trigger;
    }

    void update_from_contacts(const std::vector<physim::PhysimContactManifold>& manifolds) noexcept {
        bool contact_found = false;
        for (const auto& m : manifolds) {
            if ((m.body_a_id == spec_.target_body_a && m.body_b_id == spec_.target_body_b) ||
                (m.body_a_id == spec_.target_body_b && m.body_b_id == spec_.target_body_a)) {
                if (!m.contacts.empty()) {
                    contact_found = true;
                    break;
                }
            }
        }
        is_triggered_ = contact_found;
    }

    [[nodiscard]] bool is_closed() const noexcept {
        if (spec_.normally_closed) {
            return !is_triggered_;
        }
        return is_triggered_;
    }

    [[nodiscard]] double dynamic_resistance_ohms() const noexcept {
        return is_closed() ? spec_.r_closed_ohms : spec_.r_open_ohms;
    }

    [[nodiscard]] const LimitSwitchSpec& spec() const noexcept { return spec_; }
    void set_spec(const LimitSwitchSpec& sp) noexcept { spec_ = sp; }

private:
    LimitSwitchSpec spec_;
    bool is_triggered_{false};
};

// =============================================================================
// Optical / Light Dependent Resistor (LDR) Sensor Coupling (PH4-10.03.002)
// =============================================================================
struct LdrSpec {
    double r0_ohms{1000.0};             ///< Reference resistance at E0 lux (1000 Ohms)
    double e0_lux{10.0};                ///< Reference illuminance (10 lux)
    double gamma{0.7};                  ///< Sensitivity slope gamma (~0.7 for standard CdS)
    double dark_resistance_ohms{1e6};   ///< Resistance in pure darkness (1 MOhm)
    double ambient_lux{5.0};            ///< Baseline ambient background illuminance (5 lux)
};

class OpticalLdrSensorCoupling {
public:
    explicit OpticalLdrSensorCoupling(LdrSpec spec = {}) : spec_(spec) {}

    [[nodiscard]] double compute_illuminance(
        const physim::Vector3d& sensor_pos,
        const physim::Vector3d& sensor_normal,
        const physim::Vector3d& light_pos,
        double light_lumens,
        bool in_shadow = false) const noexcept {
        if (in_shadow) {
            return spec_.ambient_lux;
        }

        const physim::Vector3d delta = light_pos - sensor_pos;
        const double r2 = delta.length_squared();
        if (r2 <= 1e-6) {
            return spec_.dark_resistance_ohms;
        }

        const double r = std::sqrt(r2);
        const physim::Vector3d l_dir = delta * (1.0 / r);
        const double cos_theta = std::max(0.0, sensor_normal.normalized().dot(l_dir));

        // Inverse square law with Lambertian cosine angle: E = (Phi / (4 * pi * r^2)) * cos(theta)
        const double direct_lux = (light_lumens / (4.0 * std::numbers::pi * r2)) * cos_theta;
        return spec_.ambient_lux + direct_lux;
    }

    [[nodiscard]] double compute_resistance(double illuminance_lux) const noexcept {
        if (illuminance_lux <= 1e-3) {
            return spec_.dark_resistance_ohms;
        }
        
        // R(E) = R0 * (E0 / E)^gamma
        const double ratio = spec_.e0_lux / illuminance_lux;
        const double r_calc = spec_.r0_ohms * std::pow(ratio, spec_.gamma);
        return std::clamp(r_calc, 1.0, spec_.dark_resistance_ohms);
    }

    [[nodiscard]] const LdrSpec& spec() const noexcept { return spec_; }
    void set_spec(const LdrSpec& sp) noexcept { spec_ = sp; }

private:
    LdrSpec spec_;
};

// =============================================================================
// Temperature Sensor / Thermistor Coupling (PH4-10.03.003)
// =============================================================================
struct ThermistorSpec {
    double r0_ohms{10000.0};            ///< Nominal resistance R0 at T0 (10 kOhms at 25 C)
    double t0_k{298.15};                ///< Reference temperature T0 (298.15 K)
    double beta_k{3950.0};              ///< Material characteristic constant Beta (3950 K)
    double min_resistance_ohms{50.0};   ///< Lower resistance clamp
    double max_resistance_ohms{1e6};    ///< Upper resistance clamp (1 MOhm)
};

class TemperatureSensorCoupling {
public:
    explicit TemperatureSensorCoupling(ThermistorSpec spec = {}) : spec_(spec) {}

    [[nodiscard]] double compute_resistance(double temperature_k) const noexcept {
        const double t = std::max(10.0, temperature_k);
        // Steinhart-Hart / Beta equation: R(T) = R0 * exp(Beta * (1/T - 1/T0))
        const double exponent = spec_.beta_k * ((1.0 / t) - (1.0 / spec_.t0_k));
        const double r = spec_.r0_ohms * std::exp(exponent);
        return std::clamp(r, spec_.min_resistance_ohms, spec_.max_resistance_ohms);
    }

    [[nodiscard]] double compute_temperature_from_resistance(double resistance_ohms) const noexcept {
        if (resistance_ohms <= 0.0) return spec_.t0_k;
        // Inverse: 1/T = 1/T0 + (1/Beta) * ln(R / R0)
        const double inv_t = (1.0 / spec_.t0_k) + (1.0 / spec_.beta_k) * std::log(resistance_ohms / spec_.r0_ohms);
        if (inv_t <= 0.0) return 1000.0;
        return 1.0 / inv_t;
    }

    [[nodiscard]] const ThermistorSpec& spec() const noexcept { return spec_; }
    void set_spec(const ThermistorSpec& sp) noexcept { spec_ = sp; }

private:
    ThermistorSpec spec_;
};

// =============================================================================
// Rotary Encoder / Tachometer Sensor (PH4-10.03.004)
// =============================================================================
struct RotaryEncoderSpec {
    uint32_t pulses_per_rev{256};       ///< Resolution PPR (256 pulses/rev)
    double tach_volts_per_rad_s{0.05};  ///< Analog tachometer output constant (50 mV / (rad/s))
};

struct EncoderState {
    int64_t total_pulse_count{0};
    bool channel_a{false};
    bool channel_b{false};
    double current_rpm{0.0};
    double tachometer_voltage_v{0.0};
    bool clockwise{true};
};

class RotaryEncoderSensor {
public:
    explicit RotaryEncoderSensor(RotaryEncoderSpec spec = {}) : spec_(spec) {}

    EncoderState update(double delta_angle_rad, double dt_s) noexcept {
        if (dt_s > 0.0) {
            current_omega_rad_s_ = delta_angle_rad / dt_s;
        }
        accumulated_angle_rad_ += delta_angle_rad;

        // Pulses = (angle / 2*pi) * PPR
        constexpr double two_pi = 2.0 * std::numbers::pi;
        const double pulses_f = (accumulated_angle_rad_ / two_pi) * static_cast<double>(spec_.pulses_per_rev);
        const int64_t total_pulses = static_cast<int64_t>(std::floor(pulses_f));

        // Quadrature Phase A and B (90 deg electrical shift):
        // Cycle quadrant: pulse & 3 -> (0: A=0,B=0), (1: A=1,B=0), (2: A=1,B=1), (3: A=0,B=1)
        const int quad = static_cast<int>(std::abs(total_pulses) % 4);
        const bool ch_a = (quad == 1 || quad == 2);
        const bool ch_b = (quad == 2 || quad == 3);

        const double rpm = (current_omega_rad_s_ * 60.0) / two_pi;
        const double v_tach = current_omega_rad_s_ * spec_.tach_volts_per_rad_s;

        state_.total_pulse_count = total_pulses;
        state_.channel_a = ch_a;
        state_.channel_b = ch_b;
        state_.current_rpm = rpm;
        state_.tachometer_voltage_v = v_tach;
        state_.clockwise = (current_omega_rad_s_ >= 0.0);

        return state_;
    }

    [[nodiscard]] const EncoderState& state() const noexcept { return state_; }
    [[nodiscard]] const RotaryEncoderSpec& spec() const noexcept { return spec_; }
    void reset() noexcept {
        accumulated_angle_rad_ = 0.0;
        current_omega_rad_s_ = 0.0;
        state_ = {};
    }

private:
    RotaryEncoderSpec spec_;
    double accumulated_angle_rad_{0.0};
    double current_omega_rad_s_{0.0};
    EncoderState state_;
};

// =============================================================================
// Battery Vibrational & Thermal Stress Modeling (PH4-10.03.005)
// =============================================================================
struct BatteryStressSpec {
    double shock_threshold_g{15.0};             ///< G-force threshold triggering internal damage
    double shock_resistance_factor{5e-5};       ///< dR_int per G above threshold (Ohms / G)
    double thermal_threshold_k{333.15};         ///< 60 C threshold for accelerated thermal degradation
    double nominal_internal_resistance{0.025};  ///< 25 mOhms baseline internal resistance
    double nominal_capacity_ah{2.5};            ///< 2.5 Ah nominal capacity
};

struct BatteryDegradationState {
    double internal_resistance_ohms{0.025};
    double available_capacity_ah{2.5};
    double state_of_health{1.0};               ///< SOH in [0, 1]
    uint64_t total_shock_events{0};
};

class BatteryStressCoupling {
public:
    explicit BatteryStressCoupling(BatteryStressSpec spec = {})
        : spec_(spec),
          current_resistance_ohms_(spec.nominal_internal_resistance),
          available_capacity_ah_(spec.nominal_capacity_ah) {}

    void apply_mechanical_shock(const physim::Vector3d& acceleration_m_s2) noexcept {
        constexpr double g_standard = 9.80665;
        const double a_g = acceleration_m_s2.length() / g_standard;

        if (a_g > spec_.shock_threshold_g) {
            const double excess_g = a_g - spec_.shock_threshold_g;
            const double delta_r = excess_g * spec_.shock_resistance_factor;
            current_resistance_ohms_ += delta_r;
            // High shock micro-cracking degrades capacity
            available_capacity_ah_ = std::max(0.1, available_capacity_ah_ - delta_r * 0.5);
            ++shock_events_;
        }
    }

    void step_thermal_degradation(double cell_temperature_k, double dt_s) noexcept {
        if (cell_temperature_k > spec_.thermal_threshold_k && dt_s > 0.0) {
            const double delta_t = cell_temperature_k - spec_.thermal_threshold_k;
            // Arrhenius-like degradation rate: dCap / dt = k * delta_t
            constexpr double k_thermal = 1e-4; // Ah / (K * s)
            available_capacity_ah_ = std::max(0.1, available_capacity_ah_ - k_thermal * delta_t * dt_s);
            current_resistance_ohms_ += 1e-5 * delta_t * dt_s;
        }
    }

    [[nodiscard]] BatteryDegradationState state() const noexcept {
        const double soh = std::clamp(available_capacity_ah_ / spec_.nominal_capacity_ah, 0.0, 1.0);
        return BatteryDegradationState{
            current_resistance_ohms_,
            available_capacity_ah_,
            soh,
            shock_events_
        };
    }

    [[nodiscard]] double internal_resistance_ohms() const noexcept { return current_resistance_ohms_; }
    [[nodiscard]] double available_capacity_ah() const noexcept { return available_capacity_ah_; }

private:
    BatteryStressSpec spec_;
    double current_resistance_ohms_{0.025};
    double available_capacity_ah_{2.5};
    uint64_t shock_events_{0};
};

// =============================================================================
// Multi-Circuit Power Distribution Bus (PH4-10.03.006)
// =============================================================================
struct PowerRailSpec {
    std::string rail_name{"12V_Main"};
    double nominal_voltage_v{12.0};
    double internal_resistance_ohms{0.02};   ///< Battery / PSU internal resistance
    double wiring_resistance_ohms{0.01};     ///< Cable harness resistance
    double max_current_a{30.0};              ///< Fuse / Breaker trip current (30 A)
};

class PowerDistributionBus {
public:
    explicit PowerDistributionBus(PowerRailSpec spec = {}) : spec_(spec) {}

    void register_branch(const std::string& branch_id, double current_draw_a) noexcept {
        branch_currents_[branch_id] = current_draw_a;
    }

    void update_branch(const std::string& branch_id, double current_draw_a) noexcept {
        branch_currents_[branch_id] = current_draw_a;
    }

    [[nodiscard]] double total_current_draw_a() const noexcept {
        double sum = 0.0;
        for (const auto& [_, i] : branch_currents_) {
            sum += i;
        }
        return sum;
    }

    [[nodiscard]] double terminal_voltage_v() noexcept {
        const double i_tot = total_current_draw_a();

        // Check fuse trip
        if (i_tot > spec_.max_current_a) {
            fuse_tripped_ = true;
            return 0.0;
        }
        if (fuse_tripped_) {
            return 0.0;
        }

        // Terminal voltage with voltage droop: V_term = V_nom - I_tot * (R_int + R_wire)
        const double r_total = spec_.internal_resistance_ohms + spec_.wiring_resistance_ohms;
        return std::max(0.0, spec_.nominal_voltage_v - i_tot * r_total);
    }

    [[nodiscard]] bool is_fuse_tripped() const noexcept { return fuse_tripped_; }
    void reset_fuse() noexcept { fuse_tripped_ = false; }
    [[nodiscard]] const PowerRailSpec& spec() const noexcept { return spec_; }

private:
    PowerRailSpec spec_;
    std::unordered_map<std::string, double> branch_currents_;
    bool fuse_tripped_{false};
};

} // namespace shunya::cosim
