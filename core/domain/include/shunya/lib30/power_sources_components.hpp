#pragma once

#include "shunya/ecad3d/component_manifest.hpp"

#include <string_view>

namespace shunya::lib30 {

/**
 * @brief Factory producing fully-qualified power sources, protection, and passive reference devices for Submodule PH4-07.05.
 */
class PowerSourcesProtectionFactory {
public:
    /**
     * @brief Component 21: Chemical Battery (Roadmap task PH4-07.05.001)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_battery_cell(
        double nominal_voltage_volts = 3.70,
        double capacity_amp_hours = 2.60,
        double internal_resistance_ohms = 0.040,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 22: Ideal DC Voltage Source (Roadmap task PH4-07.05.002)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_dc_voltage_source(
        double setpoint_voltage_volts = 12.0,
        double current_limit_amperes = 5.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 23: AC Voltage Source (Roadmap task PH4-07.05.003)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_ac_voltage_source(
        double peak_voltage_volts = 10.0,
        double frequency_hz = 1000.0,
        double phase_degrees = 0.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 24: Earth / Ground Reference (Roadmap task PH4-07.05.004)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_ground_reference(
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 25: Fast-Acting Fuse (Roadmap task PH4-07.05.005)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_fuse(
        double rated_current_amperes = 2.0,
        double cold_resistance_ohms = 0.050,
        double max_voltage_volts = 250.0,
        std::string_view standard = "IEEE"
    );
};

} // namespace shunya::lib30
