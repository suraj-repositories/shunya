#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/rendering.hpp"

#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <memory>
#include <cmath>

namespace shunya::core::domain {

struct RasterVertex {
    base::math::Vector3<double> position{0.0, 0.0, 0.0};
    base::math::Vector3<double> normal{0.0, 1.0, 0.0};
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    base::math::Vector2<double> uv{0.0, 0.0};
};

struct RasterTriangle {
    RasterVertex v0;
    RasterVertex v1;
    RasterVertex v2;
    uint32_t object_id{0};
};

enum class CullMode {
    None,
    Back,
    Front
};

enum class DepthFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum class ShadingModel {
    Flat,
    Lambertian,
    DepthOnly,
    ObjectIdPicking
};

struct RasterizerConfig {
    uint32_t width{320};
    uint32_t height{240};
    bool enable_depth_test{true};
    bool enable_depth_write{true};
    DepthFunc depth_func{DepthFunc::Less};
    CullMode cull_mode{CullMode::Back};
    ShadingModel shading_model{ShadingModel::Lambertian};
    std::array<float, 4> clear_color{0.08f, 0.10f, 0.14f, 1.0f};
    float clear_depth{1.0f};
};

struct RasterizerStats {
    uint32_t triangles_input{0};
    uint32_t triangles_culled{0};
    uint32_t triangles_clipped{0};
    uint32_t fragments_tested{0};
    uint32_t fragments_depth_rejected{0};
    uint32_t fragments_shaded{0};

    void reset() noexcept {
        triangles_input = 0;
        triangles_culled = 0;
        triangles_clipped = 0;
        fragments_tested = 0;
        fragments_depth_rejected = 0;
        fragments_shaded = 0;
    }
};

class MockHardwareRasterizer {
public:
    explicit MockHardwareRasterizer(RasterizerConfig config = {});

    void resize(uint32_t width, uint32_t height);
    [[nodiscard]] uint32_t width() const noexcept { return config_.width; }
    [[nodiscard]] uint32_t height() const noexcept { return config_.height; }
    [[nodiscard]] const RasterizerConfig& config() const noexcept { return config_; }
    void set_config(const RasterizerConfig& config) { config_ = config; }

    void clear();
    void clear(const std::array<float, 4>& color, float depth = 1.0f);

    void set_matrices(const base::math::Matrix4x4<double>& model,
                      const base::math::Matrix4x4<double>& view,
                      const base::math::Matrix4x4<double>& projection);

    void set_light_dir(const base::math::Vector3<double>& dir);

    void draw_triangle(const RasterTriangle& tri);
    void draw_triangles(const std::vector<RasterTriangle>& tris);

    [[nodiscard]] uint32_t read_pixel_rgba(uint32_t x, uint32_t y) const noexcept;
    [[nodiscard]] float read_depth(uint32_t x, uint32_t y) const noexcept;
    [[nodiscard]] uint32_t read_object_id(uint32_t x, uint32_t y) const noexcept;

    [[nodiscard]] const std::vector<uint32_t>& color_buffer() const noexcept { return color_buffer_; }
    [[nodiscard]] const std::vector<float>& depth_buffer() const noexcept { return depth_buffer_; }
    [[nodiscard]] const std::vector<uint32_t>& id_buffer() const noexcept { return id_buffer_; }

    [[nodiscard]] uint64_t compute_checksum() const noexcept;

    [[nodiscard]] const RasterizerStats& stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_.reset(); }

private:
    RasterizerConfig config_;
    std::vector<uint32_t> color_buffer_; // RGBA8 packed
    std::vector<float> depth_buffer_;    // Depth [0.0, 1.0]
    std::vector<uint32_t> id_buffer_;    // Object picking ID
    base::math::Matrix4x4<double> model_{base::math::Matrix4x4<double>::identity()};
    base::math::Matrix4x4<double> view_{base::math::Matrix4x4<double>::identity()};
    base::math::Matrix4x4<double> proj_{base::math::Matrix4x4<double>::identity()};
    base::math::Matrix4x4<double> mvp_{base::math::Matrix4x4<double>::identity()};
    base::math::Vector3<double> light_dir_{0.57735, 0.57735, 0.57735};
    RasterizerStats stats_{};

    bool pass_depth_test(float incoming, float existing) const noexcept;
};

// ============================================================================
// Viewport Controller Offscreen Test Harness
// ============================================================================

class OffscreenViewportTestHarness {
public:
    explicit OffscreenViewportTestHarness(uint32_t width = 320, uint32_t height = 240);

    void set_dimensions(uint32_t width, uint32_t height);
    [[nodiscard]] uint32_t width() const noexcept { return rasterizer_.width(); }
    [[nodiscard]] uint32_t height() const noexcept { return rasterizer_.height(); }

    void orbit(double delta_azimuth_deg, double delta_elevation_deg);
    void pan(double delta_x, double delta_y);
    void dolly(double delta_z);
    void look_at(const base::math::Vector3<double>& eye,
                 const base::math::Vector3<double>& target,
                 const base::math::Vector3<double>& up = {0.0, 1.0, 0.0});

    [[nodiscard]] const ViewportCamera& camera() const noexcept { return camera_; }
    ViewportCamera& camera() noexcept { return camera_; }

    [[nodiscard]] Ray ray_from_screen(double screen_x, double screen_y) const;

    void render_cube(const base::math::Vector3<double>& center, double size, const std::array<float, 4>& color, uint32_t obj_id = 1);

    [[nodiscard]] uint32_t pick_object_at(double screen_x, double screen_y) const;
    [[nodiscard]] uint32_t sample_pixel(uint32_t x, uint32_t y) const;
    [[nodiscard]] float sample_depth(uint32_t x, uint32_t y) const;

    [[nodiscard]] MockHardwareRasterizer& rasterizer() noexcept { return rasterizer_; }
    [[nodiscard]] const MockHardwareRasterizer& rasterizer() const noexcept { return rasterizer_; }

private:
    ViewportCamera camera_;
    MockHardwareRasterizer rasterizer_;
    double azimuth_deg_{45.0};
    double elevation_deg_{30.0};
    double distance_{10.0};
    base::math::Vector3<double> target_{0.0, 0.0, 0.0};

    void update_camera_transform();
};

} // namespace shunya::core::domain
