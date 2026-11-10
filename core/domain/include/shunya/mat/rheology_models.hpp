#pragma once

#include <cstdint>

namespace shunya::mat {

enum class ViscosityTempModel : uint8_t {
    Andrade,               // ln(mu) = A + B / T
    VogelFulcherTammann    // mu = mu_0 * exp(B / (T - T_0))
};

struct RheologyCurveParameters {
    ViscosityTempModel temp_model{ViscosityTempModel::VogelFulcherTammann};
    double param_a{0.02939e-3}; // Andrade A (ln(Pa*s)) or VFT pre-exponential mu_0 (Pa*s)
    double param_b{507.88};     // Andrade B (K) or VFT B (K)
    double param_t0{149.3};     // VFT reference T_0 (K)
    double power_law_n{1.0};    // 1.0 = Newtonian; < 1.0 = Shear thinning
    double ref_shear_rate_s_inv{1.0};
};

} // namespace shunya::mat
