#pragma once

#include "shunya/mat/rheology_models.hpp"

namespace shunya::mat {

/**
 * @brief Evaluates temperature- and shear-rate-dependent fluid viscosity mu(T, gamma_dot).
 */
class ViscosityEvaluator {
public:
    explicit ViscosityEvaluator(RheologyCurveParameters params) : params_(params) {}

    [[nodiscard]] double base_viscosity_pa_s(double temp_k) const noexcept;
    [[nodiscard]] double effective_viscosity_pa_s(double temp_k, double shear_rate_s_inv = 1.0) const noexcept;

    [[nodiscard]] static ViscosityEvaluator liquid_water();
    [[nodiscard]] static ViscosityEvaluator sae_10w30_oil();
    [[nodiscard]] static ViscosityEvaluator liquid_gallium();

private:
    RheologyCurveParameters params_;
};

} // namespace shunya::mat
