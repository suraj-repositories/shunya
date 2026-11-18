#pragma once

#include "shunya/ecad3d/component_manifest.hpp"

#include <string_view>

namespace shunya::lib30 {

/**
 * @brief Factory producing actuators, electromechanical transducers, and integrated circuits for Submodule PH4-07.06.
 */
class ActuatorsTransducersFactory {
public:
    /**
     * @brief Component 28: Piezoelectric Buzzer (Roadmap task PH4-07.06.001)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_piezo_buzzer(
        double resonant_frequency_hz = 4000.0,
        double rated_voltage_volts = 12.0,
        double sound_pressure_level_db = 85.0,
        std::string_view standard = "IEEE"
    );

    [[nodiscard]] static double evaluate_spl_output(
        double operating_voltage_v,
        double rated_voltage_v,
        double resonant_frequency_hz,
        double actual_frequency_hz
    ) noexcept;

    /**
     * @brief Component 29: DC Permanent Magnet Motor (Roadmap task PH4-07.06.002)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_dc_motor(
        double rated_voltage_volts = 12.0,
        double armature_resistance_ohms = 1.5,
        double armature_inductance_henries = 1.2e-3,
        double torque_constant_nm_per_a = 0.025,
        double back_emf_constant_v_per_rad_s = 0.025,
        std::string_view standard = "IEEE"
    );

    [[nodiscard]] static double calculate_motor_stall_torque(
        double applied_voltage_v,
        double armature_resistance_ohms,
        double torque_constant_nm_per_a
    ) noexcept;

    [[nodiscard]] static double calculate_motor_no_load_speed_rad_s(
        double applied_voltage_v,
        double back_emf_constant_v_per_rad_s
    ) noexcept;

    /**
     * @brief Component 30: Operational Amplifier (LM741 / Rail-to-Rail) (Roadmap task PH4-07.06.003)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_opamp(
        double open_loop_gain = 1.0e5,
        double input_resistance_ohms = 2.0e6,
        double output_resistance_ohms = 75.0,
        double positive_rail_volts = 15.0,
        double negative_rail_volts = -15.0,
        std::string_view standard = "IEEE"
    );

    [[nodiscard]] static double evaluate_opamp_output(
        double v_pos_in,
        double v_neg_in,
        double open_loop_gain,
        double v_pos_rail,
        double v_neg_rail
    ) noexcept;
};

} // namespace shunya::lib30
