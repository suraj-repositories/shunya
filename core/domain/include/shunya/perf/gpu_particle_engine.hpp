#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace shunya::perf {

struct GpuSphParams {
    double smoothing_h_m{0.05};
    double rest_density_kg_m3{1000.0};
    double tait_b_pa{1.5e5};
    double tait_gamma{7.0};
    double viscosity_mu_pa_s{0.005};
    core::base::math::Vector3<double> gravity_m_s2{0.0, -9.80665, 0.0};
    size_t workgroup_size{256};
    size_t hash_table_size{4096}; // Must be power of 2
};

struct alignas(16) GpuParticleState {
    double px{0.0}, py{0.0}, pz{0.0}, mass{1e-3};
    double vx{0.0}, vy{0.0}, vz{0.0}, density{1000.0};
    double ax{0.0}, ay{0.0}, az{0.0}, pressure{0.0};
};

struct GpuDispatchStats {
    size_t particle_count{0};
    size_t workgroup_count_x{0};
    size_t neighbor_checks{0};
    double last_step_ms{0.0};
};

/**
 * @brief GPU compute workgroup & spatial-hash SPH particle engine (with deterministic multi-workgroup host backend).
 */
class GpuParticleEngine {
public:
    explicit GpuParticleEngine(GpuSphParams params = {});

    void set_particles(std::vector<GpuParticleState> particles);
    [[nodiscard]] const std::vector<GpuParticleState>& particles() const noexcept { return particles_; }

    [[nodiscard]] core::base::Result<GpuDispatchStats> dispatch_step(double dt_s);
    [[nodiscard]] core::base::Result<GpuDispatchStats> dispatch_density_and_forces_only();

    [[nodiscard]] size_t compute_workgroup_count() const noexcept;
    [[nodiscard]] static std::string_view density_shader_glsl() noexcept;
    [[nodiscard]] static std::string_view forces_shader_glsl() noexcept;

private:
    void rebuild_spatial_hash();

    GpuSphParams params_;
    std::vector<GpuParticleState> particles_;
    std::vector<uint32_t> cell_start_;
    std::vector<uint32_t> cell_end_;
    std::vector<uint32_t> sorted_indices_;
    std::vector<uint32_t> particle_hashes_;
};

} // namespace shunya::perf
