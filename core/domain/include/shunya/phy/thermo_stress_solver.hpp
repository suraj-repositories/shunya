#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/phy/von_mises_evaluator.hpp"

#include <vector>

namespace shunya::phy {

struct BimetallicStripSpec {
    double e1_pa{200e9};       // Layer 1 Young's modulus (e.g. Steel)
    double alpha1_per_k{11e-6};
    double thickness1_m{0.001};

    double e2_pa{110e9};       // Layer 2 Young's modulus (e.g. Brass / Copper)
    double alpha2_per_k{19e-6};
    double thickness2_m{0.001};

    double delta_t_k{100.0};
};

struct BimetallicBendingResult {
    double numerical_curvature_1_m{0.0};
    double analytical_timoshenko_curvature_1_m{0.0};
    double bending_radius_m{0.0};
    double peak_von_mises_pa{0.0};
};

/**
 * @brief Coupled thermo-mechanical expansion and thermal stress equilibrium solver (Hookean + Timoshenko bilayer).
 */
class ThermoMechanicalCoupler {
public:
    /**
     * @brief Computes internal thermal stress sigma_th for a constrained 1D uniaxial bar or 3D rigid-clamped block.
     */
    [[nodiscard]] static SymmetricStressTensor constrained_thermal_stress(
        double youngs_modulus_pa,
        double poissons_ratio,
        double alpha_per_k,
        double delta_t_k,
        bool fully_3d_clamped = false) noexcept;

    /**
     * @brief Solves axial force & bending moment thermo-elastic equilibrium across a layered strip vs Timoshenko formula.
     */
    [[nodiscard]] static core::base::Result<BimetallicBendingResult> solve_bimetallic_curvature(
        const BimetallicStripSpec& spec,
        int fibers_per_layer = 64);

    /**
     * @brief Computes nodal thermal stresses for a coupled 1D/3D bar temperature distribution Delta_T(x) under fixed ends.
     */
    [[nodiscard]] static std::vector<SymmetricStressTensor> solve_clamped_bar_stress_field(
        const std::vector<double>& nodal_temps_k,
        double ref_temp_k,
        double youngs_modulus_pa,
        double alpha_per_k);
};

} // namespace shunya::phy
