#include "shunya/phy/thermal_solver_3d.hpp"
#include "shunya/phy/conjugate_gradient_sparse.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

ThermalSolver3D::ThermalSolver3D(ThermalGridSpec3D spec)
    : spec_(spec),
      temp_(spec.nx * spec.ny * spec.nz, spec.initial_temp_k),
      q_gen_(spec.nx * spec.ny * spec.nz, 0.0),
      is_dirichlet_(spec.nx * spec.ny * spec.nz, false),
      dirichlet_temp_(spec.nx * spec.ny * spec.nz, spec.initial_temp_k) {}

void ThermalSolver3D::set_dirichlet_node(size_t ix, size_t iy, size_t iz, double fixed_temp_k) {
    if (ix >= spec_.nx || iy >= spec_.ny || iz >= spec_.nz) return;
    const size_t idx = index(ix, iy, iz);
    is_dirichlet_[idx] = true;
    dirichlet_temp_[idx] = fixed_temp_k;
    temp_[idx] = fixed_temp_k;
}

void ThermalSolver3D::set_dirichlet_x_min_plane(double fixed_temp_k) {
    for (size_t iz = 0; iz < spec_.nz; ++iz) {
        for (size_t iy = 0; iy < spec_.ny; ++iy) {
            set_dirichlet_node(0, iy, iz, fixed_temp_k);
        }
    }
}

void ThermalSolver3D::set_dirichlet_x_max_plane(double fixed_temp_k) {
    for (size_t iz = 0; iz < spec_.nz; ++iz) {
        for (size_t iy = 0; iy < spec_.ny; ++iy) {
            set_dirichlet_node(spec_.nx - 1, iy, iz, fixed_temp_k);
        }
    }
}

void ThermalSolver3D::set_volumetric_source(size_t ix, size_t iy, size_t iz, double q_w_m3) {
    if (ix >= spec_.nx || iy >= spec_.ny || iz >= spec_.nz) return;
    q_gen_[index(ix, iy, iz)] = q_w_m3;
}

void ThermalSolver3D::set_temperature(size_t ix, size_t iy, size_t iz, double temp_k) {
    if (ix >= spec_.nx || iy >= spec_.ny || iz >= spec_.nz) return;
    const size_t idx = index(ix, iy, iz);
    temp_[idx] = temp_k;
    if (is_dirichlet_[idx]) dirichlet_temp_[idx] = temp_k;
}

double ThermalSolver3D::temperature(size_t ix, size_t iy, size_t iz) const noexcept {
    if (ix >= spec_.nx || iy >= spec_.ny || iz >= spec_.nz) return 0.0;
    return temp_[index(ix, iy, iz)];
}

core::base::Result<void> ThermalSolver3D::step_crank_nicolson(double dt_s) {
    using namespace core::domain;

    if (spec_.nx < 2 || spec_.ny < 1 || spec_.nz < 1 || dt_s <= 0.0) {
        return core::base::Result<void>::err(
            core::base::Error{61, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "ThermalSolver3D requires valid grid dimensions and positive dt_s", ""});
    }

    const size_t N = node_count();
    const double dx = spec_.lx_m / static_cast<double>(spec_.nx - 1);
    const double dy = (spec_.ny > 1) ? (spec_.ly_m / static_cast<double>(spec_.ny - 1)) : spec_.ly_m;
    const double dz = (spec_.nz > 1) ? (spec_.lz_m / static_cast<double>(spec_.nz - 1)) : spec_.lz_m;

    auto edge_weight = [](size_t i, size_t n) -> double {
        if (n == 1) return 1.0;
        return (i == 0 || i + 1 == n) ? 0.5 : 1.0;
    };

    const double rho_cp = spec_.density_kg_m3 * spec_.specific_heat_j_kg_k;
    const double k = spec_.conductivity_w_m_k;

    std::vector<Triplet> triplets;
    triplets.reserve(N * 7);
    std::vector<double> rhs(N, 0.0);

    for (size_t iz = 0; iz < spec_.nz; ++iz) {
        const double wz = edge_weight(iz, spec_.nz);
        const double hz = wz * dz;
        for (size_t iy = 0; iy < spec_.ny; ++iy) {
            const double wy = edge_weight(iy, spec_.ny);
            const double hy = wy * dy;
            for (size_t ix = 0; ix < spec_.nx; ++ix) {
                const size_t u = index(ix, iy, iz);
                if (is_dirichlet_[u]) {
                    triplets.push_back({u, u, 1.0});
                    rhs[u] = dirichlet_temp_[u];
                    continue;
                }

                const double wx = edge_weight(ix, spec_.nx);
                const double hx = wx * dx;
                const double vol = hx * hy * hz;
                const double c_dt = (rho_cp * vol) / dt_s;

                double diag_A = c_dt;
                double rhs_u = c_dt * temp_[u] + q_gen_[u] * vol;

                auto add_neighbor = [&](size_t v, double g_cond) {
                    const double h_g = 0.5 * g_cond;
                    diag_A += h_g;
                    rhs_u += h_g * (temp_[v] - temp_[u]);
                    if (is_dirichlet_[v]) {
                        rhs_u += h_g * dirichlet_temp_[v];
                    } else {
                        triplets.push_back({u, v, -h_g});
                    }
                };

                if (ix > 0) add_neighbor(index(ix - 1, iy, iz), k * (hy * hz) / dx);
                if (ix + 1 < spec_.nx) add_neighbor(index(ix + 1, iy, iz), k * (hy * hz) / dx);
                if (iy > 0) add_neighbor(index(ix, iy - 1, iz), k * (hx * hz) / dy);
                if (iy + 1 < spec_.ny) add_neighbor(index(ix, iy + 1, iz), k * (hx * hz) / dy);
                if (iz > 0) add_neighbor(index(ix, iy, iz - 1), k * (hx * hy) / dz);
                if (iz + 1 < spec_.nz) add_neighbor(index(ix, iy, iz + 1), k * (hx * hy) / dz);

                triplets.push_back({u, u, diag_A});
                rhs[u] = rhs_u;
            }
        }
    }

    const SparseMatrixCSR A = SparseMatrixCSR::from_triplets(N, N, triplets);
    std::vector<double> x = temp_;
    auto cg_res = SparseConjugateGradient::solve(A, rhs, x, 1e-11, 4000);
    if (!cg_res.is_ok()) {
        return core::base::Result<void>::err(cg_res.error());
    }
    if (!cg_res.value().converged) {
        return core::base::Result<void>::err(
            core::base::Error{62, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
                              "ThermalSolver3D Crank-Nicolson CG failed to converge", ""});
    }
    temp_ = std::move(x);
    return core::base::Result<void>::ok();
}

core::base::Result<void> ThermalSolver3D::advance(double total_time_s, double dt_s) {
    if (total_time_s <= 0.0 || dt_s <= 0.0) {
        return core::base::Result<void>::err(
            core::base::Error{63, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "total_time_s and dt_s must be positive", ""});
    }
    const size_t steps = static_cast<size_t>(std::round(total_time_s / dt_s));
    const double exact_dt = total_time_s / static_cast<double>(std::max<size_t>(1, steps));
    for (size_t s = 0; s < std::max<size_t>(1, steps); ++s) {
        auto r = step_crank_nicolson(exact_dt);
        if (!r.is_ok()) return r;
    }
    return core::base::Result<void>::ok();
}

} // namespace shunya::phy
