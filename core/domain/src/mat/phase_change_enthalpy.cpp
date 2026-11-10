#include "shunya/mat/phase_change_enthalpy.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

double PhaseChangeEnthalpyModel::liquid_fraction(double temp_k) const noexcept {
    if (temp_k <= params_.solidus_temp_k) return 0.0;
    if (temp_k >= params_.liquidus_temp_k) return 1.0;
    const double span = params_.liquidus_temp_k - params_.solidus_temp_k;
    if (span <= 1e-12) return 0.5;
    const double u = (temp_k - params_.solidus_temp_k) / span;
    if (params_.profile == MushyFractionProfile::Linear) {
        return u;
    }
    return u * u * (3.0 - 2.0 * u);
}

double PhaseChangeEnthalpyModel::d_liquid_fraction_dT(double temp_k) const noexcept {
    if (temp_k <= params_.solidus_temp_k || temp_k >= params_.liquidus_temp_k) return 0.0;
    const double span = params_.liquidus_temp_k - params_.solidus_temp_k;
    if (span <= 1e-12) return 0.0;
    if (params_.profile == MushyFractionProfile::Linear) {
        return 1.0 / span;
    }
    const double u = (temp_k - params_.solidus_temp_k) / span;
    return (6.0 * u * (1.0 - u)) / span;
}

double PhaseChangeEnthalpyModel::apparent_cp_j_kg_k(double temp_k) const noexcept {
    const double fl = liquid_fraction(temp_k);
    const double cp_base = (1.0 - fl) * params_.cp_solid_j_kg_k + fl * params_.cp_liquid_j_kg_k;
    return cp_base + params_.latent_heat_fusion_j_kg * d_liquid_fraction_dT(temp_k);
}

double PhaseChangeEnthalpyModel::specific_enthalpy_j_kg(double temp_k, double ref_temp_k) const noexcept {
    // 64-point composite Simpson / midpoint integration relative to ref_temp_k
    if (std::abs(temp_k - ref_temp_k) <= 1e-12) return 0.0;
    const int steps = 128;
    const double dt = (temp_k - ref_temp_k) / static_cast<double>(steps);
    double sum = 0.0;
    for (int i = 0; i < steps; ++i) {
        const double tm = ref_temp_k + (static_cast<double>(i) + 0.5) * dt;
        sum += apparent_cp_j_kg_k(tm) * dt;
    }
    return sum;
}

double PhaseChangeEnthalpyModel::neumann_solidification_depth_m(double wall_temp_k,
                                                                 double time_s) const noexcept {
    const double tm = 0.5 * (params_.solidus_temp_k + params_.liquidus_temp_k);
    if (wall_temp_k >= tm || time_s <= 0.0) return 0.0;
    const double st = (params_.cp_solid_j_kg_k * (tm - wall_temp_k)) /
                      std::max(1.0, params_.latent_heat_fusion_j_kg);
    const double rhs = st / std::sqrt(3.14159265358979323846);

    // Bisection for transcendental Neumann eigenvalue lambda * exp(lambda^2) * erf(lambda) = rhs
    double lo = 0.0;
    double hi = 2.0;
    for (int iter = 0; iter < 60; ++iter) {
        const double mid = 0.5 * (lo + hi);
        const double f = mid * std::exp(mid * mid) * std::erf(mid);
        if (f < rhs) lo = mid;
        else hi = mid;
    }
    const double lambda = 0.5 * (lo + hi);
    const double alpha_s = params_.k_solid_w_mk /
                           (params_.solid_density_kg_m3 * params_.cp_solid_j_kg_k);
    return 2.0 * lambda * std::sqrt(alpha_s * time_s);
}

PhaseChangeEnthalpyModel PhaseChangeEnthalpyModel::water_ice() {
    return PhaseChangeEnthalpyModel({
        272.65, 273.65, 2108.0, 4184.0, 334000.0, 2.22, 917.0,
        MushyFractionProfile::SmoothStepCubic
    });
}

PhaseChangeEnthalpyModel PhaseChangeEnthalpyModel::aluminum_melting() {
    return PhaseChangeEnthalpyModel({
        913.0, 933.47, 910.0, 1180.0, 397000.0, 200.0, 2700.0,
        MushyFractionProfile::SmoothStepCubic
    });
}

} // namespace shunya::mat
