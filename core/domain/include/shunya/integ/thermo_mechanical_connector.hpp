#pragma once

#include "shunya/core/base/result.hpp"

namespace shunya::integ {

struct BimaterialBeamSpec {
    double length_m{0.20};
    double width_m{0.01};
    double layer1_thickness_m{0.001};
    double layer2_thickness_m{0.001};
    double e1_pa{200e9};       // Steel
    double e2_pa{70e9};        // Aluminum
    double alpha1_per_k{11e-6};
    double alpha2_per_k{23e-6};
};

struct SlenderColumnBucklingSpec {
    double length_m{1.0};
    double cross_section_area_m2{1e-4};
    double second_moment_i_m4{8.333e-10}; // Rectangular / circular I
    double youngs_modulus_pa{200e9};
    double cte_alpha_per_k{12e-6};
    double end_factor_k{1.0}; // 1.0 = pinned-pinned, 0.5 = fixed-fixed
};

/**
 * @brief Bidirectional thermo-mechanical connector: Timoshenko bimetal deflection, Euler thermal buckling, and Taylor-Quinney plastic heating.
 */
class ThermoMechanicalConnector {
public:
    static constexpr double kDefaultTaylorQuinneyBeta = 0.90;

    [[nodiscard]] static double bimetallic_tip_deflection_m(
        const BimaterialBeamSpec& spec,
        double delta_temp_k) noexcept;

    [[nodiscard]] static double euler_critical_buckling_delta_t_k(
        const SlenderColumnBucklingSpec& spec) noexcept;

    [[nodiscard]] static core::base::Result<double> axial_thermal_stress_pa(
        const SlenderColumnBucklingSpec& spec,
        double delta_temp_k,
        bool& out_buckled);

    [[nodiscard]] static double taylor_quinney_plastic_temp_rise_k(
        double yield_stress_pa,
        double plastic_strain_increment,
        double density_kg_m3,
        double specific_heat_j_kg_k,
        double taylor_quinney_beta = kDefaultTaylorQuinneyBeta) noexcept;
};

} // namespace shunya::integ
