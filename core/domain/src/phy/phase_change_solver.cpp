#include "shunya/phy/phase_change_solver.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

core::base::Result<PhaseChangeResult1D> EnthalpyPhaseChangeSolver::solve_1d_stefan(
    const PhaseChangeDomain1D& domain,
    double total_time_s,
    double cfl_factor) {
    if (domain.num_cells < 4 || domain.length_m <= 0.0 || total_time_s <= 0.0) {
        return core::base::Result<PhaseChangeResult1D>::err(
            core::base::Error{70, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "PhaseChangeDomain1D requires >= 4 cells and positive length/time", ""});
    }

    const size_t N = domain.num_cells;
    const double dx = domain.length_m / static_cast<double>(N);
    const double rho = domain.density_kg_m3;
    const double H_lat = rho * domain.latent_heat_j_kg;
    const double Cs = rho * domain.cp_solid_j_kg_k;
    const double Cl = rho * domain.cp_liquid_j_kg_k;

    // Enthalpy baseline: H = 0 corresponds to solid at T_melt
    std::vector<double> T(N, domain.initial_temp_k);
    std::vector<double> fl(N, (domain.initial_temp_k > domain.t_melt_k) ? 1.0 : 0.0);
    std::vector<double> H(N, 0.0);
    for (size_t i = 0; i < N; ++i) {
        if (domain.initial_temp_k <= domain.t_melt_k) {
            H[i] = Cs * (domain.initial_temp_k - domain.t_melt_k);
        } else {
            H[i] = H_lat + Cl * (domain.initial_temp_k - domain.t_melt_k);
        }
    }

    const double alpha_max = std::max(domain.k_liquid_w_mk / Cl, domain.k_solid_w_mk / Cs);
    const double dt_stable = cfl_factor * (dx * dx) / std::max(1e-18, alpha_max);
    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_stable));
    const double dt = total_time_s / static_cast<double>(std::max<size_t>(1, steps));

    std::vector<double> flux(N + 1, 0.0);
    double absorbed_energy = 0.0;

    for (size_t s = 0; s < steps; ++s) {
        // Effective thermal conductivity per cell
        auto k_cell = [&](size_t i) -> double {
            return fl[i] * domain.k_liquid_w_mk + (1.0 - fl[i]) * domain.k_solid_w_mk;
        };

        // Left wall Dirichlet T_wall (at x = 0, distance dx/2 to cell 0 center)
        const double k0 = (domain.wall_temp_k >= domain.t_melt_k) ? domain.k_liquid_w_mk : domain.k_solid_w_mk;
        flux[0] = -k0 * (T[0] - domain.wall_temp_k) / (0.5 * dx);
        absorbed_energy += flux[0] * dt;

        // Interior faces via harmonic mean conductivity
        for (size_t i = 1; i < N; ++i) {
            const double ka = k_cell(i - 1);
            const double kb = k_cell(i);
            const double k_face = (2.0 * ka * kb) / std::max(1e-12, ka + kb);
            flux[i] = -k_face * (T[i] - T[i - 1]) / dx;
        }

        // Right wall insulated (adiabatic)
        flux[N] = 0.0;

        for (size_t i = 0; i < N; ++i) {
            H[i] += (flux[i] - flux[i + 1]) * (dt / dx);
            if (H[i] <= 0.0) {
                fl[i] = 0.0;
                T[i] = domain.t_melt_k + H[i] / Cs;
            } else if (H[i] < H_lat) {
                fl[i] = H[i] / H_lat;
                T[i] = domain.t_melt_k;
            } else {
                fl[i] = 1.0;
                T[i] = domain.t_melt_k + (H[i] - H_lat) / Cl;
            }
        }
    }

    // Track melted/solidified depth by integrating liquid fraction along domain (for melting wall)
    double sum_melted = 0.0;
    const bool melting_from_cold = (domain.wall_temp_k > domain.t_melt_k);
    for (size_t i = 0; i < N; ++i) {
        sum_melted += (melting_from_cold ? fl[i] : (1.0 - fl[i])) * dx;
    }

    PhaseChangeResult1D out;
    out.cell_temperatures_k = std::move(T);
    out.liquid_fraction = std::move(fl);
    out.enthalpy_j_m3 = std::move(H);
    out.interface_position_m = sum_melted;
    out.total_absorbed_energy_j_m2 = absorbed_energy;
    return core::base::Result<PhaseChangeResult1D>::ok(std::move(out));
}

std::vector<LumpedMeltingSample> EnthalpyPhaseChangeSolver::simulate_lumped_melting(
    double mass_kg,
    double initial_temp_k,
    double input_power_w,
    const mat::LatentPhaseParameters& params,
    double total_time_s,
    double dt_s) {

    std::vector<LumpedMeltingSample> trace;
    if (mass_kg <= 0.0 || dt_s <= 0.0 || total_time_s <= 0.0) return trace;

    const double t_melt = 0.5 * (params.solidus_temp_k + params.liquidus_temp_k);
    const double h_lat_total = mass_kg * params.latent_heat_fusion_j_kg;
    const double cs_total = mass_kg * params.cp_solid_j_kg_k;
    const double cl_total = mass_kg * params.cp_liquid_j_kg_k;

    double enthalpy = 0.0;
    if (initial_temp_k <= t_melt) {
        enthalpy = cs_total * (initial_temp_k - t_melt);
    } else {
        enthalpy = h_lat_total + cl_total * (initial_temp_k - t_melt);
    }
    const double h0 = enthalpy;

    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_s));
    trace.reserve(steps + 1);

    for (size_t s = 0; s <= steps; ++s) {
        const double t = static_cast<double>(s) * dt_s;
        double T = t_melt;
        double fl = 0.0;
        if (enthalpy <= 0.0) {
            fl = 0.0;
            T = t_melt + enthalpy / cs_total;
        } else if (enthalpy < h_lat_total) {
            fl = enthalpy / h_lat_total;
            T = t_melt;
        } else {
            fl = 1.0;
            T = t_melt + (enthalpy - h_lat_total) / cl_total;
        }
        trace.push_back({t, T, fl, enthalpy - h0});
        enthalpy += input_power_w * dt_s;
    }
    return trace;
}

} // namespace shunya::phy
