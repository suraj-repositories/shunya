#pragma once

#include <cmath>
#include <string>
#include <string_view>

namespace shunya::mat {

/**
 * @brief Structured Mechanical Property Model:
 *        Mass density, Young's modulus E, Poisson's ratio nu, shear modulus G,
 *        bulk modulus K, yield/tensile/compressive strengths, Mohs/Vickers/Brinell hardness,
 *        and fracture toughness K1c with elastic wave speeds.
 * (Roadmap task PH4-03.02.004)
 */
struct MechanicalPropertyModel {
    double density_kg_m3{8920.0};              // Mass density rho [kg/m^3]
    double young_modulus_pa{110.0e9};          // Young's modulus E [Pa] (110 GPa for Cu)
    double poisson_ratio{0.34};                // Poisson's ratio nu (dimensionless)
    double shear_modulus_pa{48.0e9};           // Shear modulus G [Pa] (48 GPa for Cu)
    double bulk_modulus_pa{140.0e9};           // Bulk modulus K [Pa] (140 GPa for Cu)

    // Strength & Plasticity
    double yield_strength_pa{70.0e6};          // sigma_y [Pa] (70 MPa for annealed Cu)
    double ultimate_tensile_strength_pa{220.0e6}; // sigma_u [Pa] (220 MPa for Cu)
    double compressive_strength_pa{250.0e6};   // sigma_c [Pa]
    double elongation_at_break_pct{45.0};      // Ductility [%]

    // Hardness Metrics
    double hardness_mohs{3.0};                 // Mohs scale [1..10]
    double hardness_vickers_hv{50.0};          // Vickers HV [kgf/mm^2]
    double hardness_brinell_hb{45.0};          // Brinell HBW

    // Fracture Mechanics
    double fracture_toughness_k1c_mpa_m05{75.0}; // K_1c [MPa * sqrt(m)]

    /**
     * @brief Computes isotropic shear modulus G = E / (2 * (1 + nu)).
     */
    [[nodiscard]] double derived_shear_modulus_pa() const noexcept;

    /**
     * @brief Computes isotropic bulk modulus K = E / (3 * (1 - 2*nu)).
     */
    [[nodiscard]] double derived_bulk_modulus_pa() const noexcept;

    /**
     * @brief Computes longitudinal acoustic wave speed c_L = sqrt((K + 4/3*G) / rho) [m/s].
     */
    [[nodiscard]] double longitudinal_sound_speed_m_s() const noexcept;

    /**
     * @brief Computes transverse/shear acoustic wave speed c_S = sqrt(G / rho) [m/s].
     */
    [[nodiscard]] double shear_sound_speed_m_s() const noexcept;

    /**
     * @brief Computes critical energy release rate G_1c = K_1c^2 / E [J / m^2].
     */
    [[nodiscard]] double critical_energy_release_rate_j_m2() const noexcept;

    /**
     * @brief Checks thermodynamic elastic stability: E > 0, -1 < nu < 0.5, G > 0, K > 0.
     */
    [[nodiscard]] bool check_elastic_stability() const noexcept;

    [[nodiscard]] std::string to_summary_string() const;
};

} // namespace shunya::mat
