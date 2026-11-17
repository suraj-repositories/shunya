#pragma once

#include "shunya/ecad3d/component_manifest.hpp"

#include <string_view>

namespace shunya::lib30 {

/**
 * @brief Factory producing fully-qualified transistor components for Submodule PH4-07.03.
 */
class TransistorComponentsFactory {
public:
    /**
     * @brief Component 11: NPN Bipolar Junction Transistor (Roadmap task PH4-07.03.001)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_npn_bjt(
        double beta_current_gain = 200.0,
        double max_collector_current_a = 0.20,
        double max_v_ce_volts = 40.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 12: PNP Bipolar Junction Transistor (Roadmap task PH4-07.03.002)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_pnp_bjt(
        double beta_current_gain = 200.0,
        double max_collector_current_a = 0.20,
        double max_v_ce_volts = 40.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 13: N-Channel MOSFET (Roadmap task PH4-07.03.003)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_n_mosfet(
        double v_threshold_volts = 2.5,
        double r_ds_on_ohms = 0.028,
        double max_drain_current_a = 30.0,
        double max_v_ds_volts = 60.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 14: P-Channel MOSFET (Roadmap task PH4-07.03.004)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_p_mosfet(
        double v_threshold_volts = -2.5,
        double r_ds_on_ohms = 0.055,
        double max_drain_current_a = 20.0,
        double max_v_ds_volts = 55.0,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 15: N-Channel JFET (Roadmap task PH4-07.03.005)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_n_jfet(
        double i_dss_amperes = 10.0e-3, // 10 mA
        double v_pinchoff_volts = -3.0,
        double max_drain_current_a = 0.020,
        std::string_view standard = "IEEE"
    );

    /**
     * @brief Component 16: P-Channel JFET (Roadmap task PH4-07.03.006)
     */
    [[nodiscard]] static ecad3d::ComponentManifest create_p_jfet(
        double i_dss_amperes = 8.0e-3, // 8 mA
        double v_pinchoff_volts = 2.5,
        double max_drain_current_a = 0.015,
        std::string_view standard = "IEEE"
    );
};

} // namespace shunya::lib30
