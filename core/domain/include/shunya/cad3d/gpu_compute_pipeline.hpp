#pragma once

#include "shunya/cad3d/shader_compiler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::cad3d {

/**
 * @brief 16-byte aligned particle state for GPU SSBO storage and SIMD compute workgroups.
 */
struct alignas(16) ComputeParticle {
    float pos[3]{0.0f, 0.0f, 0.0f};
    float mass{1.0f};
    float vel[3]{0.0f, 0.0f, 0.0f};
    float lifetime{10.0f};
};
static_assert(sizeof(ComputeParticle) == 32, "ComputeParticle must be 32 bytes");
static_assert(alignof(ComputeParticle) == 16, "ComputeParticle must have 16-byte alignment");

struct ParticleSimConfig {
    float gravity[3]{0.0f, -9.81f, 0.0f};
    float drag_coefficient{0.01f};
    float floor_y{0.0f};
    float restitution{0.7f};
    float dt{0.016667f};
};

struct ThermalGridConfig2D {
    uint32_t width{64};
    uint32_t height{64};
    float dx{0.01f}; // 10 mm grid spacing (m)
    float dy{0.01f}; // 10 mm grid spacing (m)
    float thermal_diffusivity{1.11e-4f}; // Copper alpha (m^2/s)
    float dt{0.001f}; // Integration time step (s)
    float ambient_temp{293.15f}; // 20 C (K)
};

struct ComputeDispatchMetrics {
    uint32_t dispatch_count{0};
    uint64_t workgroups_dispatched{0};
    uint64_t threads_invoked{0};
    uint64_t bytes_transferred{0};
};

/**
 * @brief GPU Compute Pipeline abstraction managing storage buffers (SSBOs), compute shaders,
 *        workgroup dispatches, and parallel execution for physics and thermal diffusion.
 */
class GpuComputePipeline {
public:
    GpuComputePipeline() = default;

    bool initialize(
        std::string_view compute_glsl,
        ShaderTargetApi target = ShaderTargetApi::SpirV_Vulkan,
        std::string_view entry_point = "main"
    );

    [[nodiscard]] bool is_ready() const noexcept { return ready_; }
    [[nodiscard]] const CompiledShaderModule& shader_module() const noexcept { return module_; }
    [[nodiscard]] const ComputeDispatchMetrics& metrics() const noexcept { return metrics_; }

    void bind_storage_buffer(uint32_t binding_slot, const void* data, size_t size_bytes);
    bool read_storage_buffer(uint32_t binding_slot, void* out_data, size_t size_bytes) const;

    void dispatch(uint32_t groups_x, uint32_t groups_y = 1, uint32_t groups_z = 1);

    static std::string_view particle_physics_compute_glsl() noexcept;
    static std::string_view thermal_diffusion_compute_glsl() noexcept;

private:
    bool ready_{false};
    CompiledShaderModule module_{};
    ComputeDispatchMetrics metrics_{};
    std::unordered_map<uint32_t, std::vector<uint8_t>> storage_buffers_;
};

/**
 * @brief High-performance evaluator and parallel executor for particle dynamics.
 */
class ParticleComputeSimulator {
public:
    bool initialize(ShaderTargetApi target = ShaderTargetApi::SpirV_Vulkan);
    void step_compute(std::span<ComputeParticle> particles, const ParticleSimConfig& config);
    static void step_cpu_reference(std::span<ComputeParticle> particles, const ParticleSimConfig& config) noexcept;

    [[nodiscard]] const GpuComputePipeline& pipeline() const noexcept { return pipeline_; }

private:
    GpuComputePipeline pipeline_;
};

/**
 * @brief High-performance evaluator and parallel executor for Fourier thermal field diffusion.
 */
class ThermalDiffusionComputeSimulator {
public:
    bool initialize(ShaderTargetApi target = ShaderTargetApi::SpirV_Vulkan);
    void step_compute(
        std::span<const float> current_field,
        std::span<float> next_field,
        const ThermalGridConfig2D& config,
        std::span<const float> heat_sources = {}
    );
    static void step_cpu_reference(
        std::span<const float> current_field,
        std::span<float> next_field,
        const ThermalGridConfig2D& config,
        std::span<const float> heat_sources = {}
    ) noexcept;

    [[nodiscard]] const GpuComputePipeline& pipeline() const noexcept { return pipeline_; }

private:
    GpuComputePipeline pipeline_;
};

} // namespace shunya::cad3d
