#include "shunya/mat/viscosity_evaluator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

double ViscosityEvaluator::base_viscosity_pa_s(double temp_k) const noexcept {
    const double T = std::max(1.0, temp_k);
    if (params_.temp_model == ViscosityTempModel::Andrade) {
        return std::exp(params_.param_a + params_.param_b / T);
    }
    const double denom = std::max(1.0, T - params_.param_t0);
    return params_.param_a * std::exp(params_.param_b / denom);
}

double ViscosityEvaluator::effective_viscosity_pa_s(double temp_k,
                                                    double shear_rate_s_inv) const noexcept {
    const double mu0 = base_viscosity_pa_s(temp_k);
    if (std::abs(params_.power_law_n - 1.0) <= 1e-12) {
        return mu0;
    }
    const double gdot = std::max(1e-6, shear_rate_s_inv / std::max(1e-9, params_.ref_shear_rate_s_inv));
    return mu0 * std::pow(gdot, params_.power_law_n - 1.0);
}

ViscosityEvaluator ViscosityEvaluator::liquid_water() {
    // VFT fit accurate across 273.15 K (1.775 mPa*s) .. 373.15 K (0.284 mPa*s)
    return ViscosityEvaluator({
        ViscosityTempModel::VogelFulcherTammann,
        0.02939e-3, 507.88, 149.3, 1.0, 1.0
    });
}

ViscosityEvaluator ViscosityEvaluator::sae_10w30_oil() {
    // Andrade motor oil model (~0.10 Pa*s at 300 K) with mild shear-thinning capability
    return ViscosityEvaluator({
        ViscosityTempModel::Andrade,
        -12.50, 3060.0, 0.0, 0.92, 100.0
    });
}

ViscosityEvaluator ViscosityEvaluator::liquid_gallium() {
    // NIST molten Gallium Andrade parameters (303 K ~ 1.98 mPa*s)
    return ViscosityEvaluator({
        ViscosityTempModel::Andrade,
        -7.82, 485.0, 0.0, 1.0, 1.0
    });
}

} // namespace shunya::mat
