#include "shunya/phy/sph_solver_3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace shunya::phy {

using core::base::math::Vector3;

namespace {

int64_t pack_cell_key(int ix, int iy, int iz) noexcept {
    const uint64_t ux = static_cast<uint64_t>(ix + 32768) & 0xFFFFFULL;
    const uint64_t uy = static_cast<uint64_t>(iy + 32768) & 0xFFFFFULL;
    const uint64_t uz = static_cast<uint64_t>(iz + 32768) & 0xFFFFFULL;
    return static_cast<int64_t>((ux << 40) | (uy << 20) | uz);
}

Vector3<double> periodic_delta(
    const Vector3<double>& a,
    const Vector3<double>& b,
    double px,
    double pz) noexcept {
    Vector3<double> d = a - b;
    if (px > 0.0) {
        d.x -= px * std::round(d.x / px);
    }
    if (pz > 0.0) {
        d.z -= pz * std::round(d.z / pz);
    }
    return d;
}

double wrap_coord(double val, double period) noexcept {
    if (period <= 0.0) return val;
    double res = std::fmod(val, period);
    if (res < 0.0) res += period;
    return res;
}

} // namespace

void BoundaryParticleGenerator::add_box_floor_and_walls(
    std::vector<SphBoundaryParticle>& out_boundary,
    const Vector3<double>& min_pt,
    const Vector3<double>& max_pt,
    double spacing_m,
    bool include_floor,
    bool include_x_walls,
    bool include_z_walls) {

    const double vol = spacing_m * spacing_m * spacing_m;
    const int nx = std::max(1, static_cast<int>(std::round((max_pt.x - min_pt.x) / spacing_m)));
    const int ny = std::max(1, static_cast<int>(std::round((max_pt.y - min_pt.y) / spacing_m)));
    const int nz = std::max(1, static_cast<int>(std::round((max_pt.z - min_pt.z) / spacing_m)));

    // 3 boundary layers (0, -s, -2s) outward-facing into domain prevent density deficit and leakage
    for (int layer = 0; layer < 3; ++layer) {
        const double off = static_cast<double>(layer) * spacing_m;
        if (include_floor) {
            for (int ix = -2; ix <= nx + 2; ++ix) {
                const double x = min_pt.x + ix * spacing_m;
                for (int iz = -2; iz <= nz + 2; ++iz) {
                    const double z = min_pt.z + iz * spacing_m;
                    out_boundary.push_back({{x, min_pt.y - off, z}, {0, 1, 0}, {0, 0, 0}, vol});
                }
            }
        }
        if (include_x_walls) {
            for (int iy = 1; iy <= ny; ++iy) {
                const double y = min_pt.y + iy * spacing_m;
                for (int iz = -2; iz <= nz + 2; ++iz) {
                    const double z = min_pt.z + iz * spacing_m;
                    out_boundary.push_back({{min_pt.x - off, y, z}, { 1, 0, 0}, {0, 0, 0}, vol});
                    out_boundary.push_back({{max_pt.x + off, y, z}, {-1, 0, 0}, {0, 0, 0}, vol});
                }
            }
        }
        if (include_z_walls) {
            for (int iy = 1; iy <= ny; ++iy) {
                const double y = min_pt.y + iy * spacing_m;
                for (int ix = 1; ix < nx; ++ix) {
                    const double x = min_pt.x + ix * spacing_m;
                    out_boundary.push_back({{x, y, min_pt.z - off}, {0, 0,  1}, {0, 0, 0}, vol});
                    out_boundary.push_back({{x, y, max_pt.z + off}, {0, 0, -1}, {0, 0, 0}, vol});
                }
            }
        }
    }
}

SphSolver3D::SphSolver3D(SphSimulationConfig cfg) : cfg_(std::move(cfg)) {}

void SphSolver3D::add_fluid_box(const Vector3<double>& min_pt, const Vector3<double>& max_pt) {
    const double dx = cfg_.spacing_m;
    const double pmass = cfg_.rest_density_kg_m3 * dx * dx * dx;
    for (double x = min_pt.x + 0.5 * dx; x < max_pt.x; x += dx) {
        for (double y = min_pt.y + 0.5 * dx; y < max_pt.y; y += dx) {
            for (double z = min_pt.z + 0.5 * dx; z < max_pt.z; z += dx) {
                SphParticle p;
                p.position = {x, y, z};
                p.mass_kg = pmass;
                p.density_kg_m3 = cfg_.rest_density_kg_m3;
                particles_.push_back(p);
            }
        }
    }
}

void SphSolver3D::add_boundary_particles(std::vector<SphBoundaryParticle> bpts) {
    boundary_.insert(boundary_.end(), bpts.begin(), bpts.end());
}

double SphSolver3D::tait_pressure(double rho) const noexcept {
    const double ratio = std::max(0.0, rho / std::max(1.0, cfg_.rest_density_kg_m3));
    const double B = (cfg_.rest_density_kg_m3 * cfg_.speed_of_sound_m_s * cfg_.speed_of_sound_m_s) /
                     std::max(1.0, cfg_.tait_gamma);
    const double p = B * (std::pow(ratio, cfg_.tait_gamma) - 1.0);
    return std::max(0.0, p);
}

core::base::Result<void> SphSolver3D::step(double dt_s) {
    if (dt_s <= 0.0) {
        return core::base::Result<void>::err(
            core::base::Error{80, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "SPH time step dt_s must be positive", ""});
    }
    if (particles_.empty()) return core::base::Result<void>::ok();

    const double h = cfg_.smoothing_h_m;
    const double h_sup = 2.0 * h;
    const double inv_cell = 1.0 / h_sup;
    const size_t nf = particles_.size();
    const size_t nb = boundary_.size();

    // Build spatial hash grids for fluid and boundary particles
    std::unordered_map<int64_t, std::vector<size_t>> fluid_grid;
    std::unordered_map<int64_t, std::vector<size_t>> bound_grid;
    fluid_grid.reserve(nf * 2);
    for (size_t i = 0; i < nf; ++i) {
        const auto& pos = particles_[i].position;
        const int ix = static_cast<int>(std::floor(pos.x * inv_cell));
        const int iy = static_cast<int>(std::floor(pos.y * inv_cell));
        const int iz = static_cast<int>(std::floor(pos.z * inv_cell));
        fluid_grid[pack_cell_key(ix, iy, iz)].push_back(i);
    }
    for (size_t b = 0; b < nb; ++b) {
        const auto& pos = boundary_[b].position;
        const int ix = static_cast<int>(std::floor(pos.x * inv_cell));
        const int iy = static_cast<int>(std::floor(pos.y * inv_cell));
        const int iz = static_cast<int>(std::floor(pos.z * inv_cell));
        bound_grid[pack_cell_key(ix, iy, iz)].push_back(b);
    }

    const int n_px = (cfg_.periodic_x_m > 0.0) ? std::max(1, static_cast<int>(std::floor(cfg_.periodic_x_m / h_sup))) : 0;
    const int n_pz = (cfg_.periodic_z_m > 0.0) ? std::max(1, static_cast<int>(std::floor(cfg_.periodic_z_m / h_sup))) : 0;
    const double inv_cx = (n_px > 0) ? (static_cast<double>(n_px) / cfg_.periodic_x_m) : inv_cell;
    const double inv_cy = inv_cell;
    const double inv_cz = (n_pz > 0) ? (static_cast<double>(n_pz) / cfg_.periodic_z_m) : inv_cell;

    auto wrap_cell = [](int idx, int period_cells) -> int {
        if (period_cells <= 0) return idx;
        int r = idx % period_cells;
        return (r < 0) ? (r + period_cells) : r;
    };

    // Build unique dx/dz stencil offsets to avoid double-visiting cells on small periodic boxes
    std::vector<int> dx_offsets = (n_px == 1) ? std::vector<int>{0} :
                                  (n_px == 2) ? std::vector<int>{0, 1} :
                                                std::vector<int>{-1, 0, 1};
    std::vector<int> dz_offsets = (n_pz == 1) ? std::vector<int>{0} :
                                  (n_pz == 2) ? std::vector<int>{0, 1} :
                                                std::vector<int>{-1, 0, 1};

    // Rebuild spatial hash grids with exact cell dimensions
    fluid_grid.clear();
    bound_grid.clear();
    for (size_t i = 0; i < nf; ++i) {
        const auto& pos = particles_[i].position;
        const int ix = wrap_cell(static_cast<int>(std::floor(pos.x * inv_cx)), n_px);
        const int iy = static_cast<int>(std::floor(pos.y * inv_cy));
        const int iz = wrap_cell(static_cast<int>(std::floor(pos.z * inv_cz)), n_pz);
        fluid_grid[pack_cell_key(ix, iy, iz)].push_back(i);
    }
    for (size_t b = 0; b < nb; ++b) {
        const auto& pos = boundary_[b].position;
        const int ix = wrap_cell(static_cast<int>(std::floor(pos.x * inv_cx)), n_px);
        const int iy = static_cast<int>(std::floor(pos.y * inv_cy));
        const int iz = wrap_cell(static_cast<int>(std::floor(pos.z * inv_cz)), n_pz);
        bound_grid[pack_cell_key(ix, iy, iz)].push_back(b);
    }

    // 1) Density & Tait Pressure evaluation
    for (size_t i = 0; i < nf; ++i) {
        auto& pi = particles_[i];
        const int ix0 = wrap_cell(static_cast<int>(std::floor(pi.position.x * inv_cx)), n_px);
        const int iy0 = static_cast<int>(std::floor(pi.position.y * inv_cy));
        const int iz0 = wrap_cell(static_cast<int>(std::floor(pi.position.z * inv_cz)), n_pz);

        double rho = 0.0;
        for (int dx : dx_offsets) {
            const int qx = wrap_cell(ix0 + dx, n_px);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz : dz_offsets) {
                    const int qz = wrap_cell(iz0 + dz, n_pz);
                    if (auto it = fluid_grid.find(pack_cell_key(qx, iy0 + dy, qz)); it != fluid_grid.end()) {
                        for (size_t j : it->second) {
                            const double r = periodic_delta(pi.position, particles_[j].position, cfg_.periodic_x_m, cfg_.periodic_z_m).length();
                            if (r < h_sup) {
                                rho += particles_[j].mass_kg * SphKernels::wendland_c2_3d(r, h);
                            }
                        }
                    }
                    if (auto itb = bound_grid.find(pack_cell_key(qx, iy0 + dy, qz)); itb != bound_grid.end()) {
                        for (size_t b : itb->second) {
                            const double r = periodic_delta(pi.position, boundary_[b].position, cfg_.periodic_x_m, cfg_.periodic_z_m).length();
                            if (r < h_sup) {
                                const double mb = cfg_.rest_density_kg_m3 * boundary_[b].volume_m3;
                                rho += mb * SphKernels::wendland_c2_3d(r, h);
                            }
                        }
                    }
                }
            }
        }
        pi.density_kg_m3 = std::max(0.5 * cfg_.rest_density_kg_m3, rho);
        pi.pressure_pa = tait_pressure(pi.density_kg_m3);
    }

    // 2) Pressure, Viscosity, Surface Tension, Gravity & Wall Interaction Forces
    for (size_t i = 0; i < nf; ++i) {
        auto& pi = particles_[i];
        const int ix0 = wrap_cell(static_cast<int>(std::floor(pi.position.x * inv_cx)), n_px);
        const int iy0 = static_cast<int>(std::floor(pi.position.y * inv_cy));
        const int iz0 = wrap_cell(static_cast<int>(std::floor(pi.position.z * inv_cz)), n_pz);

        Vector3<double> acc = cfg_.gravity_m_s2 + cfg_.external_body_accel;
        const double pr_i = pi.pressure_pa / (pi.density_kg_m3 * pi.density_kg_m3);
        const double p_floor = 0.05 * cfg_.rest_density_kg_m3 * cfg_.speed_of_sound_m_s * cfg_.speed_of_sound_m_s /
                               (cfg_.rest_density_kg_m3 * cfg_.rest_density_kg_m3);

        for (int dx : dx_offsets) {
            const int qx = wrap_cell(ix0 + dx, n_px);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz : dz_offsets) {
                    const int qz = wrap_cell(iz0 + dz, n_pz);
                    if (auto it = fluid_grid.find(pack_cell_key(qx, iy0 + dy, qz)); it != fluid_grid.end()) {
                        for (size_t j : it->second) {
                            if (j == i) continue;
                            const auto& pj = particles_[j];
                            const Vector3<double> r_ij = periodic_delta(pi.position, pj.position, cfg_.periodic_x_m, cfg_.periodic_z_m);
                            const double r = r_ij.length();
                            if (r >= h_sup || r < 1e-12) continue;

                            const Vector3<double> gw = SphKernels::wendland_c2_3d_grad(r_ij, h);
                            const double pr_j = pj.pressure_pa / (pj.density_kg_m3 * pj.density_kg_m3);
                            acc -= gw * (pj.mass_kg * (pr_i + pr_j));

                            acc += SphViscosityModule::compute_pair_viscous_accel(
                                r_ij, pi.velocity - pj.velocity, gw,
                                pj.mass_kg, pi.density_kg_m3, pj.density_kg_m3, h, cfg_.viscosity);

                            if (cfg_.enable_surface_tension) {
                                acc += SphSurfaceTensionModule::compute_cohesion_accel(
                                    r_ij, pj.mass_kg, pi.density_kg_m3, pj.density_kg_m3,
                                    cfg_.rest_density_kg_m3, h, cfg_.surface_tension.surface_tension_n_m);
                            }
                        }
                    }
                    if (auto itb = bound_grid.find(pack_cell_key(qx, iy0 + dy, qz)); itb != bound_grid.end()) {
                        for (size_t b : itb->second) {
                            const auto& pb = boundary_[b];
                            const Vector3<double> r_ib = periodic_delta(pi.position, pb.position, cfg_.periodic_x_m, cfg_.periodic_z_m);
                            const double r = r_ib.length();
                            if (r >= h_sup || r < 1e-12) continue;

                            const double mb = cfg_.rest_density_kg_m3 * pb.volume_m3;
                            const Vector3<double> gw = SphKernels::wendland_c2_3d_grad(r_ib, h);
                            const double pr_eff = (r < 0.75 * cfg_.spacing_m) ? std::max(pr_i, p_floor) : pr_i;
                            acc -= gw * (mb * (2.0 * pr_eff));

                            // No-slip wall viscous interaction
                            acc += SphViscosityModule::compute_pair_viscous_accel(
                                r_ib, pi.velocity - pb.wall_velocity, gw,
                                mb, pi.density_kg_m3, cfg_.rest_density_kg_m3, h, cfg_.viscosity);
                        }
                    }
                }
            }
        }
        pi.accel = acc;
    }

    // 3) Symplectic Euler velocity-position update
    for (auto& pi : particles_) {
        pi.velocity += pi.accel * dt_s;
        pi.position += pi.velocity * dt_s;
        if (cfg_.periodic_x_m > 0.0) pi.position.x = wrap_coord(pi.position.x, cfg_.periodic_x_m);
        if (cfg_.periodic_z_m > 0.0) pi.position.z = wrap_coord(pi.position.z, cfg_.periodic_z_m);
    }

    return core::base::Result<void>::ok();
}

core::base::Result<void> SphSolver3D::advance(double total_time_s, double dt_s) {
    if (total_time_s <= 0.0 || dt_s <= 0.0) {
        return core::base::Result<void>::err(
            core::base::Error{81, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "SPH total_time_s and dt_s must be positive", ""});
    }
    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_s));
    const double step_dt = total_time_s / static_cast<double>(std::max<size_t>(1, steps));
    for (size_t s = 0; s < steps; ++s) {
        auto r = step(step_dt);
        if (!r.is_ok()) return r;
    }
    return core::base::Result<void>::ok();
}

double SphSolver3D::max_x_extent() const noexcept {
    double mx = -1e18;
    for (const auto& p : particles_) mx = std::max(mx, p.position.x);
    return mx;
}

Vector3<double> SphSolver3D::bounding_box_extent() const noexcept {
    if (particles_.empty()) return {0, 0, 0};
    Vector3<double> mn = particles_[0].position;
    Vector3<double> mx = particles_[0].position;
    for (const auto& p : particles_) {
        mn.x = std::min(mn.x, p.position.x);
        mn.y = std::min(mn.y, p.position.y);
        mn.z = std::min(mn.z, p.position.z);
        mx.x = std::max(mx.x, p.position.x);
        mx.y = std::max(mx.y, p.position.y);
        mx.z = std::max(mx.z, p.position.z);
    }
    return mx - mn;
}

} // namespace shunya::phy
