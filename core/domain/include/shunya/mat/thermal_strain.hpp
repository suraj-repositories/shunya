#pragma once

#include "shunya/mat/monotonic_spline.hpp"

namespace shunya::mat {

struct BimetallicStripResult {
    double curvature_per_m{0.0};
    double tip_deflection_m{0.0};
};

/**
 * @brief Evaluates instantaneous CTE alpha(T), integrated thermal strain, thermal stress, and bimetallic deflection.
 */
class ThermalStrainEvaluator {
public:
    explicit ThermalStrainEvaluator(double constant_alpha_per_k, double ref_temp_k = 293.15);
    ThermalStrainEvaluator(MonotonicCubicSpline alpha_spline, double ref_temp_k = 293.15);

    [[nodiscard]] double reference_temp_k() const noexcept { return ref_temp_k_; }
    [[nodiscard]] double instantaneous_alpha_per_k(double temp_k) const noexcept;

    /**
     * @brief Computes integral from T_ref to T of alpha(tau) d tau. Returns 0.0 at T = T_ref.
     */
    [[nodiscard]] double linear_thermal_strain(double temp_k) const noexcept;
    [[nodiscard]] double volumetric_thermal_strain(double temp_k) const noexcept;

    /**
     * @brief Constrained axial thermal stress sigma_th = -E * epsilon_th(T) [Pa].
     */
    [[nodiscard]] double constrained_thermal_stress_pa(double youngs_modulus_pa, double temp_k) const noexcept;

    /**
     * @brief Timoshenko (1925) bimetallic strip curvature and cantilever tip deflection.
     */
    [[nodiscard]] static BimetallicStripResult compute_bimetallic_deflection(
        double length_m,
        double thickness_1_m, double youngs_1_pa, double alpha_1_per_k,
        double thickness_2_m, double youngs_2_pa, double alpha_2_per_k,
        double delta_temp_k) noexcept;

private:
    bool use_spline_{false};
    double const_alpha_{12e-6};
    double ref_temp_k_{293.15};
    MonotonicCubicSpline alpha_spline_;
};

} // namespace shunya::mat
