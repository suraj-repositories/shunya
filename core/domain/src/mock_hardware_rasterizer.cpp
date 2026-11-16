#include "shunya/core/domain/mock_hardware_rasterizer.hpp"
#include <algorithm>
#include <cmath>

namespace shunya::core::domain {

namespace {

inline uint32_t pack_rgba(float r, float g, float b, float a) {
    auto cr = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
    auto cg = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
    auto cb = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
    auto ca = static_cast<uint8_t>(std::clamp(a * 255.0f, 0.0f, 255.0f));
    return (static_cast<uint32_t>(ca) << 24) |
           (static_cast<uint32_t>(cb) << 16) |
           (static_cast<uint32_t>(cg) << 8)  |
           (static_cast<uint32_t>(cr));
}

inline double edge_function(const base::math::Vector2<double>& a,
                            const base::math::Vector2<double>& b,
                            const base::math::Vector2<double>& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

} // namespace

MockHardwareRasterizer::MockHardwareRasterizer(RasterizerConfig config)
    : config_(config) {
    resize(config_.width, config_.height);
}

void MockHardwareRasterizer::resize(uint32_t width, uint32_t height) {
    config_.width = std::max(1u, width);
    config_.height = std::max(1u, height);
    const size_t total_pixels = static_cast<size_t>(config_.width) * config_.height;
    color_buffer_.resize(total_pixels, 0);
    depth_buffer_.resize(total_pixels, 1.0f);
    id_buffer_.resize(total_pixels, 0);
    clear();
}

void MockHardwareRasterizer::clear() {
    clear(config_.clear_color, config_.clear_depth);
}

void MockHardwareRasterizer::clear(const std::array<float, 4>& color, float depth) {
    const uint32_t packed = pack_rgba(color[0], color[1], color[2], color[3]);
    std::fill(color_buffer_.begin(), color_buffer_.end(), packed);
    std::fill(depth_buffer_.begin(), depth_buffer_.end(), depth);
    std::fill(id_buffer_.begin(), id_buffer_.end(), 0u);
}

void MockHardwareRasterizer::set_matrices(const base::math::Matrix4x4<double>& model,
                                          const base::math::Matrix4x4<double>& view,
                                          const base::math::Matrix4x4<double>& projection) {
    model_ = model;
    view_ = view;
    proj_ = projection;
    mvp_ = proj_ * view_ * model_;
}

void MockHardwareRasterizer::set_light_dir(const base::math::Vector3<double>& dir) {
    double len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-9) {
        light_dir_ = {dir.x / len, dir.y / len, dir.z / len};
    }
}

bool MockHardwareRasterizer::pass_depth_test(float incoming, float existing) const noexcept {
    switch (config_.depth_func) {
        case DepthFunc::Never: return false;
        case DepthFunc::Less: return incoming < existing;
        case DepthFunc::Equal: return std::abs(incoming - existing) < 1e-6f;
        case DepthFunc::LessEqual: return incoming <= existing;
        case DepthFunc::Greater: return incoming > existing;
        case DepthFunc::NotEqual: return std::abs(incoming - existing) >= 1e-6f;
        case DepthFunc::GreaterEqual: return incoming >= existing;
        case DepthFunc::Always: return true;
    }
    return false;
}

void MockHardwareRasterizer::draw_triangle(const RasterTriangle& tri) {
    ++stats_.triangles_input;

    // Transform vertices through MVP
    auto transform_v = [&](const RasterVertex& rv, base::math::Vector4<double>& out_clip, base::math::Vector3<double>& out_norm) {
        base::math::Vector4<double> pos4{rv.position.x, rv.position.y, rv.position.z, 1.0};
        out_clip = mvp_ * pos4;
        // Transform normal by model matrix (assume uniform scale for mock)
        base::math::Vector4<double> n4{rv.normal.x, rv.normal.y, rv.normal.z, 0.0};
        auto tn = model_ * n4;
        double len = std::sqrt(tn.x * tn.x + tn.y * tn.y + tn.z * tn.z);
        if (len > 1e-9) {
            out_norm = {tn.x / len, tn.y / len, tn.z / len};
        } else {
            out_norm = {0.0, 1.0, 0.0};
        }
    };

    base::math::Vector4<double> c0, c1, c2;
    base::math::Vector3<double> n0, n1, n2;
    transform_v(tri.v0, c0, n0);
    transform_v(tri.v1, c1, n1);
    transform_v(tri.v2, c2, n2);

    // Near-plane guard: clip if all behind eye
    if (c0.w <= 0.01 && c1.w <= 0.01 && c2.w <= 0.01) {
        ++stats_.triangles_clipped;
        return;
    }

    // Perspective divide to NDC [-1, 1]
    const double inv_w0 = (c0.w != 0.0) ? (1.0 / c0.w) : 1.0;
    const double inv_w1 = (c1.w != 0.0) ? (1.0 / c1.w) : 1.0;
    const double inv_w2 = (c2.w != 0.0) ? (1.0 / c2.w) : 1.0;

    base::math::Vector3<double> ndc0{c0.x * inv_w0, c0.y * inv_w0, c0.z * inv_w0};
    base::math::Vector3<double> ndc1{c1.x * inv_w1, c1.y * inv_w1, c1.z * inv_w1};
    base::math::Vector3<double> ndc2{c2.x * inv_w2, c2.y * inv_w2, c2.z * inv_w2};

    // Viewport transform to Screen Space [0, W] x [0, H]
    const double hw = static_cast<double>(config_.width) * 0.5;
    const double hh = static_cast<double>(config_.height) * 0.5;

    base::math::Vector2<double> s0{(ndc0.x + 1.0) * hw, (1.0 - ndc0.y) * hh};
    base::math::Vector2<double> s1{(ndc1.x + 1.0) * hw, (1.0 - ndc1.y) * hh};
    base::math::Vector2<double> s2{(ndc2.x + 1.0) * hw, (1.0 - ndc2.y) * hh};

    // Back-face culling
    double area = edge_function(s0, s1, s2);
    if (config_.cull_mode == CullMode::Back && area <= 0.0) {
        ++stats_.triangles_culled;
        return;
    }
    if (config_.cull_mode == CullMode::Front && area >= 0.0) {
        ++stats_.triangles_culled;
        return;
    }
    if (std::abs(area) < 1e-5) {
        ++stats_.triangles_culled;
        return; // Degenerate
    }

    const double inv_area = 1.0 / area;

    // Bounding Box
    int min_x = std::clamp(static_cast<int>(std::floor(std::min({s0.x, s1.x, s2.x}))), 0, static_cast<int>(config_.width) - 1);
    int max_x = std::clamp(static_cast<int>(std::ceil(std::max({s0.x, s1.x, s2.x}))), 0, static_cast<int>(config_.width) - 1);
    int min_y = std::clamp(static_cast<int>(std::floor(std::min({s0.y, s1.y, s2.y}))), 0, static_cast<int>(config_.height) - 1);
    int max_y = std::clamp(static_cast<int>(std::ceil(std::max({s0.y, s1.y, s2.y}))), 0, static_cast<int>(config_.height) - 1);

    // Flat face normal & diffuse calculation
    base::math::Vector3<double> face_normal = ((n0 + n1 + n2) * (1.0 / 3.0));
    double n_dot_l = face_normal.x * light_dir_.x + face_normal.y * light_dir_.y + face_normal.z * light_dir_.z;
    float diffuse = static_cast<float>(std::clamp(n_dot_l, 0.2, 1.0));

    // Barycentric rasterization loop
    for (int y = min_y; y <= max_y; ++y) {
        const double py = static_cast<double>(y) + 0.5;
        for (int x = min_x; x <= max_x; ++x) {
            const double px = static_cast<double>(x) + 0.5;
            base::math::Vector2<double> p{px, py};

            double w0 = edge_function(s1, s2, p) * inv_area;
            double w1 = edge_function(s2, s0, p) * inv_area;
            double w2 = edge_function(s0, s1, p) * inv_area;

            if (w0 >= 0.0 && w1 >= 0.0 && w2 >= 0.0) {
                ++stats_.fragments_tested;

                // Interpolate depth in NDC [0, 1] range
                double z_ndc = w0 * ndc0.z + w1 * ndc1.z + w2 * ndc2.z;
                float depth_val = static_cast<float>(std::clamp((z_ndc + 1.0) * 0.5, 0.0, 1.0));

                const size_t pixel_idx = static_cast<size_t>(y) * config_.width + static_cast<size_t>(x);
                float existing_depth = depth_buffer_[pixel_idx];

                if (config_.enable_depth_test && !pass_depth_test(depth_val, existing_depth)) {
                    ++stats_.fragments_depth_rejected;
                    continue;
                }

                if (config_.enable_depth_write) {
                    depth_buffer_[pixel_idx] = depth_val;
                }

                // Shading model calculation
                uint32_t final_color = 0;
                if (config_.shading_model == ShadingModel::Lambertian) {
                    float r = tri.v0.color[0] * diffuse;
                    float g = tri.v0.color[1] * diffuse;
                    float b = tri.v0.color[2] * diffuse;
                    final_color = pack_rgba(r, g, b, tri.v0.color[3]);
                } else if (config_.shading_model == ShadingModel::Flat) {
                    final_color = pack_rgba(tri.v0.color[0], tri.v0.color[1], tri.v0.color[2], tri.v0.color[3]);
                } else if (config_.shading_model == ShadingModel::DepthOnly) {
                    final_color = pack_rgba(depth_val, depth_val, depth_val, 1.0f);
                } else if (config_.shading_model == ShadingModel::ObjectIdPicking) {
                    uint32_t id = tri.object_id;
                    float r = static_cast<float>(id & 0xFF) / 255.0f;
                    float g = static_cast<float>((id >> 8) & 0xFF) / 255.0f;
                    float b = static_cast<float>((id >> 16) & 0xFF) / 255.0f;
                    final_color = pack_rgba(r, g, b, 1.0f);
                }

                color_buffer_[pixel_idx] = final_color;
                id_buffer_[pixel_idx] = tri.object_id;
                ++stats_.fragments_shaded;
            }
        }
    }
}

void MockHardwareRasterizer::draw_triangles(const std::vector<RasterTriangle>& tris) {
    for (const auto& tri : tris) {
        draw_triangle(tri);
    }
}

uint32_t MockHardwareRasterizer::read_pixel_rgba(uint32_t x, uint32_t y) const noexcept {
    if (x >= config_.width || y >= config_.height) return 0;
    return color_buffer_[static_cast<size_t>(y) * config_.width + x];
}

float MockHardwareRasterizer::read_depth(uint32_t x, uint32_t y) const noexcept {
    if (x >= config_.width || y >= config_.height) return 1.0f;
    return depth_buffer_[static_cast<size_t>(y) * config_.width + x];
}

uint32_t MockHardwareRasterizer::read_object_id(uint32_t x, uint32_t y) const noexcept {
    if (x >= config_.width || y >= config_.height) return 0;
    return id_buffer_[static_cast<size_t>(y) * config_.width + x];
}

uint64_t MockHardwareRasterizer::compute_checksum() const noexcept {
    // 64-bit FNV-1a hash over color buffer
    uint64_t hash = 14695981039346656037ULL;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(color_buffer_.data());
    const size_t total_bytes = color_buffer_.size() * sizeof(uint32_t);
    for (size_t i = 0; i < total_bytes; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// ============================================================================
// Viewport Controller Offscreen Test Harness Implementation
// ============================================================================

OffscreenViewportTestHarness::OffscreenViewportTestHarness(uint32_t width, uint32_t height)
    : rasterizer_({.width = width, .height = height}) {
    camera_.fov_degrees = 60.0;
    camera_.near_plane = 0.1;
    camera_.far_plane = 100.0;
    camera_.aspect_ratio = static_cast<double>(width) / static_cast<double>(height);
    update_camera_transform();
}

void OffscreenViewportTestHarness::set_dimensions(uint32_t width, uint32_t height) {
    rasterizer_.resize(width, height);
    camera_.aspect_ratio = static_cast<double>(width) / static_cast<double>(height);
}

void OffscreenViewportTestHarness::orbit(double delta_azimuth_deg, double delta_elevation_deg) {
    azimuth_deg_ += delta_azimuth_deg;
    elevation_deg_ = std::clamp(elevation_deg_ + delta_elevation_deg, -89.0, 89.0);
    update_camera_transform();
}

void OffscreenViewportTestHarness::pan(double delta_x, double delta_y) {
    base::math::Vector3<double> fwd = (target_ - camera_.position).normalized();
    base::math::Vector3<double> right = fwd.cross(camera_.up).normalized();
    base::math::Vector3<double> up = right.cross(fwd).normalized();

    base::math::Vector3<double> offset = right * delta_x + up * delta_y;
    camera_.position = camera_.position + offset;
    target_ = target_ + offset;
    update_camera_transform();
}

void OffscreenViewportTestHarness::dolly(double delta_z) {
    distance_ = std::clamp(distance_ - delta_z, 0.5, 500.0);
    update_camera_transform();
}

void OffscreenViewportTestHarness::look_at(const base::math::Vector3<double>& eye,
                                          const base::math::Vector3<double>& target,
                                          const base::math::Vector3<double>& up) {
    camera_.position = eye;
    target_ = target;
    camera_.up = up.normalized();
    base::math::Vector3<double> diff = eye - target;
    distance_ = diff.length();
    update_camera_transform();
}

void OffscreenViewportTestHarness::update_camera_transform() {
    double az_rad = azimuth_deg_ * (base::math::PI / 180.0);
    double el_rad = elevation_deg_ * (base::math::PI / 180.0);

    double x = distance_ * std::cos(el_rad) * std::sin(az_rad);
    double y = distance_ * std::sin(el_rad);
    double z = distance_ * std::cos(el_rad) * std::cos(az_rad);

    camera_.position = target_ + base::math::Vector3<double>{x, y, z};
    camera_.target = target_;
    camera_.up = {0.0, 1.0, 0.0};

    auto view = base::math::Matrix4x4<double>::look_at(camera_.position, camera_.target, camera_.up);
    auto proj = base::math::Matrix4x4<double>::perspective(
        camera_.fov_degrees * (base::math::PI / 180.0),
        camera_.aspect_ratio,
        camera_.near_plane,
        camera_.far_plane
    );

    rasterizer_.set_matrices(base::math::Matrix4x4<double>::identity(), view, proj);
}

Ray OffscreenViewportTestHarness::ray_from_screen(double screen_x, double screen_y) const {
    const double w = static_cast<double>(rasterizer_.width());
    const double h = static_cast<double>(rasterizer_.height());

    // Normalized Device Coordinates
    double ndc_x = (2.0 * screen_x) / w - 1.0;
    double ndc_y = 1.0 - (2.0 * screen_y) / h;

    auto view = base::math::Matrix4x4<double>::look_at(camera_.position, camera_.target, camera_.up);
    auto proj = base::math::Matrix4x4<double>::perspective(
        camera_.fov_degrees * (base::math::PI / 180.0),
        camera_.aspect_ratio,
        camera_.near_plane,
        camera_.far_plane
    );

    auto inv_vp = (proj * view).inverse();
    base::math::Vector4<double> near_ndc{ndc_x, ndc_y, -1.0, 1.0};
    base::math::Vector4<double> far_ndc{ndc_x, ndc_y, 1.0, 1.0};

    auto near_world = inv_vp * near_ndc;
    auto far_world = inv_vp * far_ndc;

    if (near_world.w != 0.0) near_world = near_world * (1.0 / near_world.w);
    if (far_world.w != 0.0) far_world = far_world * (1.0 / far_world.w);

    base::math::Vector3<double> origin{near_world.x, near_world.y, near_world.z};
    base::math::Vector3<double> dest{far_world.x, far_world.y, far_world.z};
    base::math::Vector3<double> dir = (dest - origin).normalized();

    return Ray{origin, dir};
}

void OffscreenViewportTestHarness::render_cube(const base::math::Vector3<double>& center,
                                              double size,
                                              const std::array<float, 4>& color,
                                              uint32_t obj_id) {
    const double h = size * 0.5;
    std::vector<base::math::Vector3<double>> corners = {
        center + base::math::Vector3<double>{-h, -h, -h}, // 0
        center + base::math::Vector3<double>{ h, -h, -h}, // 1
        center + base::math::Vector3<double>{ h,  h, -h}, // 2
        center + base::math::Vector3<double>{-h,  h, -h}, // 3
        center + base::math::Vector3<double>{-h, -h,  h}, // 4
        center + base::math::Vector3<double>{ h, -h,  h}, // 5
        center + base::math::Vector3<double>{ h,  h,  h}, // 6
        center + base::math::Vector3<double>{-h,  h,  h}  // 7
    };

    struct Face {
        int idx[4];
        base::math::Vector3<double> normal;
    };

    std::vector<Face> faces = {
        {{0, 3, 2, 1}, { 0.0,  0.0, -1.0}}, // Front
        {{4, 5, 6, 7}, { 0.0,  0.0,  1.0}}, // Back
        {{0, 1, 5, 4}, { 0.0, -1.0,  0.0}}, // Bottom
        {{2, 3, 7, 6}, { 0.0,  1.0,  0.0}}, // Top
        {{0, 4, 7, 3}, {-1.0,  0.0,  0.0}}, // Left
        {{1, 2, 6, 5}, { 1.0,  0.0,  0.0}}  // Right
    };

    std::vector<RasterTriangle> tris;
    tris.reserve(12);

    for (const auto& f : faces) {
        RasterVertex v0{.position = corners[f.idx[0]], .normal = f.normal, .color = color};
        RasterVertex v1{.position = corners[f.idx[1]], .normal = f.normal, .color = color};
        RasterVertex v2{.position = corners[f.idx[2]], .normal = f.normal, .color = color};
        RasterVertex v3{.position = corners[f.idx[3]], .normal = f.normal, .color = color};

        tris.push_back(RasterTriangle{v0, v1, v2, obj_id});
        tris.push_back(RasterTriangle{v0, v2, v3, obj_id});
    }

    rasterizer_.draw_triangles(tris);
}

uint32_t OffscreenViewportTestHarness::pick_object_at(double screen_x, double screen_y) const {
    if (screen_x < 0.0 || screen_y < 0.0) return 0;
    uint32_t x = static_cast<uint32_t>(screen_x);
    uint32_t y = static_cast<uint32_t>(screen_y);
    return rasterizer_.read_object_id(x, y);
}

uint32_t OffscreenViewportTestHarness::sample_pixel(uint32_t x, uint32_t y) const {
    return rasterizer_.read_pixel_rgba(x, y);
}

float OffscreenViewportTestHarness::sample_depth(uint32_t x, uint32_t y) const {
    return rasterizer_.read_depth(x, y);
}

} // namespace shunya::core::domain
