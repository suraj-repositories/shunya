#include "shunya/cad3d/rhi_renderer_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace shunya::cad3d {

namespace {

uint64_t fnv1a_hash(std::string_view sv) noexcept {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : sv) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

struct ClipVert {
    double x{0}, y{0}, z{0}, w{1};
};

ClipVert transform_point(const core::base::math::Matrix4x4<double>& mat, const Point3D& p) noexcept {
    const auto& m = mat.m;
    return {
        m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3],
        m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
        m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11],
        m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15]
    };
}

Point3D transform_pos(const core::base::math::Matrix4x4<double>& mat, const Point3D& p) noexcept {
    const ClipVert c = transform_point(mat, p);
    return {c.x, c.y, c.z};
}

Vec3d transform_dir(const core::base::math::Matrix4x4<double>& mat, const Vec3d& v) noexcept {
    const auto& m = mat.m;
    return Vec3d{
        m[0] * v.x + m[1] * v.y + m[2] * v.z,
        m[4] * v.x + m[5] * v.y + m[6] * v.z,
        m[8] * v.x + m[9] * v.y + m[10] * v.z
    }.normalized();
}

} // namespace

CompiledShaderModule ShaderCompiler::compile(
    std::string_view source_code,
    ShaderStage stage,
    ShaderTargetApi target,
    std::string_view entry_point
) {
    uint64_t key = fnv1a_hash(source_code);
    key ^= static_cast<uint64_t>(stage) * 0x9e3779b97f4a7c15ULL;
    key ^= static_cast<uint64_t>(target) * 0xbf58476d1ce4e5b9ULL;
    key ^= fnv1a_hash(entry_point);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        ++cache_hits_;
        return it->second;
    }

    ++compile_count_;
    CompiledShaderModule mod;
    mod.entry_point = std::string(entry_point);
    mod.stage = stage;
    mod.target = target;
    mod.source_hash = fnv1a_hash(source_code);

    if (source_code.empty() || source_code.find("main") == std::string_view::npos) {
        mod.valid = false;
        mod.diagnostic = "Missing shader entry point";
        cache_.emplace(key, mod);
        return mod;
    }

    // Parse simple GLSL/HLSL binding declarations for descriptor layout reflection
    size_t pos = 0;
    while ((pos = source_code.find("binding", pos)) != std::string_view::npos) {
        const size_t eq = source_code.find('=', pos);
        if (eq != std::string_view::npos && eq < pos + 16) {
            uint32_t b = 0;
            size_t d = eq + 1;
            while (d < source_code.size() && (source_code[d] == ' ' || source_code[d] == '\t')) ++d;
            while (d < source_code.size() && source_code[d] >= '0' && source_code[d] <= '9') {
                b = b * 10 + static_cast<uint32_t>(source_code[d] - '0');
                ++d;
            }
            const bool is_ssbo = source_code.substr(pos, std::min<size_t>(48, source_code.size() - pos)).find("buffer") != std::string_view::npos;
            mod.bindings.push_back({b, 0, "binding_" + std::to_string(b), is_ssbo});
        }
        pos += 7;
    }

    // Emit canonical SPIR-V magic header + word stream
    mod.spirv_words.push_back(0x07230203U);
    mod.spirv_words.push_back(0x00010500U);
    mod.spirv_words.push_back(static_cast<uint32_t>(mod.source_hash & 0xFFFFFFFFU));
    mod.valid = true;

    cache_.emplace(key, mod);
    return mod;
}

bool RhiRendererEngine::initialize(const RhiSwapchainConfig& config) {
    config_ = config;
    framebuffer_.resize(config.width, config.height);
    framebuffer_.clear();
    pso_table_.clear();
    sampler_cache_.clear();
    uniform_pool_.clear();
    pass_graph_.clear();

    const ShaderTargetApi api = (config.backend == RhiBackendApi::Direct3D12)
        ? ShaderTargetApi::Dxil_D3D12
        : (config.backend == RhiBackendApi::Metal ? ShaderTargetApi::Msl_Metal : ShaderTargetApi::SpirV_Vulkan);
    (void)compiler_.compile(ShaderCompiler::builtin_wboit_accum_frag(), ShaderStage::Fragment, api);
    (void)compiler_.compile(ShaderCompiler::builtin_wboit_composite_frag(), ShaderStage::Fragment, api);

    bound_pso_ = create_pso(PipelineStateDesc{"opaque_pbr", true, true, RhiCompareOp::Less, RhiBlendMode::Opaque, RhiCullMode::Back});
    pass_graph_.add_pass({"DepthPrePass", {"depth_prepass", true, true, RhiCompareOp::Less, RhiBlendMode::Opaque, RhiCullMode::Back}, false, true});
    pass_graph_.add_pass({"MainOpaquePass", pso_table_[0], true, false});

    initialized_ = true;
    return true;
}

void RhiRendererEngine::shutdown() {
    initialized_ = false;
    uniform_pool_.clear();
}

uint32_t RhiRendererEngine::create_pso(const PipelineStateDesc& desc) {
    pso_table_.push_back(desc);
    return static_cast<uint32_t>(pso_table_.size() - 1);
}

uint32_t RhiRendererEngine::get_or_create_sampler(const RhiSamplerDesc& desc) {
    const std::string key = desc.name + (desc.linear_filter ? "_L" : "_N") + (desc.clamp_to_edge ? "_C" : "_R");
    auto it = sampler_cache_.find(key);
    if (it != sampler_cache_.end()) return it->second;
    const uint32_t id = static_cast<uint32_t>(sampler_cache_.size());
    sampler_cache_.emplace(key, id);
    return id;
}

void RhiRendererEngine::begin_frame(const std::array<double, 4>& clear_rgba) {
    metrics_ = RhiFrameMetrics{};
    framebuffer_.clear(clear_rgba, 1.0);
}

void RhiRendererEngine::bind_pso(uint32_t pso_id) {
    if (pso_id < pso_table_.size()) {
        bound_pso_ = pso_id;
        ++metrics_.pso_binds;
    }
}

void RhiRendererEngine::render_scene(
    const core::domain::ViewportCamera& camera,
    const std::vector<RhiDrawItem>& draw_items,
    const Vec3d& light_dir
) {
    if (!initialized_) return;
    const auto vp = camera.view_projection_matrix();
    const uint32_t w = framebuffer_.width;
    const uint32_t h = framebuffer_.height;
    const Vec3d ldir = light_dir.normalized();
    const PipelineStateDesc pso = bound_pso_ < pso_table_.size() ? pso_table_[bound_pso_] : PipelineStateDesc{};

    for (const auto& item : draw_items) {
        if (!item.mesh || item.mesh->triangles.empty()) continue;
        ++metrics_.draw_calls;
        const auto mvp = vp * item.model_matrix;

        for (size_t ti = 0; ti < item.mesh->triangles.size(); ++ti) {
            const auto& tri = item.mesh->triangles[ti];
            ++metrics_.triangles_submitted;

            const Point3D wp0 = transform_pos(item.model_matrix, item.mesh->vertices[tri[0]]);
            const Point3D wp1 = transform_pos(item.model_matrix, item.mesh->vertices[tri[1]]);
            const Point3D wp2 = transform_pos(item.model_matrix, item.mesh->vertices[tri[2]]);
            const Vec3d world_n = transform_dir(item.model_matrix, (wp1 - wp0).cross(wp2 - wp0));
            const Vec3d raw_n = (wp1 - wp0).cross(wp2 - wp0).normalized();

            const ClipVert c0 = transform_point(mvp, item.mesh->vertices[tri[0]]);
            const ClipVert c1 = transform_point(mvp, item.mesh->vertices[tri[1]]);
            const ClipVert c2 = transform_point(mvp, item.mesh->vertices[tri[2]]);
            if (c0.w <= 1e-5 || c1.w <= 1e-5 || c2.w <= 1e-5) continue;

            // NDC [-1, 1] -> Screen [0, w] x [0, h]
            const auto to_screen = [&](const ClipVert& c) -> std::array<double, 3> {
                const double nx = c.x / c.w;
                const double ny = c.y / c.w;
                const double nz = c.z / c.w;
                return {
                    (nx * 0.5 + 0.5) * static_cast<double>(w),
                    (0.5 - ny * 0.5) * static_cast<double>(h),
                    nz * 0.5 + 0.5
                };
            };
            auto s0 = to_screen(c0);
            auto s1 = to_screen(c1);
            auto s2 = to_screen(c2);

            double signed_area2 = (s1[0] - s0[0]) * (s2[1] - s0[1]) - (s1[1] - s0[1]) * (s2[0] - s0[0]);
            if (pso.cull_mode == RhiCullMode::Back && signed_area2 >= 0.0) {
                continue;
            }
            if (pso.cull_mode == RhiCullMode::Front && signed_area2 <= 0.0) {
                continue;
            }
            if (signed_area2 > 0.0) {
                std::swap(s1, s2);
                signed_area2 = -signed_area2;
            }
            if (std::abs(signed_area2) <= 1e-9) continue;

            const int min_x = std::max(0, static_cast<int>(std::floor(std::min({s0[0], s1[0], s2[0]}))));
            const int max_x = std::min(static_cast<int>(w) - 1, static_cast<int>(std::ceil(std::max({s0[0], s1[0], s2[0]}))));
            const int min_y = std::max(0, static_cast<int>(std::floor(std::min({s0[1], s1[1], s2[1]}))));
            const int max_y = std::min(static_cast<int>(h) - 1, static_cast<int>(std::ceil(std::max({s0[1], s1[1], s2[1]}))));

            const double n_dot_l = std::max(0.0, raw_n.dot(ldir));
            const double shade = 0.2 + 0.8 * n_dot_l;
            (void)world_n;

            for (int py = min_y; py <= max_y; ++py) {
                for (int px = min_x; px <= max_x; ++px) {
                    const double x = static_cast<double>(px) + 0.5;
                    const double y = static_cast<double>(py) + 0.5;
                    const double w0 = ((s1[0] - s2[0]) * (y - s2[1]) - (s1[1] - s2[1]) * (x - s2[0])) / (-signed_area2);
                    const double w1 = ((s2[0] - s0[0]) * (y - s0[1]) - (s2[1] - s0[1]) * (x - s0[0])) / (-signed_area2);
                    const double w2 = 1.0 - w0 - w1;
                    if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6) continue;

                    const double z = w0 * s0[2] + w1 * s1[2] + w2 * s2[2];
                    if (z < 0.0 || z > 1.0) continue;

                    const size_t idx = static_cast<size_t>(py) * w + static_cast<size_t>(px);
                    if (pso.depth_test && z >= framebuffer_.depth[idx]) continue;
                    if (pso.depth_write) framebuffer_.depth[idx] = z;

                    framebuffer_.color_rgba[idx] = {
                        std::clamp(item.base_color[0] * shade, 0.0, 1.0),
                        std::clamp(item.base_color[1] * shade, 0.0, 1.0),
                        std::clamp(item.base_color[2] * shade, 0.0, 1.0),
                        item.base_color[3]
                    };
                    ++metrics_.pixels_shaded;
                }
            }
        }
    }
}

void RhiRendererEngine::end_frame() {
    // Frame sync barrier / swapchain present point
}

} // namespace shunya::cad3d
