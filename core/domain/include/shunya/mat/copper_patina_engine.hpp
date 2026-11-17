#pragma once

#include "shunya/mat/epistemic_value.hpp"
#include "shunya/mat/iron_oxidation_engine.hpp"

namespace shunya::mat {

struct CopperPatinaState {
    double exposure_time_hours{0.0};
    double oxide_layer_thickness_nm{0.5}; // Fresh copper naturally has ~0.5 - 1.0 nm native Cu2O
    double constriction_resistance_ohms{0.0};
    double film_resistance_ohms{0.0};
    double total_contact_resistance_ohms{0.0};
    double contact_spot_radius_um{0.0};
    bool is_film_punctured_by_force{false};
    double contact_degradation_ratio{1.0}; // Ratio of current contact R to virgin R_0
};

/**
 * @brief Copper surface patina/oxidation and electrical contact resistance (Holm theory)
 *        degradation modeling engine.
 * (Roadmap task PH4-03.04.002)
 */
class CopperPatinaEngine {
public:
    static constexpr double CU_HARDNESS_PA = 4.9e8;          // Copper Vickers hardness ~50 HV (~490 MPa)
    static constexpr double CU_BULK_RESISTIVITY_OHM_M = 1.68e-8;
    static constexpr double CU2O_OXIDE_RESISTIVITY_OHM_M = 1.0e4; // Cuprous oxide film resistivity
    static constexpr double TUNNELING_CUTOFF_THICKNESS_NM = 2.5;  // Quantum tunneling threshold

    /**
     * @brief Computes oxide thickness [nm] via Cabrera-Mott logarithmic growth at ambient temperatures
     *        transitioning to Wagner parabolic growth at high temperatures.
     */
    [[nodiscard]] static double compute_oxide_thickness_nm(
        double exposure_hours,
        double temperature_k = 293.15,
        double relative_humidity_pct = 50.0) noexcept;

    /**
     * @brief Computes Holm electrical contact resistance R_c = R_constriction + R_film [Ohms]
     *        under mechanical normal clamping force F_N [Newtons].
     */
    [[nodiscard]] static CopperPatinaState evaluate_contact_state(
        double exposure_hours,
        double normal_force_newtons,
        double temperature_k = 293.15,
        double relative_humidity_pct = 50.0,
        bool apply_mechanical_wipe = false) noexcept;

    /**
     * @brief Returns total contact resistance as an EpistemicScalar.
     */
    [[nodiscard]] static EpistemicScalar evaluate_contact_resistance(
        double exposure_hours,
        double normal_force_newtons,
        double temperature_k = 293.15,
        double relative_humidity_pct = 50.0) noexcept;
};

} // namespace shunya::mat
