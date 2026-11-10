#include "shunya/mat/composite_homogenizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shunya::mat {

namespace {

core::base::Error make_homog_error(std::string msg) {
    return core::base::Error{
        .code = 4230,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

double hs_bulk(const std::vector<ElasticPhase>& phases, double g_ref) noexcept {
    const double shift = (4.0 / 3.0) * g_ref;
    double inv_sum = 0.0;
    for (const auto& p : phases) {
        if (p.volume_fraction <= 0.0) continue;
        inv_sum += p.volume_fraction / (p.bulk_modulus_gpa + shift);
    }
    if (inv_sum <= 0.0) return 0.0;
    return (1.0 / inv_sum) - shift;
}

double hs_shear(const std::vector<ElasticPhase>& phases, double k_ref, double g_ref) noexcept {
    const double denom = k_ref + 2.0 * g_ref;
    if (denom <= 0.0) return 0.0;
    const double zeta = (g_ref / 6.0) * ((9.0 * k_ref + 8.0 * g_ref) / denom);
    double inv_sum = 0.0;
    for (const auto& p : phases) {
        if (p.volume_fraction <= 0.0) continue;
        inv_sum += p.volume_fraction / (p.shear_modulus_gpa + zeta);
    }
    if (inv_sum <= 0.0) return 0.0;
    return (1.0 / inv_sum) - zeta;
}

} // namespace

core::base::Result<HomogenizationBounds> CompositeHomogenizer::evaluate(
    const std::vector<ElasticPhase>& phases) {
    if (phases.empty()) {
        return make_homog_error("At least one constituent phase is required for homogenization");
    }

    double f_sum = 0.0;
    for (const auto& p : phases) {
        if (p.volume_fraction < -1e-9) {
            return make_homog_error("Phase volume fraction cannot be negative");
        }
        if (p.bulk_modulus_gpa <= 0.0 || p.shear_modulus_gpa <= 0.0) {
            return make_homog_error("Bulk and shear moduli must be strictly positive");
        }
        f_sum += p.volume_fraction;
    }
    if (std::abs(f_sum - 1.0) > 1e-4) {
        return make_homog_error("Phase volume fractions must sum to 1.0");
    }

    HomogenizationBounds out;
    double inv_k_sum = 0.0;
    double inv_g_sum = 0.0;
    double k_min = std::numeric_limits<double>::infinity();
    double k_max = 0.0;
    double g_min = std::numeric_limits<double>::infinity();
    double g_max = 0.0;

    for (const auto& p : phases) {
        const double f = p.volume_fraction / f_sum;
        out.bulk_voigt_gpa += f * p.bulk_modulus_gpa;
        out.shear_voigt_gpa += f * p.shear_modulus_gpa;
        inv_k_sum += f / p.bulk_modulus_gpa;
        inv_g_sum += f / p.shear_modulus_gpa;
        if (p.volume_fraction > 0.0) {
            k_min = std::min(k_min, p.bulk_modulus_gpa);
            k_max = std::max(k_max, p.bulk_modulus_gpa);
            g_min = std::min(g_min, p.shear_modulus_gpa);
            g_max = std::max(g_max, p.shear_modulus_gpa);
        }
    }
    out.bulk_reuss_gpa = 1.0 / inv_k_sum;
    out.shear_reuss_gpa = 1.0 / inv_g_sum;

    out.bulk_hs_lower_gpa = hs_bulk(phases, g_min);
    out.bulk_hs_upper_gpa = hs_bulk(phases, g_max);
    out.shear_hs_lower_gpa = hs_shear(phases, k_min, g_min);
    out.shear_hs_upper_gpa = hs_shear(phases, k_max, g_max);

    out.bulk_hill_gpa = 0.5 * (out.bulk_voigt_gpa + out.bulk_reuss_gpa);
    out.shear_hill_gpa = 0.5 * (out.shear_voigt_gpa + out.shear_reuss_gpa);

    const double k = out.bulk_hill_gpa;
    const double g = out.shear_hill_gpa;
    out.effective_youngs_gpa = (9.0 * k * g) / (3.0 * k + g);
    out.effective_poisson = (3.0 * k - 2.0 * g) / (2.0 * (3.0 * k + g));
    return out;
}

} // namespace shunya::mat
