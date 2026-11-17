#include "shunya/mat/shomate_engine.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

EpistemicScalar ShomateEngine::compute_molar_heat_capacity(
    double temp_k, const ShomateCoefficients& coeff) noexcept {
    const double t_k = std::clamp(temp_k, 50.0, 10000.0);
    const double t = t_k / 1000.0;
    const double t2 = t * t;
    const double t3 = t2 * t;

    // Cp = A + B*t + C*t^2 + D*t^3 + E/t^2 [J / (mol * K)]
    const double cp_m = coeff.A + coeff.B * t + coeff.C * t2 + coeff.D * t3 + (coeff.E / std::max(1e-8, t2));

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "NIST Shomate Polynomial Heat Capacity Cp(t)";
    meta.reference_standard = "NIST Chemistry WebBook SRD 69";
    meta.relative_uncertainty_pct = coeff.is_temperature_in_range(temp_k) ? 1.0 : 4.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(cp_m, meta);
}

EpistemicScalar ShomateEngine::compute_specific_heat_capacity(
    double temp_k, const ShomateCoefficients& coeff) noexcept {
    auto molar_cp = compute_molar_heat_capacity(temp_k, coeff);
    const double m_mol = coeff.molar_mass_g_mol > 0.0 ? coeff.molar_mass_g_mol : 1.0;
    // Conversion: [J / (mol * K)] / [kg / mol] = [J / (mol * K)] / (M_g / 1000) = Cp_m * 1000 / M_g [J / (kg * K)]
    const double cp_specific = (molar_cp.value / m_mol) * 1000.0;
    return EpistemicScalar(cp_specific, molar_cp.classification);
}

EpistemicScalar ShomateEngine::compute_molar_enthalpy_increment(
    double temp_k, const ShomateCoefficients& coeff) noexcept {
    const double t_k = std::clamp(temp_k, 50.0, 10000.0);
    const double t = t_k / 1000.0;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double t4 = t3 * t;

    // H - H_298 = A*t + B*t^2/2 + C*t^3/3 + D*t^4/4 - E/t + F - H [kJ / mol]
    const double delta_h = coeff.A * t +
                           (coeff.B * t2 / 2.0) +
                           (coeff.C * t3 / 3.0) +
                           (coeff.D * t4 / 4.0) -
                           (coeff.E / std::max(1e-4, t)) +
                           coeff.F - coeff.H;

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "NIST Shomate Enthalpy Increment (H - H_298.15)";
    meta.reference_standard = "NIST Chemistry WebBook SRD 69";
    meta.relative_uncertainty_pct = coeff.is_temperature_in_range(temp_k) ? 1.5 : 5.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(delta_h, meta);
}

EpistemicScalar ShomateEngine::compute_molar_entropy(
    double temp_k, const ShomateCoefficients& coeff) noexcept {
    const double t_k = std::clamp(temp_k, 50.0, 10000.0);
    const double t = t_k / 1000.0;
    const double t2 = t * t;
    const double t3 = t2 * t;

    // S = A*ln(t) + B*t + C*t^2/2 + D*t^3/3 - E/(2*t^2) + G [J / (mol * K)]
    const double s_mol = coeff.A * std::log(std::max(1e-4, t)) +
                         coeff.B * t +
                         (coeff.C * t2 / 2.0) +
                         (coeff.D * t3 / 3.0) -
                         (coeff.E / (2.0 * std::max(1e-8, t2))) +
                         coeff.G;

    CalculationClassification meta;
    meta.tier = CalculationTier::EmpiricalFit;
    meta.source_model_or_formula = "NIST Shomate Standard Entropy S(T)";
    meta.reference_standard = "NIST Chemistry WebBook SRD 69";
    meta.relative_uncertainty_pct = coeff.is_temperature_in_range(temp_k) ? 1.5 : 5.0;
    meta.is_empirically_validated = true;
    return EpistemicScalar(s_mol, meta);
}

ShomateCoefficients ShomateEngine::copper_solid_nist() noexcept {
    ShomateCoefficients c;
    c.substance_name = "Copper Solid (Cu)";
    c.t_min_k = 298.15;
    c.t_max_k = 1357.77;
    c.molar_mass_g_mol = 63.546;
    c.A = 17.72891;
    c.B = 28.09870;
    c.C = -31.25289;
    c.D = 13.97243;
    c.E = 0.068611;
    c.F = -6.056591;
    c.G = 67.89300;
    c.H = 0.0;
    return c;
}

ShomateCoefficients ShomateEngine::aluminum_solid_nist() noexcept {
    ShomateCoefficients c;
    c.substance_name = "Aluminum Solid (Al)";
    c.t_min_k = 298.15;
    c.t_max_k = 933.47;
    c.molar_mass_g_mol = 26.9815385;
    c.A = 28.29170;
    c.B = -4.364230;
    c.C = 17.70270;
    c.D = -10.98500;
    c.E = -0.063230;
    c.F = -8.995900;
    c.G = 56.49470;
    c.H = 0.0;
    return c;
}

ShomateCoefficients ShomateEngine::iron_solid_nist() noexcept {
    ShomateCoefficients c;
    c.substance_name = "Iron Solid Alpha (Fe)";
    c.t_min_k = 298.15;
    c.t_max_k = 1042.0;
    c.molar_mass_g_mol = 55.845;
    c.A = 18.42868;
    c.B = 24.64301;
    c.C = -8.913711;
    c.D = 9.664714;
    c.E = -0.012643;
    c.F = -6.573024;
    c.G = 42.51486;
    c.H = 0.0;
    return c;
}

} // namespace shunya::mat
