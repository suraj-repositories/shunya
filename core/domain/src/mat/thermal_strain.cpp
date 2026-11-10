#include "shunya/mat/thermal_strain.hpp"

#include <cmath>

namespace shunya::mat {

ThermalStrainEvaluator::ThermalStrainEvaluator(double constant_alpha_per_k, double ref_temp_k)
    : use_spline_(false), const_alpha_(constant_alpha_per_k), ref_temp_k_(ref_temp_k) {}

ThermalStrainEvaluator::ThermalStrainEvaluator(MonotonicCubicSpline alpha_spline, double ref_temp_k)
    : use_spline_(true), ref_temp_k_(ref_temp_k), alpha_spline_(std::move(alpha_spline)) {}

double ThermalStrainEvaluator::instantaneous_alpha_per_k(double temp_k) const noexcept {
    return use_spline_ ? alpha_spline_.evaluate(temp_k) : const_alpha_;
}

double ThermalStrainEvaluator::linear_thermal_strain(double temp_k) const noexcept {
    if (std::abs(temp_k - ref_temp_k_) <= 1e-12) return 0.0;
    if (!use_spline_) {
        return const_alpha_ * (temp_k - ref_temp_k_);
    }
    return alpha_spline_.integrate(ref_temp_k_, temp_k);
}

double ThermalStrainEvaluator::volumetric_thermal_strain(double temp_k) const noexcept {
    return 3.0 * linear_thermal_strain(temp_k);
}

double ThermalStrainEvaluator::constrained_thermal_stress_pa(double youngs_modulus_pa, double temp_k) const noexcept {
    return -youngs_modulus_pa * linear_thermal_strain(temp_k);
}

BimetallicStripResult ThermalStrainEvaluator::compute_bimetallic_deflection(
    double length_m,
    double t1, double e1, double alpha1,
    double t2, double e2, double alpha2,
    double delta_temp_k) noexcept {
    const double h = t1 + t2;
    if (h <= 0.0 || t1 <= 0.0 || t2 <= 0.0 || e1 <= 0.0 || e2 <= 0.0) return {};

    const double m = t1 / t2;
    const double n = e1 / e2;
    const double one_p_m = 1.0 + m;
    const double denom = h * (3.0 * one_p_m * one_p_m +
                              (1.0 + m * n) * (m * m + 1.0 / (m * n)));
    const double kappa = (6.0 * (alpha2 - alpha1) * delta_temp_k * one_p_m * one_p_m) / denom;
    return BimetallicStripResult{kappa, 0.5 * kappa * length_m * length_m};
}

} // namespace shunya::mat
