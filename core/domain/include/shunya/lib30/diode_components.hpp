#pragma once

#include "shunya/ecad3d/component_manifest.hpp"

#include <string_view>

namespace shunya::lib30 {

/**
 * @brief Factory producing fully-qualified diode & optoelectronic components for Submodule PH4-07.02.
 */
class DiodeComponentsFactory {
public:
    /**
     * @brief Component 7: PN Junction Diode (Roadmap task PH4-07.02.001)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_pn_diode(
        double forward_drop_volts = 0.70,
        double max_reverse_voltage_volts = 100.0,
        double max_forward_current_amperes = 1.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 8: Light Emitting Diode (LED) (Roadmap task PH4-07.02.002)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_led(
        std::string_view color = "Red",
        double emission_wavelength_nm = 630.0,
        double forward_voltage_volts = 2.0,
        double max_current_amperes = 0.030,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 9: Zener Diode (Roadmap task PH4-07.02.003)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_zener_diode(
        double zener_voltage_volts = 5.1,
        double power_rating_watts = 0.50,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 10: Schottky Diode (Roadmap task PH4-07.02.004)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_schottky_diode(
        double forward_drop_volts = 0.28,
        double max_reverse_voltage_volts = 40.0,
        double max_forward_current_amperes = 3.0,
        std::string_view standard = "IEEE"
    );
};

} // namespace shunya::lib30
