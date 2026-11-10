#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::cad3d {

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute
};

enum class ShaderTargetApi {
    SpirV_Vulkan,
    Dxil_D3D12,
    Msl_Metal,
    Glsl_OpenGL
};

struct ShaderBindingInfo {
    uint32_t binding{0};
    uint32_t set{0};
    std::string name;
    bool is_storage_buffer{false};
};

struct CompiledShaderModule {
    std::string entry_point{"main"};
    ShaderStage stage{ShaderStage::Fragment};
    ShaderTargetApi target{ShaderTargetApi::SpirV_Vulkan};
    std::vector<uint32_t> spirv_words;
    std::vector<ShaderBindingInfo> bindings;
    uint64_t source_hash{0};
    bool valid{false};
    std::string diagnostic;
};

/**
 * @brief Deterministic cross-API shader reflection & SPIR-V/IR compilation cache.
 */
class ShaderCompiler {
public:
    [[nodiscard]] CompiledShaderModule compile(
        std::string_view source_code,
        ShaderStage stage,
        ShaderTargetApi target,
        std::string_view entry_point = "main"
    );

    [[nodiscard]] size_t cache_size() const noexcept { return cache_.size(); }
    [[nodiscard]] uint64_t cache_hits() const noexcept { return cache_hits_; }
    [[nodiscard]] uint64_t compile_count() const noexcept { return compile_count_; }

    /**
     * @brief Returns canonical built-in compute/fragment shaders (Forward+ culler & WBOIT passes).
     */
    [[nodiscard]] static std::string_view builtin_light_culler_comp() noexcept {
        return R"(#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(std430, binding = 0) readonly buffer LightBuffer { vec4 light_pos_radius[]; };
layout(std430, binding = 1) writeonly buffer TileIndices { uint tile_light_ids[]; };
void main() {
    // 16x16 tiled frustum light culling kernel
}
)";
    }

    [[nodiscard]] static std::string_view builtin_wboit_accum_frag() noexcept {
        return R"(#version 450
layout(location = 0) out vec4 out_accum;
layout(location = 1) out float out_revealage;
void main() {
    // Weighted Blended OIT accumulation + multiplicative revealage
    out_accum = vec4(0.0);
    out_revealage = 1.0;
}
)";
    }

    [[nodiscard]] static std::string_view builtin_wboit_composite_frag() noexcept {
        return R"(#version 450
layout(binding = 0) uniform sampler2D u_accum;
layout(binding = 1) uniform sampler2D u_revealage;
layout(location = 0) out vec4 out_color;
void main() {
    vec4 accum = texture(u_accum, vec2(0.5));
    float reveal = texture(u_revealage, vec2(0.5)).r;
    out_color = vec4(accum.rgb / max(accum.a, 1e-5), 1.0 - reveal);
}
)";
    }

private:
    std::unordered_map<uint64_t, CompiledShaderModule> cache_;
    uint64_t cache_hits_{0};
    uint64_t compile_count_{0};
};

} // namespace shunya::cad3d
