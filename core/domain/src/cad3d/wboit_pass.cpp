#include "shunya/cad3d/wboit_pass.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad3d {

namespace {

struct ClipV {
    double x, y, z, w;
};

ClipV xform_clip(const core::base::math::Matrix4x4<double>& mat, const Point3D& p) noexcept {
    const auto& m = mat.m;
    return {
        m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3],
        m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
        m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11],
        m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15]
    };
}

double wboit_weight(double z_ndc, double alpha) noexcept {
    // McGuire-Bavoil 2013 depth-ordered weighting function
    const double a_term = std::min(1.0, alpha * 10.0) + 0.01;
    const double d_term = 1.0 - std::clamp(z_ndc, 0.0, 1.0) * 0.9;
    return std::clamp(a_term * a_term * a_term * 1e4 * d_term * d_term * d_term, 1e-2, 3e3);
}

} // namespace

void WboitPass::resize(uint32_t width, uint32_t height) {
    width_ = std::max<uint32_t>(1, width);
    height_ = std::max<uint32_t>(1, height);
    clear();
}

void WboitPass::clear() {
    const size_t n = static_cast<size_t>(width_) * static_cast<size_t>(height_);
    accum_.assign(n, {0.0, 0.0, 0.0, 0.0});
    revealage_.assign(n, 1.0);
}

void WboitPass::accumulate(
    const core::domain::ViewportCamera& camera,
    const std::vector<RhiDrawItem>& transparent_items,
    const std::vector<double>& opaque_depth
) {
    if (width_ == 0 || height_ == 0) return;
    const auto vp = camera.view_projection_matrix();
    const bool has_depth = opaque_depth.size() == static_cast<size_t>(width_) * height_;

    for (const auto& item : transparent_items) {
        if (!item.mesh || item.mesh->triangles.empty()) continue;
        const double alpha = std::clamp(item.base_color[3], 0.0, 1.0);
        if (alpha <= 1e-6) continue;

        const auto mvp = vp * item.model_matrix;
        for (const auto& tri : item.mesh->triangles) {
            const ClipV c0 = xform_clip(mvp, item.mesh->vertices[tri[0]]);
            const ClipV c1 = xform_clip(mvp, item.mesh->vertices[tri[1]]);
            const ClipV c2 = xform_clip(mvp, item.mesh->vertices[tri[2]]);
            if (c0.w <= 1e-5 || c1.w <= 1e-5 || c2.w <= 1e-5) continue;

            const auto to_s = [&](const ClipV& c) -> std::array<double, 3> {
                return {
                    ((c.x / c.w) * 0.5 + 0.5) * width_,
                    (0.5 - (c.y / c.w) * 0.5) * height_,
                    (c.z / c.w) * 0.5 + 0.5
                };
            };
            const auto s0 = to_s(c0);
            const auto s1 = to_s(c1);
            const auto s2 = to_s(c2);

            const double signed_area2 = (s1[0] - s0[0]) * (s2[1] - s0[1]) - (s1[1] - s0[1]) * (s2[0] - s0[0]);
            if (signed_area2 >= -1e-9) continue; // front-face only

            const int min_x = std::max(0, static_cast<int>(std::floor(std::min({s0[0], s1[0], s2[0]}))));
            const int max_x = std::min(static_cast<int>(width_) - 1, static_cast<int>(std::ceil(std::max({s0[0], s1[0], s2[0]}))));
            const int min_y = std::max(0, static_cast<int>(std::floor(std::min({s0[1], s1[1], s2[1]}))));
            const int max_y = std::min(static_cast<int>(height_) - 1, static_cast<int>(std::ceil(std::max({s0[1], s1[1], s2[1]}))));

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

                    const size_t idx = static_cast<size_t>(py) * width_ + static_cast<size_t>(px);
                    if (has_depth && z >= opaque_depth[idx]) continue;

                    const double wt = wboit_weight(z, alpha);
                    accum_[idx][0] += item.base_color[0] * alpha * wt;
                    accum_[idx][1] += item.base_color[1] * alpha * wt;
                    accum_[idx][2] += item.base_color[2] * alpha * wt;
                    accum_[idx][3] += alpha * wt;
                    revealage_[idx] *= (1.0 - alpha);
                }
            }
        }
    }
}

void WboitPass::composite(RhiRenderTarget& target_opaque_rt) const {
    if (target_opaque_rt.width != width_ || target_opaque_rt.height != height_) return;
    const size_t n = static_cast<size_t>(width_) * height_;
    for (size_t i = 0; i < n; ++i) {
        const double reveal = std::clamp(revealage_[i], 0.0, 1.0);
        if (reveal >= 1.0 - 1e-9 || accum_[i][3] <= 1e-12) continue;

        const double inv_a = 1.0 / std::max(accum_[i][3], 1e-5);
        const std::array<double, 3> avg_col{
            accum_[i][0] * inv_a,
            accum_[i][1] * inv_a,
            accum_[i][2] * inv_a
        };
        auto& dst = target_opaque_rt.color_rgba[i];
        dst[0] = std::clamp(avg_col[0] * (1.0 - reveal) + dst[0] * reveal, 0.0, 1.0);
        dst[1] = std::clamp(avg_col[1] * (1.0 - reveal) + dst[1] * reveal, 0.0, 1.0);
        dst[2] = std::clamp(avg_col[2] * (1.0 - reveal) + dst[2] * reveal, 0.0, 1.0);
        dst[3] = 1.0;
    }
}

} // namespace shunya::cad3d
