#pragma once

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace shunya::cad3d {

enum class RhiCompareOp {
    Less,
    LessEqual,
    Greater,
    Always
};

enum class RhiBlendMode {
    Opaque,
    AlphaBlend,
    WeightedOitAccum
};

enum class RhiCullMode {
    None,
    Back,
    Front
};

struct PipelineStateDesc {
    std::string name{"default_pso"};
    bool depth_test{true};
    bool depth_write{true};
    RhiCompareOp depth_op{RhiCompareOp::Less};
    RhiBlendMode blend_mode{RhiBlendMode::Opaque};
    RhiCullMode cull_mode{RhiCullMode::Back};
};

struct RhiSamplerDesc {
    std::string name{"linear_clamp"};
    bool linear_filter{true};
    bool clamp_to_edge{true};
    bool compare_enable{false};
};

struct RhiRenderTarget {
    uint32_t width{256};
    uint32_t height{256};
    std::vector<std::array<double, 4>> color_rgba; // linear RGBA floating-point target
    std::vector<double> depth;

    void resize(uint32_t w, uint32_t h) {
        width = std::max<uint32_t>(1, w);
        height = std::max<uint32_t>(1, h);
        const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height);
        color_rgba.assign(n, {0.0, 0.0, 0.0, 1.0});
        depth.assign(n, 1.0);
    }

    void clear(const std::array<double, 4>& rgba = {0.08, 0.09, 0.12, 1.0}, double d = 1.0) {
        if (color_rgba.size() != static_cast<size_t>(width) * height) {
            resize(width, height);
        }
        std::fill(color_rgba.begin(), color_rgba.end(), rgba);
        std::fill(depth.begin(), depth.end(), d);
    }

    [[nodiscard]] uint64_t quantized_checksum() const noexcept {
        uint64_t hash = 14695981039346656037ULL;
        for (const auto& px : color_rgba) {
            for (int c = 0; c < 4; ++c) {
                const uint32_t q = static_cast<uint32_t>(std::clamp(px[c], 0.0, 1.0) * 255.0 + 0.5);
                hash ^= q;
                hash *= 1099511628211ULL;
            }
        }
        return hash;
    }
};

struct RenderPassNode {
    std::string name;
    PipelineStateDesc pso;
    bool clear_color{true};
    bool clear_depth{true};
};

class RenderPassGraph {
public:
    void add_pass(RenderPassNode node) { passes_.push_back(std::move(node)); }
    void clear() noexcept { passes_.clear(); }
    [[nodiscard]] const std::vector<RenderPassNode>& passes() const noexcept { return passes_; }
    [[nodiscard]] size_t pass_count() const noexcept { return passes_.size(); }

private:
    std::vector<RenderPassNode> passes_;
};

} // namespace shunya::cad3d
