#include "shunya/phy/mac_grid_solver_3d.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

using core::base::math::Vector2;
using core::base::math::Vector3;

MacGridFluidSolver3D::MacGridFluidSolver3D(MacGridConfig cfg)
    : cfg_(cfg),
      cell_types_(cfg.nx * cfg.ny * cfg.nz, CellFluidKind::Fluid),
      u_((cfg.nx + 1) * cfg.ny * cfg.nz, 0.0),
      v_(cfg.nx * (cfg.ny + 1) * cfg.nz, 0.0),
      w_(cfg.nx * cfg.ny * (cfg.nz + 1), 0.0),
      p_(cfg.nx * cfg.ny * cfg.nz, 0.0),
      dye_(cfg.nx * cfg.ny * cfg.nz, 0.0) {}

void MacGridFluidSolver3D::set_cell_type(size_t i, size_t j, size_t k, CellFluidKind kind) {
    if (i < cfg_.nx && j < cfg_.ny && k < cfg_.nz) {
        cell_types_[(k * cfg_.ny + j) * cfg_.nx + i] = kind;
    }
}

void MacGridFluidSolver3D::set_dye(size_t i, size_t j, size_t k, double val) {
    if (i < cfg_.nx && j < cfg_.ny && k < cfg_.nz) {
        dye_[(k * cfg_.ny + j) * cfg_.nx + i] = val;
    }
}

double MacGridFluidSolver3D::dye(size_t i, size_t j, size_t k) const noexcept {
    if (i < cfg_.nx && j < cfg_.ny && k < cfg_.nz) {
        return dye_[(k * cfg_.ny + j) * cfg_.nx + i];
    }
    return 0.0;
}

double MacGridFluidSolver3D::u(size_t i, size_t j, size_t k) const noexcept {
    return u_[(k * cfg_.ny + j) * (cfg_.nx + 1) + i];
}
double MacGridFluidSolver3D::v(size_t i, size_t j, size_t k) const noexcept {
    return v_[(k * (cfg_.ny + 1) + j) * cfg_.nx + i];
}
double MacGridFluidSolver3D::w(size_t i, size_t j, size_t k) const noexcept {
    return w_[(k * cfg_.ny + j) * cfg_.nx + i];
}
double MacGridFluidSolver3D::pressure(size_t i, size_t j, size_t k) const noexcept {
    return p_[(k * cfg_.ny + j) * cfg_.nx + i];
}

void MacGridFluidSolver3D::set_u(size_t i, size_t j, size_t k, double val) {
    if (i <= cfg_.nx && j < cfg_.ny && k < cfg_.nz) {
        u_[(k * cfg_.ny + j) * (cfg_.nx + 1) + i] = val;
    }
}
void MacGridFluidSolver3D::set_v(size_t i, size_t j, size_t k, double val) {
    if (i < cfg_.nx && j <= cfg_.ny && k < cfg_.nz) {
        v_[(k * (cfg_.ny + 1) + j) * cfg_.nx + i] = val;
    }
}
void MacGridFluidSolver3D::set_w(size_t i, size_t j, size_t k, double val) {
    if (i < cfg_.nx && j < cfg_.ny && k <= cfg_.nz) {
        w_[(k * cfg_.ny + j) * cfg_.nx + i] = val;
    }
}

Vector3<double> MacGridFluidSolver3D::sample_velocity(double x, double y, double z) const noexcept {
    const double inv_dx = 1.0 / cfg_.dx_m;
    const double ux = SemiLagrangianAdvector::sample_trilinear(
        u_, cfg_.nx + 1, cfg_.ny, cfg_.nz,
        x * inv_dx, y * inv_dx - 0.5, (cfg_.nz > 1) ? (z * inv_dx - 0.5) : 0.0);
    const double vy = SemiLagrangianAdvector::sample_trilinear(
        v_, cfg_.nx, cfg_.ny + 1, cfg_.nz,
        x * inv_dx - 0.5, y * inv_dx, (cfg_.nz > 1) ? (z * inv_dx - 0.5) : 0.0);
    const double wz = (cfg_.nz > 1) ? SemiLagrangianAdvector::sample_trilinear(
        w_, cfg_.nx, cfg_.ny, cfg_.nz + 1,
        x * inv_dx - 0.5, y * inv_dx - 0.5, z * inv_dx) : 0.0;
    return {ux, vy, wz};
}

void MacGridFluidSolver3D::apply_wall_bcs() {
    for (size_t k = 0; k < cfg_.nz; ++k) {
        for (size_t j = 0; j < cfg_.ny; ++j) {
            const bool fluid_left = (cell_types_[(k * cfg_.ny + j) * cfg_.nx + 0] == CellFluidKind::Fluid);
            u_[(k * cfg_.ny + j) * (cfg_.nx + 1) + 0] =
                (fluid_left && cfg_.inlet_left_u_m_s != 0.0) ? cfg_.inlet_left_u_m_s : 0.0;
            if (!cfg_.open_right_outlet ||
                cell_types_[(k * cfg_.ny + j) * cfg_.nx + (cfg_.nx - 1)] == CellFluidKind::Solid) {
                u_[(k * cfg_.ny + j) * (cfg_.nx + 1) + cfg_.nx] = 0.0;
            }
        }
        for (size_t i = 0; i < cfg_.nx; ++i) {
            v_[(k * (cfg_.ny + 1) + 0) * cfg_.nx + i] = 0.0;
            v_[(k * (cfg_.ny + 1) + cfg_.ny) * cfg_.nx + i] = 0.0;
        }
    }
    if (cfg_.nz > 1) {
        for (size_t j = 0; j < cfg_.ny; ++j) {
            for (size_t i = 0; i < cfg_.nx; ++i) {
                w_[(0 * cfg_.ny + j) * cfg_.nx + i] = 0.0;
                w_[(cfg_.nz * cfg_.ny + j) * cfg_.nx + i] = 0.0;
            }
        }
    }
}

void MacGridFluidSolver3D::apply_viscosity(double dt_s) {
    if (cfg_.kinematic_viscosity_m2_s <= 0.0) return;
    const double nu = cfg_.kinematic_viscosity_m2_s;
    const double dx = cfg_.dx_m;
    const double max_sub_dt = 0.1 * (dx * dx) / nu;
    const int substeps = std::max(1, static_cast<int>(std::ceil(dt_s / max_sub_dt)));
    const double sdt = dt_s / static_cast<double>(substeps);
    const double coeff = nu * sdt / (dx * dx);

    for (int sub = 0; sub < substeps; ++sub) {
        // 1) u diffusion with tangential no-slip ghosts at y=0 (0) & y=ny*dx (lid_top_u_m_s)
        std::vector<double> un = u_;
        for (size_t k = 0; k < cfg_.nz; ++k) {
            for (size_t j = 0; j < cfg_.ny; ++j) {
                for (size_t i = 1; i < cfg_.nx; ++i) {
                    const size_t c = (k * cfg_.ny + j) * (cfg_.nx + 1) + i;
                    const double uc = u_[c];
                    const double ul = u_[c - 1];
                    const double ur = u_[c + 1];
                    const double ud = (j > 0) ? u_[(k * cfg_.ny + (j - 1)) * (cfg_.nx + 1) + i] : -uc;
                    const double uu = (j + 1 < cfg_.ny) ? u_[(k * cfg_.ny + (j + 1)) * (cfg_.nx + 1) + i]
                                                        : (2.0 * cfg_.lid_top_u_m_s - uc);
                    double uz_m = uc, uz_p = uc;
                    if (cfg_.nz > 1) {
                        uz_m = (k > 0) ? u_[((k - 1) * cfg_.ny + j) * (cfg_.nx + 1) + i] : -uc;
                        uz_p = (k + 1 < cfg_.nz) ? u_[((k + 1) * cfg_.ny + j) * (cfg_.nx + 1) + i] : -uc;
                    }
                    const double lap = (ul + ur + ud + uu + uz_m + uz_p - 6.0 * uc);
                    un[c] = uc + coeff * lap;
                }
            }
        }
        u_ = std::move(un);

        // 2) v diffusion with tangential no-slip ghosts at x=0 & x=nx*dx (u_wall_tan = 0)
        std::vector<double> vn = v_;
        for (size_t k = 0; k < cfg_.nz; ++k) {
            for (size_t j = 1; j < cfg_.ny; ++j) {
                for (size_t i = 0; i < cfg_.nx; ++i) {
                    const size_t c = (k * (cfg_.ny + 1) + j) * cfg_.nx + i;
                    const double vc = v_[c];
                    const double vl = (i > 0) ? v_[(k * (cfg_.ny + 1) + j) * cfg_.nx + (i - 1)] : -vc;
                    const double vr = (i + 1 < cfg_.nx) ? v_[(k * (cfg_.ny + 1) + j) * cfg_.nx + (i + 1)] : -vc;
                    const double vd = v_[(k * (cfg_.ny + 1) + (j - 1)) * cfg_.nx + i];
                    const double vu = v_[(k * (cfg_.ny + 1) + (j + 1)) * cfg_.nx + i];
                    double vz_m = vc, vz_p = vc;
                    if (cfg_.nz > 1) {
                        vz_m = (k > 0) ? v_[((k - 1) * (cfg_.ny + 1) + j) * cfg_.nx + i] : -vc;
                        vz_p = (k + 1 < cfg_.nz) ? v_[((k + 1) * (cfg_.ny + 1) + j) * cfg_.nx + i] : -vc;
                    }
                    vn[c] = vc + coeff * (vl + vr + vd + vu + vz_m + vz_p - 6.0 * vc);
                }
            }
        }
        v_ = std::move(vn);

        if (cfg_.nz > 1) {
            std::vector<double> wn = w_;
            for (size_t k = 1; k < cfg_.nz; ++k) {
                for (size_t j = 0; j < cfg_.ny; ++j) {
                    for (size_t i = 0; i < cfg_.nx; ++i) {
                        const size_t c = (k * cfg_.ny + j) * cfg_.nx + i;
                        const double wc = w_[c];
                        const double wl = (i > 0) ? w_[(k * cfg_.ny + j) * cfg_.nx + (i - 1)] : -wc;
                        const double wr = (i + 1 < cfg_.nx) ? w_[(k * cfg_.ny + j) * cfg_.nx + (i + 1)] : -wc;
                        const double wd = (j > 0) ? w_[(k * cfg_.ny + (j - 1)) * cfg_.nx + i] : -wc;
                        const double wu = (j + 1 < cfg_.ny) ? w_[(k * cfg_.ny + (j + 1)) * cfg_.nx + i] : -wc;
                        const double wz_m = w_[((k - 1) * cfg_.ny + j) * cfg_.nx + i];
                        const double wz_p = w_[((k + 1) * cfg_.ny + j) * cfg_.nx + i];
                        wn[c] = wc + coeff * (wl + wr + wd + wu + wz_m + wz_p - 6.0 * wc);
                    }
                }
            }
            w_ = std::move(wn);
        }
    }
}

core::base::Result<PoissonSolveStats> MacGridFluidSolver3D::step(double dt_s) {
    if (dt_s <= 0.0) {
        return core::base::Result<PoissonSolveStats>::err(
            core::base::Error{86, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "MacGridFluidSolver3D dt_s must be positive", ""});
    }

    auto vel_sampler = [this](double x, double y, double z) {
        return this->sample_velocity(x, y, z);
    };

    // 1) BFECC semi-Lagrangian self-advection of staggered velocity & scalar dye
    std::vector<double> u_next = u_;
    std::vector<double> v_next = v_;
    std::vector<double> w_next = w_;
    std::vector<double> dye_next = dye_;

    const double z_off = (cfg_.nz > 1) ? 0.5 : 0.0;
    SemiLagrangianAdvector::advect_bfecc(u_, u_next, cfg_.nx + 1, cfg_.ny, cfg_.nz, cfg_.dx_m, 0.0, 0.5, z_off, dt_s, vel_sampler);
    SemiLagrangianAdvector::advect_bfecc(v_, v_next, cfg_.nx, cfg_.ny + 1, cfg_.nz, cfg_.dx_m, 0.5, 0.0, z_off, dt_s, vel_sampler);
    if (cfg_.nz > 1) {
        SemiLagrangianAdvector::advect_bfecc(w_, w_next, cfg_.nx, cfg_.ny, cfg_.nz + 1, cfg_.dx_m, 0.5, 0.5, 0.0, dt_s, vel_sampler);
    }
    SemiLagrangianAdvector::advect_bfecc(dye_, dye_next, cfg_.nx, cfg_.ny, cfg_.nz, cfg_.dx_m, 0.5, 0.5, z_off, dt_s, vel_sampler);

    u_ = std::move(u_next);
    v_ = std::move(v_next);
    w_ = std::move(w_next);
    dye_ = std::move(dye_next);

    apply_wall_bcs();
    apply_viscosity(dt_s);
    apply_wall_bcs();

    // 2) Preconditioned Conjugate Gradient Poisson pressure projection enforcing div(u) = 0
    return PressurePoissonSolver::project(
        cfg_.nx, cfg_.ny, cfg_.nz, cfg_.dx_m, cfg_.density_kg_m3, dt_s,
        cell_types_, u_, v_, w_, p_, 1e-10,
        cfg_.inlet_left_u_m_s, cfg_.open_right_outlet);
}

double MacGridFluidSolver3D::max_divergence() const noexcept {
    return PressurePoissonSolver::compute_max_divergence(
        cfg_.nx, cfg_.ny, cfg_.nz, cfg_.dx_m, cell_types_, u_, v_, w_);
}

Vector2<double> MacGridFluidSolver3D::find_primary_vortex_center_2d() const noexcept {
    // Compute 2D streamfunction psi(i, j) by integrating u dy upward from y = 0 (psi = 0 at y = 0)
    const size_t nx = cfg_.nx;
    const size_t ny = cfg_.ny;
    std::vector<double> psi((nx + 1) * (ny + 1), 0.0);

    for (size_t j = 1; j <= ny; ++j) {
        for (size_t i = 0; i <= nx; ++i) {
            psi[j * (nx + 1) + i] = psi[(j - 1) * (nx + 1) + i] + u(i, j - 1, 0) * cfg_.dx_m;
        }
    }

    // Top-moving lid (u_top > 0) produces clockwise circulation -> primary vortex core is minimum psi
    size_t best_i = 1, best_j = 1;
    double best_psi = 1e30;
    for (size_t j = 2; j < ny - 1; ++j) {
        for (size_t i = 2; i < nx - 1; ++i) {
            const double val = psi[j * (nx + 1) + i];
            if (val < best_psi) {
                best_psi = val;
                best_i = i;
                best_j = j;
            }
        }
    }

    // Sub-cell quadratic refinement
    const double x_m = psi[best_j * (nx + 1) + (best_i - 1)];
    const double x_c = best_psi;
    const double x_p = psi[best_j * (nx + 1) + (best_i + 1)];
    const double denom_x = x_m - 2.0 * x_c + x_p;
    const double dx_off = (std::abs(denom_x) > 1e-15) ? (0.5 * (x_m - x_p) / denom_x) : 0.0;

    const double y_m = psi[(best_j - 1) * (nx + 1) + best_i];
    const double y_p = psi[(best_j + 1) * (nx + 1) + best_i];
    const double denom_y = y_m - 2.0 * x_c + y_p;
    const double dy_off = (std::abs(denom_y) > 1e-15) ? (0.5 * (y_m - y_p) / denom_y) : 0.0;

    return {
        (static_cast<double>(best_i) + std::clamp(dx_off, -0.5, 0.5)) * cfg_.dx_m,
        (static_cast<double>(best_j) + std::clamp(dy_off, -0.5, 0.5)) * cfg_.dx_m
    };
}

double MacGridFluidSolver3D::total_dye() const noexcept {
    double sum = 0.0;
    for (double d : dye_) sum += d;
    return sum;
}

} // namespace shunya::phy
