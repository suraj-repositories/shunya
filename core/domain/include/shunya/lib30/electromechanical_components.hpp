#pragma once

#include "shunya/ecad3d/component_manifest.hpp"

#include <string_view>

namespace shunya::lib30 {

/**
 * @brief Factory producing fully-qualified electromechanical & switching devices for Submodule PH4-07.04.
 */
class ElectromechanicalComponentsFactory {
public:
    /**
     * @brief Component 17: Push Button (Momentary NO) (Roadmap task PH4-07.04.001)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_push_button(
        double contact_resistance_ohms = 0.05,
        double max_current_amperes = 0.50,
        double max_voltage_volts = 50.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 18: SPST Toggle Switch (Roadmap task PH4-07.04.002)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_toggle_switch(
        double contact_resistance_ohms = 0.01,
        double max_current_amperes = 5.0,
        double max_voltage_volts = 250.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 19: SPDT Slide Switch (Roadmap task PH4-07.04.003)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_slide_switch(
        double contact_resistance_ohms = 0.02,
        double max_current_amperes = 1.0,
        double max_voltage_volts = 50.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 20: Electromechanical Relay (Roadmap task PH4-07.04.004)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_relay(
        double coil_nominal_voltage_v = 12.0,
        double coil_resistance_ohms = 400.0,
        double contact_rating_current_a = 10.0,
        double contact_rating_voltage_v = 250.0,
        std::string_view standard = "IEEE"
    );
};

} // namespace shunya::lib30
