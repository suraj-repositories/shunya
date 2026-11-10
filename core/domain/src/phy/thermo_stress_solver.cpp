#include "shunya/phy/thermo_stress_solver.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

SymmetricStressTensor ThermoMechanicalCoupler::constrained_thermal_stress(
    double youngs_modulus_pa,
    double poissons_ratio,
    double alpha_per_k,
    double delta_t_k,
    bool fully_3d_clamped) noexcept {
    SymmetricStressTensor s;
    const double sig_1d = -youngs_modulus_pa * alpha_per_k * delta_t_k;
    if (!fully_3d_clamped) {
        s.sxx = sig_1d;
    } else {
        const double factor = 1.0 / std::max(1e-6, 1.0 - 2.0 * poissons_ratio);
        s.sxx = sig_1d * factor;
        s.syy = sig_1d * factor;
        s.szz = sig_1d * factor;
    }
    return s;
}

core::base::Result<BimetallicBendingResult> ThermoMechanicalCoupler::solve_bimetallic_curvature(
    const BimetallicStripSpec& spec,
    int fibers_per_layer) {
    if (spec.thickness1_m <= 0.0 || spec.thickness2_m <= 0.0 ||
        spec.e1_pa <= 0.0 || spec.e2_pa <= 0.0 || fibers_per_layer < 4) {
        return core::base::Result<BimetallicBendingResult>::err(
            core::base::Error{72, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Bimetallic strip parameters must be positive", ""});
    }

    // Solve thermo-elastic sectional equilibrium:
    //   [ A  B ] [ eps0  ] = [ N_th ]
    //   [ B  D ] [ kappa ]   [ M_th ]
    double A = 0.0, B = 0.0, D = 0.0, N_th = 0.0, M_th = 0.0;

    auto integrate_layer = [&](double y_start, double thick, double E, double alpha) {
        const double dy = thick / static_cast<double>(fibers_per_layer);
        for (int k = 0; k < fibers_per_layer; ++k) {
            const double y = y_start + (static_cast<double>(k) + 0.5) * dy;
            const double ea = E * dy;
            A += ea;
            B += ea * y;
            D += ea * (y * y + dy * dy / 12.0);
            N_th += ea * alpha * spec.delta_t_k;
            M_th += ea * y * alpha * spec.delta_t_k;
        }
    };

    // Place interface at y = 0: layer 1 in [-t1, 0], layer 2 in [0, +t2]
    integrate_layer(-spec.thickness1_m, spec.thickness1_m, spec.e1_pa, spec.alpha1_per_k);
    integrate_layer(0.0, spec.thickness2_m, spec.e2_pa, spec.alpha2_per_k);

    const double det = A * D - B * B;
    if (std::abs(det) < 1e-24) {
        return core::base::Result<BimetallicBendingResult>::err(
            core::base::Error{73, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
                              "Singular bimetallic bending matrix", ""});
    }

    const double eps0 = (D * N_th - B * M_th) / det;
    const double kappa_num = (-B * N_th + A * M_th) / det;

    // Peak von Mises across bottom, interface, and top fibers
    double peak_vm = 0.0;
    for (double y : {-spec.thickness1_m, 0.0}) {
        const double sig_1 = spec.e1_pa * (eps0 + kappa_num * y - spec.alpha1_per_k * spec.delta_t_k);
        peak_vm = std::max(peak_vm, VonMisesEvaluator::von_mises_stress({sig_1, 0, 0, 0, 0, 0}));
    }
    for (double y : {0.0, spec.thickness2_m}) {
        const double sig_2 = spec.e2_pa * (eps0 + kappa_num * y - spec.alpha2_per_k * spec.delta_t_k);
        peak_vm = std::max(peak_vm, VonMisesEvaluator::von_mises_stress({sig_2, 0, 0, 0, 0, 0}));
    }

    // Closed-form Timoshenko (1925) analytical curvature formula (signed for alpha_2 - alpha_1 when layer 2 is at +y)
    const double h = spec.thickness1_m + spec.thickness2_m;
    const double m = spec.thickness1_m / spec.thickness2_m;
    const double n = spec.e1_pa / spec.e2_pa;
    const double denom = h * (3.0 * (1.0 + m) * (1.0 + m) + (1.0 + m * n) * (m * m + 1.0 / (m * n)));
    const double kappa_timoshenko = 6.0 * (spec.alpha2_per_k - spec.alpha1_per_k) * spec.delta_t_k *
                                    (1.0 + m) * (1.0 + m) / denom;

    BimetallicBendingResult out;
    out.numerical_curvature_1_m = kappa_num;
    out.analytical_timoshenko_curvature_1_m = kappa_timoshenko;
    out.bending_radius_m = (std::abs(kappa_num) > 1e-12) ? (1.0 / std::abs(kappa_num)) : 1e12;
    out.peak_von_mises_pa = peak_vm;
    return core::base::Result<BimetallicBendingResult>::ok(out);
}

std::vector<SymmetricStressTensor> ThermoMechanicalCoupler::solve_clamped_bar_stress_field(
    const std::vector<double>& nodal_temps_k,
    double ref_temp_k,
    double youngs_modulus_pa,
    double alpha_per_k) {
    // Fixed ends force average axial strain == 0 -> sigma = E * (avg_th_strain - local_th_strain) ?
    // Wait: for uniform E & cross-section with clamped ends: sigma_xx is constant along bar = -E * alpha * avg(Delta T)!
    if (nodal_temps_k.empty()) return {};
    double sum_dt = 0.0;
    for (double t : nodal_temps_k) sum_dt += (t - ref_temp_k);
    const double avg_dt = sum_dt / static_cast<double>(nodal_temps_k.size());
    const double sigma_axial = -youngs_modulus_pa * alpha_per_k * avg_dt;

    std::vector<SymmetricStressTensor> out(nodal_temps_k.size());
    for (auto& s : out) s.sxx = sigma_axial;
    return out;
}

} // namespace shunya::phy
