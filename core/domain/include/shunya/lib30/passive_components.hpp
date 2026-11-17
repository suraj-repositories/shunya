#pragma once

#include "shunya/ecad3d/component_manifest.hpp"

#include <string_view>

namespace shunya::lib30 {

/**
 * @brief Factory producing fully-qualified standard electronic components for Submodule PH4-07.01.
 */
class PassiveComponentsFactory {
public:
    /**
     * @brief Component 1: Fixed Resistor (Roadmap task PH4-07.01.001)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_resistor(
        double resistance_ohms = 1000.0,
        double tolerance_percent = 5.0,
        double power_rating_watts = 0.25,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 2: Variable Resistor / Rheostat (Roadmap task PH4-07.01.002)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_rheostat(
        double r_min_ohms = 0.0,
        double r_max_ohms = 100.0,
        double wiper_position_ratio = 0.5,
        double power_rating_watts = 50.0
    );

    /**
     * @brief Component 3: Potentiometer (Roadmap task PH4-07.01.003)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_potentiometer(
        double total_resistance_ohms = 10000.0,
        double wiper_ratio = 0.5,
        double power_rating_watts = 0.5
    );

    /**
     * @brief Component 4: Ceramic Capacitor (Roadmap task PH4-07.01.004)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_ceramic_capacitor(
        double capacitance_farads = 100.0e-9, // 100 nF
        double max_voltage_volts = 50.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 5: Polarized Electrolytic Capacitor (Roadmap task PH4-07.01.005)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_electrolytic_capacitor(
        double capacitance_farads = 100.0e-6, // 100 uF
        double max_voltage_volts = 25.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 6: Inductor (Roadmap task PH4-07.01.006)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_inductor(
        double inductance_henries = 10.0e-3, // 10 mH
        double max_current_amperes = 2.0,
        std::string_view standard = "IEEE"
    );
};

} // namespace shunya::lib30
