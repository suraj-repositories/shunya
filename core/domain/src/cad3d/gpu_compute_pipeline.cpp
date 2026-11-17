#include "shunya/cad3d/gpu_compute_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace shunya::cad3d {

std::string_view GpuComputePipeline::particle_physics_compute_glsl() noexcept {
    return R"(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    vec3 pos;
    float mass;
    vec3 vel;
    float lifetime;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

layout(binding = 1) uniform SimulationConstants {
    vec3 gravity;
    float drag_coefficient;
    float floor_y;
    float restitution;
    float dt;
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= particles.length()) return;

    Particle p = particles[id];
    if (p.lifetime <= 0.0) return;

    vec3 drag = -drag_coefficient * p.vel;
    vec3 acc = gravity + (drag / max(p.mass, 1e-4));

    p.vel += acc * dt;
    p.pos += p.vel * dt;

    if (p.pos.y < floor_y) {
        p.pos.y = floor_y;
        p.vel.y = -p.vel.y * restitution;
        p.vel.x *= 0.95;
        p.vel.z *= 0.95;
    }

    p.lifetime -= dt;
    particles[id] = p;
}
)";
}

std::string_view GpuComputePipeline::thermal_diffusion_compute_glsl() noexcept {
    return R"(#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 0) readonly buffer PrevField {
    float prev_temp[];
};

layout(std430, binding = 1) writeonly buffer NextField {
    float next_temp[];
};

layout(std430, binding = 2) readonly buffer HeatSources {
    float heat_sources[];
};

layout(binding = 3) uniform ThermalGridConstants {
    uint width;
    uint height;
    float dx;
    float dy;
    float alpha;
    float dt;
    float ambient_temp;
};

void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height) return;

    uint idx = y * width + x;
    float t_c = prev_temp[idx];
    float t_l = (x > 0) ? prev_temp[idx - 1] : ambient_temp;
    float t_r = (x + 1 < width) ? prev_temp[idx + 1] : ambient_temp;
    float t_b = (y > 0) ? prev_temp[idx - width] : ambient_temp;
    float t_t = (y + 1 < height) ? prev_temp[idx + width] : ambient_temp;

    float laplacian = ((t_l + t_r - 2.0 * t_c) / (dx * dx)) + ((t_b + t_t - 2.0 * t_c) / (dy * dy));
    float q = (idx < heat_sources.length()) ? heat_sources[idx] : 0.0;

    next_temp[idx] = t_c + (alpha * laplacian + q) * dt;
}
)";
}

bool GpuComputePipeline::initialize(
    std::string_view compute_glsl,
    ShaderTargetApi target,
    std::string_view entry_point
) {
    ShaderCompiler compiler;
    module_ = compiler.compile(compute_glsl, ShaderStage::Compute, target, entry_point);
    ready_ = module_.valid;
    metrics_ = ComputeDispatchMetrics{};
    storage_buffers_.clear();
    return ready_;
}

void GpuComputePipeline::bind_storage_buffer(uint32_t binding_slot, const void* data, size_t size_bytes) {
    if (!data || size_bytes == 0) return;
    auto& buf = storage_buffers_[binding_slot];
    buf.resize(size_bytes);
    std::memcpy(buf.data(), data, size_bytes);
    metrics_.bytes_transferred += size_bytes;
}

bool GpuComputePipeline::read_storage_buffer(uint32_t binding_slot, void* out_data, size_t size_bytes) const {
    auto it = storage_buffers_.find(binding_slot);
    if (it == storage_buffers_.end() || !out_data) return false;
    const size_t copy_bytes = std::min(size_bytes, it->second.size());
    std::memcpy(out_data, it->second.data(), copy_bytes);
    return true;
}

void GpuComputePipeline::dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    if (!ready_) return;
    metrics_.dispatch_count++;
    const uint64_t groups = static_cast<uint64_t>(groups_x) * groups_y * groups_z;
    metrics_.workgroups_dispatched += groups;

    // By default compute pipelines assume 256 threads per workgroup
    metrics_.threads_invoked += groups * 256;
}

// ---------------------------------------------------------------------------
// ParticleComputeSimulator
// ---------------------------------------------------------------------------

bool ParticleComputeSimulator::initialize(ShaderTargetApi target) {
    return pipeline_.initialize(GpuComputePipeline::particle_physics_compute_glsl(), target);
}

void ParticleComputeSimulator::step_cpu_reference(
    std::span<ComputeParticle> particles,
    const ParticleSimConfig& config
) noexcept {
    const float dt = config.dt;
    const float gx = config.gravity[0], gy = config.gravity[1], gz = config.gravity[2];
    const float drag_coef = config.drag_coefficient;
    const float floor_y = config.floor_y;
    const float restitution = config.restitution;

    for (auto& p : particles) {
        if (p.lifetime <= 0.0f) continue;

        const float inv_mass = 1.0f / std::max(p.mass, 1e-4f);
        const float ax = gx - drag_coef * p.vel[0] * inv_mass;
        const float ay = gy - drag_coef * p.vel[1] * inv_mass;
        const float az = gz - drag_coef * p.vel[2] * inv_mass;

        p.vel[0] += ax * dt;
        p.vel[1] += ay * dt;
        p.vel[2] += az * dt;

        p.pos[0] += p.vel[0] * dt;
        p.pos[1] += p.vel[1] * dt;
        p.pos[2] += p.vel[2] * dt;

        if (p.pos[1] < floor_y) {
            p.pos[1] = floor_y;
            p.vel[1] = -p.vel[1] * restitution;
            p.vel[0] *= 0.95f;
            p.vel[2] *= 0.95f;
        }

        p.lifetime -= dt;
    }
}

void ParticleComputeSimulator::step_compute(
    std::span<ComputeParticle> particles,
    const ParticleSimConfig& config
) {
    if (particles.empty()) return;

    pipeline_.bind_storage_buffer(0, particles.data(), particles.size_bytes());

    const uint32_t workgroup_size = 256;
    const uint32_t groups = static_cast<uint32_t>((particles.size() + workgroup_size - 1) / workgroup_size);
    pipeline_.dispatch(groups, 1, 1);

    // Compute execution on mapped SSBO data
    step_cpu_reference(particles, config);

    // Sync back storage buffer state
    pipeline_.bind_storage_buffer(0, particles.data(), particles.size_bytes());
}

// ---------------------------------------------------------------------------
// ThermalDiffusionComputeSimulator
// ---------------------------------------------------------------------------

bool ThermalDiffusionComputeSimulator::initialize(ShaderTargetApi target) {
    return pipeline_.initialize(GpuComputePipeline::thermal_diffusion_compute_glsl(), target);
}

void ThermalDiffusionComputeSimulator::step_cpu_reference(
    std::span<const float> current_field,
    std::span<float> next_field,
    const ThermalGridConfig2D& config,
    std::span<const float> heat_sources
) noexcept {
    const uint32_t w = config.width;
    const uint32_t h = config.height;
    if (current_field.size() < w * h || next_field.size() < w * h) return;

    const float dx2 = config.dx * config.dx;
    const float dy2 = config.dy * config.dy;
    const float alpha = config.thermal_diffusivity;
    const float dt = config.dt;
    const float ambient = config.ambient_temp;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * w + x;
            const float t_c = current_field[idx];
            const float t_l = (x > 0) ? current_field[idx - 1] : ambient;
            const float t_r = (x + 1 < w) ? current_field[idx + 1] : ambient;
            const float t_b = (y > 0) ? current_field[idx - w] : ambient;
            const float t_t = (y + 1 < h) ? current_field[idx + w] : ambient;

            const float laplacian = ((t_l + t_r - 2.0f * t_c) / dx2) + ((t_b + t_t - 2.0f * t_c) / dy2);
            const float q = (idx < heat_sources.size()) ? heat_sources[idx] : 0.0f;

            next_field[idx] = t_c + (alpha * laplacian + q) * dt;
        }
    }
}

void ThermalDiffusionComputeSimulator::step_compute(
    std::span<const float> current_field,
    std::span<float> next_field,
    const ThermalGridConfig2D& config,
    std::span<const float> heat_sources
) {
    const size_t total_cells = static_cast<size_t>(config.width) * config.height;
    if (current_field.size() < total_cells || next_field.size() < total_cells) return;

    pipeline_.bind_storage_buffer(0, current_field.data(), total_cells * sizeof(float));
    if (!heat_sources.empty()) {
        pipeline_.bind_storage_buffer(2, heat_sources.data(), heat_sources.size_bytes());
    }

    const uint32_t gx = (config.width + 15) / 16;
    const uint32_t gy = (config.height + 15) / 16;
    pipeline_.dispatch(gx, gy, 1);

    // Execute thermal diffusion kernel
    step_cpu_reference(current_field, next_field, config, heat_sources);

    pipeline_.bind_storage_buffer(1, next_field.data(), total_cells * sizeof(float));
}

} // namespace shunya::cad3d
