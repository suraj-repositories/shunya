#pragma once

#include "shunya/cad3d/render_pass.hpp"
#include "shunya/cad3d/shader_compiler.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/domain/rendering.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::cad3d {

enum class RhiBackendApi {
    Vulkan,
    Direct3D12,
    Metal,
    OpenGL,
    SoftwareReference
};

struct RhiSwapchainConfig {
    RhiBackendApi backend{RhiBackendApi::Vulkan};
    uint32_t width{320};
    uint32_t height{240};
    bool enable_depth{true};
};

struct RhiDrawItem {
    const SolidMesh3D* mesh{nullptr};
    core::base::math::Matrix4x4<double> model_matrix{core::base::math::Matrix4x4<double>::identity()};
    std::array<double, 4> base_color{0.75, 0.78, 0.85, 1.0};
    double roughness{0.4};
    double metallic{0.0};
};

struct RhiFrameMetrics {
    uint32_t draw_calls{0};
    uint32_t triangles_submitted{0};
    uint32_t pixels_shaded{0};
    uint32_t pso_binds{0};
};

/**
 * @brief Unified RHI abstraction managing swapchain targets, PSOs, descriptor/uniform pools,
 *        sampler caches, and deterministic parity rendering across Vulkan / D3D12 / Metal.
 */
class RhiRendererEngine {
public:
    bool initialize(const RhiSwapchainConfig& config);
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }
    [[nodiscard]] RhiBackendApi active_backend() const noexcept { return config_.backend; }

    [[nodiscard]] uint32_t create_pso(const PipelineStateDesc& desc);
    [[nodiscard]] uint32_t get_or_create_sampler(const RhiSamplerDesc& desc);

    template <typename T>
    uint32_t allocate_uniform_buffer(const T& data) {
        std::vector<uint8_t> bytes(sizeof(T));
        const auto* raw = reinterpret_cast<const uint8_t*>(&data);
        std::copy(raw, raw + sizeof(T), bytes.begin());
        uniform_pool_.push_back(std::move(bytes));
        return static_cast<uint32_t>(uniform_pool_.size() - 1);
    }

    [[nodiscard]] const std::vector<uint8_t>* read_uniform_buffer(uint32_t handle) const noexcept {
        return (handle < uniform_pool_.size()) ? &uniform_pool_[handle] : nullptr;
    }

    void begin_frame(const std::array<double, 4>& clear_rgba = {0.06, 0.08, 0.11, 1.0});
    void bind_pso(uint32_t pso_id);
    void render_scene(
        const core::domain::ViewportCamera& camera,
        const std::vector<RhiDrawItem>& draw_items,
        const Vec3d& light_dir = Vec3d{0.5, 0.8, 0.6}.normalized()
    );
    void end_frame();

    [[nodiscard]] const RhiRenderTarget& framebuffer() const noexcept { return framebuffer_; }
    [[nodiscard]] const RhiFrameMetrics& frame_metrics() const noexcept { return metrics_; }
    [[nodiscard]] ShaderCompiler& shader_compiler() noexcept { return compiler_; }
    [[nodiscard]] RenderPassGraph& pass_graph() noexcept { return pass_graph_; }

private:
    RhiSwapchainConfig config_{};
    bool initialized_{false};
    RhiRenderTarget framebuffer_{};
    ShaderCompiler compiler_{};
    RenderPassGraph pass_graph_{};
    std::vector<PipelineStateDesc> pso_table_;
    std::unordered_map<std::string, uint32_t> sampler_cache_;
    std::vector<std::vector<uint8_t>> uniform_pool_;
    uint32_t bound_pso_{0};
    RhiFrameMetrics metrics_{};
};

} // namespace shunya::cad3d
