#include "shunya/phy/pressure_poisson.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

PoissonSolveStats GridPcgSolver::solve_7pt_laplacian(
    size_t nx, size_t ny, size_t nz,
    const std::vector<double>& diag,
    const std::vector<double>& plus_x,
    const std::vector<double>& plus_y,
    const std::vector<double>& plus_z,
    const std::vector<double>& rhs,
    std::vector<double>& x,
    double tolerance,
    size_t max_iterations,
    double mic_tau) {

    const size_t N = nx * ny * nz;
    auto idx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * ny + j) * nx + i;
    };

    if (x.size() != N) x.assign(N, 0.0);

    // 1) Build Bridson MIC(0) preconditioner vector
    std::vector<double> precon(N, 0.0);
    for (size_t k = 0; k < nz; ++k) {
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const size_t c = idx(i, j, k);
                if (diag[c] <= 1e-18) {
                    precon[c] = 0.0;
                    continue;
                }
                double e = diag[c];
                if (i > 0) {
                    const size_t cx = idx(i - 1, j, k);
                    const double px = plus_x[cx] * precon[cx];
                    const double py = plus_y[cx] * precon[cx];
                    const double pz = plus_z[cx] * precon[cx];
                    e -= px * px + mic_tau * px * (py + pz);
                }
                if (j > 0) {
                    const size_t cy = idx(i, j - 1, k);
                    const double px = plus_x[cy] * precon[cy];
                    const double py = plus_y[cy] * precon[cy];
                    const double pz = plus_z[cy] * precon[cy];
                    e -= py * py + mic_tau * py * (px + pz);
                }
                if (k > 0) {
                    const size_t cz = idx(i, j, k - 1);
                    const double px = plus_x[cz] * precon[cz];
                    const double py = plus_y[cz] * precon[cz];
                    const double pz = plus_z[cz] * precon[cz];
                    e -= pz * pz + mic_tau * pz * (px + py);
                }
                if (e < 0.25 * diag[c]) e = diag[c];
                precon[c] = 1.0 / std::sqrt(e);
            }
        }
    }

    auto apply_precond = [&](const std::vector<double>& r, std::vector<double>& z) {
        std::vector<double> q(N, 0.0);
        // Forward substitution L * q = r
        for (size_t k = 0; k < nz; ++k) {
            for (size_t j = 0; j < ny; ++j) {
                for (size_t i = 0; i < nx; ++i) {
                    const size_t c = idx(i, j, k);
                    if (precon[c] == 0.0) continue;
                    double t = r[c];
                    if (i > 0) {
                        const size_t cx = idx(i - 1, j, k);
                        t += plus_x[cx] * precon[cx] * q[cx];
                    }
                    if (j > 0) {
                        const size_t cy = idx(i, j - 1, k);
                        t += plus_y[cy] * precon[cy] * q[cy];
                    }
                    if (k > 0) {
                        const size_t cz = idx(i, j, k - 1);
                        t += plus_z[cz] * precon[cz] * q[cz];
                    }
                    q[c] = t * precon[c];
                }
            }
        }
        // Back substitution L^T * z = q
        for (size_t kk = nz; kk-- > 0;) {
            for (size_t jj = ny; jj-- > 0;) {
                for (size_t ii = nx; ii-- > 0;) {
                    const size_t c = idx(ii, jj, kk);
                    if (precon[c] == 0.0) {
                        z[c] = 0.0;
                        continue;
                    }
                    double t = q[c];
                    if (ii + 1 < nx) t += plus_x[c] * precon[c] * z[idx(ii + 1, jj, kk)];
                    if (jj + 1 < ny) t += plus_y[c] * precon[c] * z[idx(ii, jj + 1, kk)];
                    if (kk + 1 < nz) t += plus_z[c] * precon[c] * z[idx(ii, jj, kk + 1)];
                    z[c] = t * precon[c];
                }
            }
        }
    };

    auto apply_A = [&](const std::vector<double>& v, std::vector<double>& Av) {
        for (size_t k = 0; k < nz; ++k) {
            for (size_t j = 0; j < ny; ++j) {
                for (size_t i = 0; i < nx; ++i) {
                    const size_t c = idx(i, j, k);
                    if (diag[c] <= 1e-18) {
                        Av[c] = 0.0;
                        continue;
                    }
                    double s = diag[c] * v[c];
                    if (i > 0) s -= plus_x[idx(i - 1, j, k)] * v[idx(i - 1, j, k)];
                    if (i + 1 < nx) s -= plus_x[c] * v[idx(i + 1, j, k)];
                    if (j > 0) s -= plus_y[idx(i, j - 1, k)] * v[idx(i, j - 1, k)];
                    if (j + 1 < ny) s -= plus_y[c] * v[idx(i, j + 1, k)];
                    if (k > 0) s -= plus_z[idx(i, j, k - 1)] * v[idx(i, j, k - 1)];
                    if (k + 1 < nz) s -= plus_z[c] * v[idx(i, j, k + 1)];
                    Av[c] = s;
                }
            }
        }
    };

    std::vector<double> Ax(N, 0.0), r(N, 0.0), z(N, 0.0), p(N, 0.0), s_vec(N, 0.0);
    apply_A(x, Ax);
    double max_res = 0.0;
    for (size_t c = 0; c < N; ++c) {
        r[c] = (diag[c] > 1e-18) ? (rhs[c] - Ax[c]) : 0.0;
        max_res = std::max(max_res, std::abs(r[c]));
    }

    PoissonSolveStats stats;
    stats.max_residual = max_res;
    if (max_res <= tolerance) {
        stats.converged = true;
        return stats;
    }

    apply_precond(r, z);
    p = z;
    double sigma = 0.0;
    for (size_t c = 0; c < N; ++c) sigma += r[c] * z[c];

    for (size_t iter = 1; iter <= max_iterations; ++iter) {
        apply_A(p, s_vec);
        double pAs = 0.0;
        for (size_t c = 0; c < N; ++c) pAs += p[c] * s_vec[c];
        if (std::abs(pAs) <= 1e-30) {
            stats.iterations = iter;
            stats.max_residual = max_res;
            stats.converged = (max_res <= tolerance);
            return stats;
        }
        const double alpha = sigma / pAs;
        max_res = 0.0;
        for (size_t c = 0; c < N; ++c) {
            x[c] += alpha * p[c];
            r[c] -= alpha * s_vec[c];
            max_res = std::max(max_res, std::abs(r[c]));
        }
        stats.iterations = iter;
        stats.max_residual = max_res;
        if (max_res <= tolerance) {
            stats.converged = true;
            return stats;
        }

        apply_precond(r, z);
        double sigma_new = 0.0;
        for (size_t c = 0; c < N; ++c) sigma_new += r[c] * z[c];
        const double beta = sigma_new / std::max(1e-30, sigma);
        for (size_t c = 0; c < N; ++c) {
            p[c] = z[c] + beta * p[c];
        }
        sigma = sigma_new;
    }

    stats.converged = (max_res <= tolerance * 100.0);
    return stats;
}

core::base::Result<PoissonSolveStats> PressurePoissonSolver::project(
    size_t nx, size_t ny, size_t nz,
    double dx_m,
    double density_kg_m3,
    double dt_s,
    const std::vector<CellFluidKind>& cell_types,
    std::vector<double>& u,
    std::vector<double>& v,
    std::vector<double>& w,
    std::vector<double>& p,
    double tolerance,
    double inlet_left_u_m_s,
    bool open_right_outlet) {

    const size_t N = nx * ny * nz;
    if (N == 0 || dx_m <= 0.0 || dt_s <= 0.0 || density_kg_m3 <= 0.0 || cell_types.size() != N) {
        return core::base::Result<PoissonSolveStats>::err(
            core::base::Error{85, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Invalid PressurePoissonSolver dimensions or parameters", ""});
    }

    auto idx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * ny + j) * nx + i;
    };
    auto uidx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * ny + j) * (nx + 1) + i;
    };
    auto vidx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * (ny + 1) + j) * nx + i;
    };
    auto widx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * ny + j) * nx + i;
    };

    bool has_open_air = false;
    size_t first_fluid = N;
    for (size_t c = 0; c < N; ++c) {
        if (cell_types[c] == CellFluidKind::OpenAir) has_open_air = true;
        if (cell_types[c] == CellFluidKind::Fluid && first_fluid == N) first_fluid = c;
    }
    if (first_fluid == N) return core::base::Result<PoissonSolveStats>::ok(PoissonSolveStats{0, 0.0, true});

    // If enclosed pure-Neumann cavity, pin one fluid reference cell (p = 0) to eliminate nullspace
    auto is_dirichlet_zero = [&](size_t c) -> bool {
        if (cell_types[c] == CellFluidKind::OpenAir) return true;
        return (!has_open_air && c == first_fluid);
    };

    // Zero all Solid-adjacent normal velocities before divergence RHS computation so RHS matches projected BCs
    for (size_t k = 0; k < nz; ++k) {
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i <= nx; ++i) {
                if (i == 0) {
                    const bool fluid_left = (cell_types[idx(0, j, k)] == CellFluidKind::Fluid);
                    u[uidx(0, j, k)] = (fluid_left && inlet_left_u_m_s != 0.0) ? inlet_left_u_m_s : 0.0;
                } else if (i == nx) {
                    if (open_right_outlet && nx >= 2 &&
                        cell_types[idx(nx - 1, j, k)] != CellFluidKind::Solid) {
                        u[uidx(nx, j, k)] = u[uidx(nx - 1, j, k)];
                    } else {
                        u[uidx(nx, j, k)] = 0.0;
                    }
                } else if (cell_types[idx(i - 1, j, k)] == CellFluidKind::Solid ||
                           cell_types[idx(i, j, k)] == CellFluidKind::Solid) {
                    u[uidx(i, j, k)] = 0.0;
                }
            }
        }
    }
    for (size_t k = 0; k < nz; ++k) {
        for (size_t j = 0; j <= ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                if (j == 0 || j == ny ||
                    cell_types[idx(i, j - 1, k)] == CellFluidKind::Solid ||
                    cell_types[idx(i, j, k)] == CellFluidKind::Solid) {
                    v[vidx(i, j, k)] = 0.0;
                }
            }
        }
    }
    if (nz > 1) {
        for (size_t k = 0; k <= nz; ++k) {
            for (size_t j = 0; j < ny; ++j) {
                for (size_t i = 0; i < nx; ++i) {
                    if (k == 0 || k == nz ||
                        cell_types[idx(i, j, k - 1)] == CellFluidKind::Solid ||
                        cell_types[idx(i, j, k)] == CellFluidKind::Solid) {
                        w[widx(i, j, k)] = 0.0;
                    }
                }
            }
        }
    }

    std::vector<double> diag(N, 0.0), plus_x(N, 0.0), plus_y(N, 0.0), plus_z(N, 0.0), rhs(N, 0.0);
    const double scale = (density_kg_m3 * dx_m) / dt_s;

    for (size_t k = 0; k < nz; ++k) {
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const size_t c = idx(i, j, k);
                if (cell_types[c] != CellFluidKind::Fluid || is_dirichlet_zero(c)) {
                    diag[c] = is_dirichlet_zero(c) ? 1.0 : 0.0;
                    rhs[c] = 0.0;
                    continue;
                }

                const double div_dx =
                    (u[uidx(i + 1, j, k)] - u[uidx(i, j, k)]) +
                    (v[vidx(i, j + 1, k)] - v[vidx(i, j, k)]) +
                    ((nz > 1) ? (w[widx(i, j, k + 1)] - w[widx(i, j, k)]) : 0.0);
                rhs[c] = -scale * div_dx;

                double d = 0.0;
                if (i > 0 && cell_types[idx(i - 1, j, k)] != CellFluidKind::Solid) d += 1.0;
                if (i + 1 < nx && cell_types[idx(i + 1, j, k)] != CellFluidKind::Solid) {
                    d += 1.0;
                    if (!is_dirichlet_zero(idx(i + 1, j, k))) plus_x[c] = 1.0;
                }
                if (j > 0 && cell_types[idx(i, j - 1, k)] != CellFluidKind::Solid) d += 1.0;
                if (j + 1 < ny && cell_types[idx(i, j + 1, k)] != CellFluidKind::Solid) {
                    d += 1.0;
                    if (!is_dirichlet_zero(idx(i, j + 1, k))) plus_y[c] = 1.0;
                }
                if (nz > 1) {
                    if (k > 0 && cell_types[idx(i, j, k - 1)] != CellFluidKind::Solid) d += 1.0;
                    if (k + 1 < nz && cell_types[idx(i, j, k + 1)] != CellFluidKind::Solid) {
                        d += 1.0;
                        if (!is_dirichlet_zero(idx(i, j, k + 1))) plus_z[c] = 1.0;
                    }
                }
                diag[c] = d;
            }
        }
    }

    if (p.size() != N) p.assign(N, 0.0);
    for (size_t c = 0; c < N; ++c) {
        if (is_dirichlet_zero(c) || cell_types[c] == CellFluidKind::Solid) p[c] = 0.0;
    }

    const auto stats = GridPcgSolver::solve_7pt_laplacian(
        nx, ny, nz, diag, plus_x, plus_y, plus_z, rhs, p, tolerance * scale, 1000, 0.97);

    // Velocity projection u -= (dt / rho) * grad(p)
    const double grad_scale = dt_s / (density_kg_m3 * dx_m);
    for (size_t k = 0; k < nz; ++k) {
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i <= nx; ++i) {
                if (i == 0) {
                    const bool fluid_left = (cell_types[idx(0, j, k)] == CellFluidKind::Fluid);
                    u[uidx(0, j, k)] = (fluid_left && inlet_left_u_m_s != 0.0) ? inlet_left_u_m_s : 0.0;
                    continue;
                }
                if (i == nx) {
                    if (open_right_outlet && nx >= 2 &&
                        cell_types[idx(nx - 1, j, k)] != CellFluidKind::Solid) {
                        u[uidx(nx, j, k)] = u[uidx(nx - 1, j, k)];
                    } else {
                        u[uidx(nx, j, k)] = 0.0;
                    }
                    continue;
                }
                const auto ca = cell_types[idx(i - 1, j, k)];
                const auto cb = cell_types[idx(i, j, k)];
                if (ca == CellFluidKind::Solid || cb == CellFluidKind::Solid) {
                    u[uidx(i, j, k)] = 0.0;
                } else {
                    u[uidx(i, j, k)] -= grad_scale * (p[idx(i, j, k)] - p[idx(i - 1, j, k)]);
                }
            }
        }
    }
    for (size_t k = 0; k < nz; ++k) {
        for (size_t j = 0; j <= ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                if (j == 0 || j == ny) {
                    v[vidx(i, j, k)] = 0.0;
                    continue;
                }
                const auto ca = cell_types[idx(i, j - 1, k)];
                const auto cb = cell_types[idx(i, j, k)];
                if (ca == CellFluidKind::Solid || cb == CellFluidKind::Solid) {
                    v[vidx(i, j, k)] = 0.0;
                } else {
                    v[vidx(i, j, k)] -= grad_scale * (p[idx(i, j, k)] - p[idx(i, j - 1, k)]);
                }
            }
        }
    }
    if (nz > 1) {
        for (size_t k = 0; k <= nz; ++k) {
            for (size_t j = 0; j < ny; ++j) {
                for (size_t i = 0; i < nx; ++i) {
                    if (k == 0 || k == nz) {
                        w[widx(i, j, k)] = 0.0;
                        continue;
                    }
                    const auto ca = cell_types[idx(i, j, k - 1)];
                    const auto cb = cell_types[idx(i, j, k)];
                    if (ca == CellFluidKind::Solid || cb == CellFluidKind::Solid) {
                        w[widx(i, j, k)] = 0.0;
                    } else {
                        w[widx(i, j, k)] -= grad_scale * (p[idx(i, j, k)] - p[idx(i, j, k - 1)]);
                    }
                }
            }
        }
    }

    return core::base::Result<PoissonSolveStats>::ok(stats);
}

double PressurePoissonSolver::compute_max_divergence(
    size_t nx, size_t ny, size_t nz,
    double dx_m,
    const std::vector<CellFluidKind>& cell_types,
    const std::vector<double>& u,
    const std::vector<double>& v,
    const std::vector<double>& w) noexcept {

    auto idx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * ny + j) * nx + i;
    };
    auto uidx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * ny + j) * (nx + 1) + i;
    };
    auto vidx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * (ny + 1) + j) * nx + i;
    };
    auto widx = [&](size_t i, size_t j, size_t k) noexcept {
        return (k * ny + j) * nx + i;
    };

    double max_div = 0.0;
    for (size_t k = 0; k < nz; ++k) {
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                if (cell_types[idx(i, j, k)] != CellFluidKind::Fluid) continue;
                const double du = u[uidx(i + 1, j, k)] - u[uidx(i, j, k)];
                const double dv = v[vidx(i, j + 1, k)] - v[vidx(i, j, k)];
                const double dw = (nz > 1) ? (w[widx(i, j, k + 1)] - w[widx(i, j, k)]) : 0.0;
                max_div = std::max(max_div, std::abs(du + dv + dw) / dx_m);
            }
        }
    }
    return max_div;
}

} // namespace shunya::phy
