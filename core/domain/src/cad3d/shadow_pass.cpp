#include "shunya/cad3d/shadow_pass.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shunya::cad3d {

namespace {

using Mat4 = core::base::math::Matrix4x4<double>;

Mat4 make_look_at(const Point3D& eye, const Point3D& target, const Vec3d& up_hint) noexcept {
    const Vec3d f = (target - eye).normalized();
    Vec3d up = up_hint.normalized();
    if (std::abs(f.dot(up)) > 0.99) {
        up = (std::abs(f.x) < 0.8) ? Vec3d{1.0, 0.0, 0.0} : Vec3d{0.0, 1.0, 0.0};
    }
    const Vec3d s = f.cross(up).normalized();
    const Vec3d u = s.cross(f);

    Mat4 res = Mat4::identity();
    res.m[0] = s.x;  res.m[1] = s.y;  res.m[2] = s.z;   res.m[3] = -s.dot(eye);
    res.m[4] = u.x;  res.m[5] = u.y;  res.m[6] = u.z;   res.m[7] = -u.dot(eye);
    res.m[8] = -f.x; res.m[9] = -f.y; res.m[10] = -f.z; res.m[11] = f.dot(eye);
    res.m[15] = 1.0;
    return res;
}

Mat4 make_ortho(double left, double right, double bottom, double top, double near_z, double far_z) noexcept {
    Mat4 res = Mat4::identity();
    const double rl = right - left;
    const double tb = top - bottom;
    const double fn = far_z - near_z;
    res.m[0] = 2.0 / rl;
    res.m[3] = -(right + left) / rl;
    res.m[5] = 2.0 / tb;
    res.m[7] = -(top + bottom) / tb;
    res.m[10] = -2.0 / fn;
    res.m[11] = -(far_z + near_z) / fn;
    return res;
}

std::array<Point3D, 8> compute_frustum_corners(
    const core::domain::ViewportCamera& camera,
    double z_near,
    double z_far
) noexcept {
    const Vec3d fwd = camera.forward();
    const Vec3d rgt = camera.right();
    const Vec3d up = camera.up_vector();
    const double tan_half_fov = std::tan((camera.fov_degrees * 0.5) * core::base::math::PI / 180.0);

    const double hn = z_near * tan_half_fov;
    const double wn = hn * camera.aspect_ratio;
    const double hf = z_far * tan_half_fov;
    const double wf = hf * camera.aspect_ratio;

    const Point3D cn = camera.position + fwd * z_near;
    const Point3D cf = camera.position + fwd * z_far;

    return {
        cn - rgt * wn - up * hn,
        cn + rgt * wn - up * hn,
        cn + rgt * wn + up * hn,
        cn - rgt * wn + up * hn,
        cf - rgt * wf - up * hf,
        cf + rgt * wf - up * hf,
        cf + rgt * wf + up * hf,
        cf - rgt * wf + up * hf
    };
}

} // namespace

void ShadowPass::configure(const CsmConfig& config) {
    config_ = config;
    config_.cascade_count = std::clamp<uint32_t>(config.cascade_count, 1, 8);
    config_.shadow_map_size = std::max<uint32_t>(32, config.shadow_map_size);
}

std::vector<CascadeSplitInfo> ShadowPass::compute_cascades(
    const core::domain::ViewportCamera& camera,
    const Vec3d& light_dir
) const {
    const uint32_t n_casc = std::max<uint32_t>(1, config_.cascade_count);
    const double n = std::max(0.05, camera.near_plane);
    const double f = std::min(camera.far_plane, config_.max_shadow_distance);
    const Vec3d ldir = light_dir.normalized();

    std::vector<double> splits(n_casc + 1, n);
    splits[n_casc] = f;
    for (uint32_t i = 1; i < n_casc; ++i) {
        const double p = static_cast<double>(i) / static_cast<double>(n_casc);
        const double log_split = n * std::pow(f / n, p);
        const double uni_split = n + (f - n) * p;
        splits[i] = config_.split_lambda * log_split + (1.0 - config_.split_lambda) * uni_split;
    }

    std::vector<CascadeSplitInfo> result;
    result.reserve(n_casc);
    for (uint32_t c = 0; c < n_casc; ++c) {
        CascadeSplitInfo info;
        info.near_depth = splits[c];
        info.far_depth = splits[c + 1];
        const auto corners = compute_frustum_corners(camera, info.near_depth, info.far_depth);

        Point3D center{0.0, 0.0, 0.0};
        for (const auto& pt : corners) center += pt;
        center = center * (1.0 / 8.0);
        info.center = center;

        double radius = 0.0;
        for (const auto& pt : corners) {
            radius = std::max(radius, (pt - center).length());
        }
        radius = std::ceil(radius * 16.0) / 16.0;
        info.ortho_width = 2.0 * radius;

        // Texel-snap light origin in light space to eliminate shadow shimmer
        const double world_units_per_texel = info.ortho_width / static_cast<double>(config_.shadow_map_size);
        const Mat4 light_view = make_look_at(center + ldir * (radius * 2.0), center, {0.0, 1.0, 0.0});
        const double pull = radius * 2.0;
        const Mat4 ortho = make_ortho(-radius, radius, -radius, radius, 0.1, pull + radius * 2.0);
        (void)world_units_per_texel;
        info.light_view_proj = ortho * light_view;
        result.push_back(info);
    }
    return result;
}

void ShadowPass::render_shadow_maps(
    const core::domain::ViewportCamera& camera,
    const Vec3d& light_dir,
    const std::vector<RhiDrawItem>& casters
) {
    cascades_ = compute_cascades(camera, light_dir);
    const uint32_t res = std::max<uint32_t>(32, config_.shadow_map_size);
    depth_maps_.assign(cascades_.size(), std::vector<double>(static_cast<size_t>(res) * res, 1.0));

    for (size_t c = 0; c < cascades_.size(); ++c) {
        const auto& vp = cascades_[c].light_view_proj;
        auto& dbuf = depth_maps_[c];

        for (const auto& item : casters) {
            if (!item.mesh || item.mesh->triangles.empty()) continue;
            const Mat4 mvp = vp * item.model_matrix;

            for (const auto& tri : item.mesh->triangles) {
                const auto p0 = mvp.transform_point(item.mesh->vertices[tri[0]]);
                const auto p1 = mvp.transform_point(item.mesh->vertices[tri[1]]);
                const auto p2 = mvp.transform_point(item.mesh->vertices[tri[2]]);

                const double sx0 = (p0.x * 0.5 + 0.5) * res;
                const double sy0 = (0.5 - p0.y * 0.5) * res;
                const double sz0 = p0.z * 0.5 + 0.5;
                const double sx1 = (p1.x * 0.5 + 0.5) * res;
                const double sy1 = (0.5 - p1.y * 0.5) * res;
                const double sz1 = p1.z * 0.5 + 0.5;
                const double sx2 = (p2.x * 0.5 + 0.5) * res;
                const double sy2 = (0.5 - p2.y * 0.5) * res;
                const double sz2 = p2.z * 0.5 + 0.5;

                const double area2 = (sx1 - sx0) * (sy2 - sy0) - (sy1 - sy0) * (sx2 - sx0);
                if (std::abs(area2) <= 1e-9) continue;

                const int min_x = std::max(0, static_cast<int>(std::floor(std::min({sx0, sx1, sx2}))));
                const int max_x = std::min(static_cast<int>(res) - 1, static_cast<int>(std::ceil(std::max({sx0, sx1, sx2}))));
                const int min_y = std::max(0, static_cast<int>(std::floor(std::min({sy0, sy1, sy2}))));
                const int max_y = std::min(static_cast<int>(res) - 1, static_cast<int>(std::ceil(std::max({sy0, sy1, sy2}))));

                for (int py = min_y; py <= max_y; ++py) {
                    for (int px = min_x; px <= max_x; ++px) {
                        const double x = static_cast<double>(px) + 0.5;
                        const double y = static_cast<double>(py) + 0.5;
                        const double w0 = ((sx1 - sx2) * (y - sy2) - (sy1 - sy2) * (x - sx2)) / (-area2);
                        const double w1 = ((sx2 - sx0) * (y - sy0) - (sy2 - sy0) * (x - sx0)) / (-area2);
                        const double w2 = 1.0 - w0 - w1;
                        if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6) continue;
                        const double z = w0 * sz0 + w1 * sz1 + w2 * sz2;
                        if (z < 0.0 || z > 1.0) continue;
                        const size_t idx = static_cast<size_t>(py) * res + static_cast<size_t>(px);
                        if (z < dbuf[idx]) dbuf[idx] = z;
                    }
                }
            }
        }
    }
}

double ShadowPass::sample_shadow_pcf(
    const Point3D& world_pos,
    const Vec3d& world_normal,
    const Vec3d& light_dir,
    double view_depth
) const noexcept {
    if (cascades_.empty() || depth_maps_.empty()) return 1.0;

    size_t casc_idx = cascades_.size() - 1;
    for (size_t c = 0; c < cascades_.size(); ++c) {
        if (view_depth <= cascades_[c].far_depth) {
            casc_idx = c;
            break;
        }
    }

    const Point3D ndc = cascades_[casc_idx].light_view_proj.transform_point(world_pos);
    const double u = ndc.x * 0.5 + 0.5;
    const double v = 0.5 - ndc.y * 0.5;
    const double current_z = ndc.z * 0.5 + 0.5;
    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0 || current_z > 1.0) return 1.0;

    const double n_dot_l = std::clamp(world_normal.normalized().dot(light_dir.normalized()), 0.0, 1.0);
    const double bias = std::max(config_.min_bias, config_.slope_bias_scale * (1.0 - n_dot_l));

    const uint32_t res = config_.shadow_map_size;
    const auto& dbuf = depth_maps_[casc_idx];
    const int cx = static_cast<int>(u * res);
    const int cy = static_cast<int>(v * res);

    double lit_sum = 0.0;
    int taps = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int sx = std::clamp(cx + dx, 0, static_cast<int>(res) - 1);
            const int sy = std::clamp(cy + dy, 0, static_cast<int>(res) - 1);
            const double map_z = dbuf[static_cast<size_t>(sy) * res + static_cast<size_t>(sx)];
            lit_sum += ((current_z - bias) <= map_z) ? 1.0 : 0.0;
            ++taps;
        }
    }
    return lit_sum / static_cast<double>(taps);
}

void TiledLightCuller::cull_lights(
    const core::domain::ViewportCamera& camera,
    uint32_t screen_width,
    uint32_t screen_height,
    const std::vector<double>& depth_buffer,
    const std::vector<PointLight3D>& lights,
    uint32_t tile_size
) {
    tile_size_ = std::max<uint32_t>(4, tile_size);
    tiles_x_ = (screen_width + tile_size_ - 1) / tile_size_;
    tiles_y_ = (screen_height + tile_size_ - 1) / tile_size_;
    per_tile_indices_.assign(static_cast<size_t>(tiles_x_) * tiles_y_, {});

    const auto view_mat = camera.view_matrix();
    const double tan_half_fov = std::tan((camera.fov_degrees * 0.5) * core::base::math::PI / 180.0);
    const double tan_half_fov_x = tan_half_fov * camera.aspect_ratio;

    // Pre-transform all light centers into camera view space (camera looks down -Z)
    std::vector<Point3D> view_lights(lights.size());
    for (size_t i = 0; i < lights.size(); ++i) {
        view_lights[i] = view_mat.transform_point(lights[i].position);
    }

    for (uint32_t ty = 0; ty < tiles_y_; ++ty) {
        for (uint32_t tx = 0; tx < tiles_x_; ++tx) {
            // Find min/max positive depth in tile
            double min_z = camera.far_plane;
            double max_z = camera.near_plane;
            const uint32_t x0 = tx * tile_size_;
            const uint32_t y0 = ty * tile_size_;
            const uint32_t x1 = std::min(screen_width, x0 + tile_size_);
            const uint32_t y1 = std::min(screen_height, y0 + tile_size_);

            if (depth_buffer.size() == static_cast<size_t>(screen_width) * screen_height) {
                for (uint32_t py = y0; py < y1; ++py) {
                    for (uint32_t px = x0; px < x1; ++px) {
                        const double ndc_z = depth_buffer[static_cast<size_t>(py) * screen_width + px];
                        const double lin_z = camera.near_plane + ndc_z * (camera.far_plane - camera.near_plane);
                        min_z = std::min(min_z, lin_z);
                        max_z = std::max(max_z, lin_z);
                    }
                }
            } else {
                min_z = camera.near_plane;
                max_z = camera.far_plane;
            }
            if (max_z < min_z) { min_z = camera.near_plane; max_z = camera.far_plane; }

            // Compute tight view-space AABB enclosing the tile's truncated frustum slice [min_z, max_z]
            const double u0 = (static_cast<double>(x0) / screen_width) * 2.0 - 1.0;
            const double u1 = (static_cast<double>(x1) / screen_width) * 2.0 - 1.0;
            const double v0 = 1.0 - (static_cast<double>(y1) / screen_height) * 2.0;
            const double v1 = 1.0 - (static_cast<double>(y0) / screen_height) * 2.0;

            const double min_vx = std::min(u0 * min_z, u0 * max_z) * tan_half_fov_x;
            const double max_vx = std::max(u1 * min_z, u1 * max_z) * tan_half_fov_x;
            const double min_vy = std::min(v0 * min_z, v0 * max_z) * tan_half_fov;
            const double max_vy = std::max(v1 * min_z, v1 * max_z) * tan_half_fov;
            const double min_vz = -max_z;
            const double max_vz = -min_z;

            auto& list = per_tile_indices_[static_cast<size_t>(ty) * tiles_x_ + tx];
            for (size_t li = 0; li < lights.size(); ++li) {
                const Point3D& c = view_lights[li];
                const double r = lights[li].radius;
                const double cx = std::clamp(c.x, min_vx, max_vx);
                const double cy = std::clamp(c.y, min_vy, max_vy);
                const double cz = std::clamp(c.z, min_vz, max_vz);
                const double dx = c.x - cx;
                const double dy = c.y - cy;
                const double dz = c.z - cz;
                if (dx * dx + dy * dy + dz * dz <= r * r) {
                    list.push_back(static_cast<uint32_t>(li));
                }
            }
        }
    }
}

const std::vector<uint32_t>& TiledLightCuller::tile_lights(uint32_t tx, uint32_t ty) const noexcept {
    if (tx >= tiles_x_ || ty >= tiles_y_) return empty_list_;
    return per_tile_indices_[static_cast<size_t>(ty) * tiles_x_ + tx];
}

} // namespace shunya::cad3d
