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

} // namespace shunya::cosim
