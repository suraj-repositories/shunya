#include "shunya/perf/gpu_particle_engine.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <thread>

namespace shunya::perf {

namespace {

constexpr double kPi = 3.14159265358979323846;

uint32_t round_pow2_mask(size_t n) noexcept {
    size_t p = 64;
    while (p < n) p <<= 1;
    return static_cast<uint32_t>(p - 1);
}

uint32_t hash_cell_coords(int ix, int iy, int iz, uint32_t mask) noexcept {
    const uint32_t h = (static_cast<uint32_t>(ix) * 73856093u) ^
                       (static_cast<uint32_t>(iy) * 19349663u) ^
                       (static_cast<uint32_t>(iz) * 83492791u);
    return h & mask;
}

template <typename Fn>
void parallel_workgroups(size_t total_items, size_t wg_size, Fn&& fn) {
    if (total_items == 0) return;
    const size_t hw = std::max<size_t>(1, std::thread::hardware_concurrency());
    const size_t n_threads = (total_items < wg_size * 2) ? 1 : std::min<size_t>(hw, 8);
    if (n_threads <= 1) {
        fn(0, total_items);
        return;
    }
    std::vector<std::thread> workers;
    workers.reserve(n_threads - 1);
    const size_t chunk = (total_items + n_threads - 1) / n_threads;
    for (size_t t = 1; t < n_threads; ++t) {
        const size_t begin = t * chunk;
        const size_t end = std::min(total_items, begin + chunk);
        if (begin < end) {
            workers.emplace_back([&, begin, end]() { fn(begin, end); });
        }
    }
    fn(0, std::min(total_items, chunk));
    for (auto& w : workers) w.join();
}

} // namespace

GpuParticleEngine::GpuParticleEngine(GpuSphParams params) : params_(params) {}

void GpuParticleEngine::set_particles(std::vector<GpuParticleState> particles) {
    particles_ = std::move(particles);
}

size_t GpuParticleEngine::compute_workgroup_count() const noexcept {
    const size_t wg = std::max<size_t>(1, params_.workgroup_size);
    return (particles_.size() + wg - 1) / wg;
}

void GpuParticleEngine::rebuild_spatial_hash() {
    const size_t n = particles_.size();
    const uint32_t mask = round_pow2_mask(params_.hash_table_size);
    const size_t table_len = static_cast<size_t>(mask) + 1;

    cell_start_.assign(table_len, 0);
    cell_end_.assign(table_len, 0);
    sorted_indices_.resize(n);
    particle_hashes_.resize(n);

    const double inv_h = 1.0 / std::max(1e-6, params_.smoothing_h_m);
    for (size_t i = 0; i < n; ++i) {
        const int ix = static_cast<int>(std::floor(particles_[i].px * inv_h));
        const int iy = static_cast<int>(std::floor(particles_[i].py * inv_h));
        const int iz = static_cast<int>(std::floor(particles_[i].pz * inv_h));
        const uint32_t b = hash_cell_coords(ix, iy, iz, mask);
        particle_hashes_[i] = b;
        ++cell_end_[b];
    }

    uint32_t running = 0;
    for (size_t b = 0; b < table_len; ++b) {
        cell_start_[b] = running;
        running += cell_end_[b];
        cell_end_[b] = cell_start_[b];
    }

    for (size_t i = 0; i < n; ++i) {
        const uint32_t b = particle_hashes_[i];
        sorted_indices_[cell_end_[b]++] = static_cast<uint32_t>(i);
    }
}

core::base::Result<GpuDispatchStats> GpuParticleEngine::dispatch_density_and_forces_only() {
    if (params_.smoothing_h_m <= 0.0 || params_.rest_density_kg_m3 <= 0.0) {
        return core::base::Error{
            501, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid GPU SPH smoothing length or rest density", ""
        };
    }

    const auto t0 = std::chrono::steady_clock::now();
    const size_t n = particles_.size();
    GpuDispatchStats stats;
    stats.particle_count = n;
    stats.workgroup_count_x = compute_workgroup_count();
    if (n == 0) return stats;

    rebuild_spatial_hash();
    const uint32_t mask = static_cast<uint32_t>(cell_start_.size() - 1);
    const double h = params_.smoothing_h_m;
    const double h2 = h * h;
    const double inv_h = 1.0 / h;
    const double poly6_coeff = 315.0 / (64.0 * kPi * std::pow(h, 9));
    const double spiky_grad = -45.0 / (kPi * std::pow(h, 6));
    const double visc_lap = 45.0 / (kPi * std::pow(h, 6));

    // Pass 1: Density & Tait pressure summation (256-thread workgroups)
    parallel_workgroups(n, params_.workgroup_size, [&](size_t begin, size_t end) {
        std::array<uint32_t, 27> unique_buckets{};
        for (size_t i = begin; i < end; ++i) {
            const double xi = particles_[i].px;
            const double yi = particles_[i].py;
            const double zi = particles_[i].pz;
            const int cx = static_cast<int>(std::floor(xi * inv_h));
            const int cy = static_cast<int>(std::floor(yi * inv_h));
            const int cz = static_cast<int>(std::floor(zi * inv_h));

            int n_unique = 0;
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const uint32_t b = hash_cell_coords(cx + dx, cy + dy, cz + dz, mask);
                        bool dup = false;
                        for (int m = 0; m < n_unique; ++m) {
                            if (unique_buckets[static_cast<size_t>(m)] == b) {
                                dup = true;
                                break;
                            }
                        }
                        if (!dup) unique_buckets[static_cast<size_t>(n_unique++)] = b;
                    }
                }
            }

            double rho = 0.0;
            for (int m = 0; m < n_unique; ++m) {
                const uint32_t b = unique_buckets[static_cast<size_t>(m)];
                for (uint32_t k = cell_start_[b]; k < cell_end_[b]; ++k) {
                    const uint32_t j = sorted_indices_[k];
                    const double dx = xi - particles_[j].px;
                    const double dy = yi - particles_[j].py;
                    const double dz = zi - particles_[j].pz;
                    const double r2 = dx * dx + dy * dy + dz * dz;
                    if (r2 < h2) {
                        const double diff = h2 - r2;
                        rho += particles_[j].mass * poly6_coeff * diff * diff * diff;
                    }
                }
            }
            rho = std::max(rho, 1e-4);
            const double ratio = rho / params_.rest_density_kg_m3;
            const double press = std::max(0.0, params_.tait_b_pa * (std::pow(ratio, params_.tait_gamma) - 1.0));
            particles_[i].density = rho;
            particles_[i].pressure = press;
        }
    });

    // Pass 2: Pressure gradient, viscosity & gravity forces
    parallel_workgroups(n, params_.workgroup_size, [&](size_t begin, size_t end) {
        std::array<uint32_t, 27> unique_buckets{};
        for (size_t i = begin; i < end; ++i) {
            const auto& pi = particles_[i];
            const int cx = static_cast<int>(std::floor(pi.px * inv_h));
            const int cy = static_cast<int>(std::floor(pi.py * inv_h));
            const int cz = static_cast<int>(std::floor(pi.pz * inv_h));

            int n_unique = 0;
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const uint32_t b = hash_cell_coords(cx + dx, cy + dy, cz + dz, mask);
                        bool dup = false;
                        for (int m = 0; m < n_unique; ++m) {
                            if (unique_buckets[static_cast<size_t>(m)] == b) {
                                dup = true;
                                break;
                            }
                        }
                        if (!dup) unique_buckets[static_cast<size_t>(n_unique++)] = b;
                    }
                }
            }

            double fx = 0.0, fy = 0.0, fz = 0.0;
            for (int m = 0; m < n_unique; ++m) {
                const uint32_t b = unique_buckets[static_cast<size_t>(m)];
                for (uint32_t k = cell_start_[b]; k < cell_end_[b]; ++k) {
                    const uint32_t j = sorted_indices_[k];
                    if (j == i) continue;
                    const auto& pj = particles_[j];
                    const double dx = pi.px - pj.px;
                    const double dy = pi.py - pj.py;
                    const double dz = pi.pz - pj.pz;
                    const double r2 = dx * dx + dy * dy + dz * dz;
                    if (r2 > 1e-16 && r2 < h2) {
                        const double r = std::sqrt(r2);
                        const double hr = h - r;
                        const double grad_w = spiky_grad * hr * hr / r;
                        const double p_sym = pj.mass * ((pi.pressure / (pi.density * pi.density)) +
                                                        (pj.pressure / (pj.density * pj.density)));
                        const double fp = -p_sym * pi.density * grad_w;
                        fx += fp * dx;
                        fy += fp * dy;
                        fz += fp * dz;

                        const double visc_w = params_.viscosity_mu_pa_s * pj.mass * (visc_lap * hr) / pj.density;
                        fx += visc_w * (pj.vx - pi.vx);
                        fy += visc_w * (pj.vy - pi.vy);
                        fz += visc_w * (pj.vz - pi.vz);
                    }
                }
            }

            particles_[i].ax = (fx / pi.density) + params_.gravity_m_s2.x;
            particles_[i].ay = (fy / pi.density) + params_.gravity_m_s2.y;
            particles_[i].az = (fz / pi.density) + params_.gravity_m_s2.z;
        }
    });

    const auto t1 = std::chrono::steady_clock::now();
    stats.last_step_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return stats;
}

core::base::Result<GpuDispatchStats> GpuParticleEngine::dispatch_step(double dt_s) {
    if (dt_s <= 0.0 || !std::isfinite(dt_s)) {
        return core::base::Error{
            502, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "GPU SPH step dt_s must be positive", ""
        };
    }

    auto res = dispatch_density_and_forces_only();
    if (!res.is_ok()) return res;

    parallel_workgroups(particles_.size(), params_.workgroup_size, [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            auto& p = particles_[i];
            p.vx += p.ax * dt_s;
            p.vy += p.ay * dt_s;
            p.vz += p.az * dt_s;
            p.px += p.vx * dt_s;
            p.py += p.vy * dt_s;
            p.pz += p.vz * dt_s;
        }
    });

    return res;
}

std::string_view GpuParticleEngine::density_shader_glsl() noexcept {
    return R"(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
// See core/domain/shaders/sph_density.comp for full Vulkan SPIR-V source
)";
}

std::string_view GpuParticleEngine::forces_shader_glsl() noexcept {
    return R"(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
// See core/domain/shaders/sph_forces.comp for full Vulkan SPIR-V source
)";
}

} // namespace shunya::perf
