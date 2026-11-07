#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/data_exchange.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// REN-01: Renderer Abstraction & Backend Strategy
// =============================================================================

enum class RendererBackendType {
    Null,
    HeadlessSoftware,
    OpenGL,
    Vulkan,
    Direct3D12,
    Metal
};

[[nodiscard]] std::string_view backend_type_to_string(RendererBackendType type) noexcept;
[[nodiscard]] std::optional<RendererBackendType> backend_type_from_string(std::string_view str) noexcept;

struct RenderBackendConfig {
    RendererBackendType preferred_backend = RendererBackendType::HeadlessSoftware;
    uint32_t initial_width = 1280;
    uint32_t initial_height = 720;
    bool enable_vsync = false;
    bool enable_debug_validation = false;
    uint32_t msaa_samples = 1;
};

struct RenderCapabilities {
    std::string gpu_vendor = "Software";
    std::string renderer_name = "Shunya Headless Rasterizer";
    std::string api_version = "1.0";
    uint32_t max_texture_dimension = 4096;
    uint32_t max_render_targets = 8;
    bool supports_compute_shaders = false;
    bool supports_ray_tracing = false;
    bool supports_mesh_shaders = false;
    bool supports_multisampling = true;
    bool is_hardware_accelerated = false;
};

struct RenderFrameStats {
    uint32_t draw_calls = 0;
    uint32_t triangles_submitted = 0;
    uint32_t vertices_submitted = 0;
    uint32_t instances_culled = 0;
    uint32_t instances_rendered = 0;
    uint32_t shader_binds = 0;
    uint32_t state_changes = 0;
    double cpu_frame_time_ms = 0.0;
    double gpu_frame_time_ms = 0.0;
    size_t vertex_memory_bytes = 0;
    size_t texture_memory_bytes = 0;

    void reset() noexcept {
        draw_calls = 0;
        triangles_submitted = 0;
        vertices_submitted = 0;
        instances_culled = 0;
        instances_rendered = 0;
        shader_binds = 0;
        state_changes = 0;
        cpu_frame_time_ms = 0.0;
        gpu_frame_time_ms = 0.0;
    }
};

struct RenderPassDescriptor {
    std::string pass_name = "default_pass";
    std::array<float, 4> clear_color = {0.1f, 0.1f, 0.12f, 1.0f};
    bool clear_depth = true;
    float clear_depth_value = 1.0f;
    uint32_t viewport_width = 1280;
    uint32_t viewport_height = 720;
};

class SceneSubmissionQueue; // Forward declaration

class IRendererBackend {
public:
    virtual ~IRendererBackend() = default;

    [[nodiscard]] virtual RendererBackendType backend_type() const noexcept = 0;
    [[nodiscard]] virtual std::string_view backend_name() const noexcept = 0;

    virtual base::Result<void> initialize(const RenderBackendConfig& config) = 0;
    virtual void shutdown() = 0;
    [[nodiscard]] virtual bool is_initialized() const noexcept = 0;

    virtual base::Result<void> begin_frame() = 0;
    virtual base::Result<void> end_frame() = 0;
    virtual base::Result<void> execute_pass(const RenderPassDescriptor& pass, const SceneSubmissionQueue& queue) = 0;

    [[nodiscard]] virtual RenderCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual RenderFrameStats current_frame_stats() const noexcept = 0;
};

class NullRendererBackend : public IRendererBackend {
public:
    NullRendererBackend() = default;

    [[nodiscard]] RendererBackendType backend_type() const noexcept override { return RendererBackendType::Null; }
    [[nodiscard]] std::string_view backend_name() const noexcept override { return "Null Backend"; }

    base::Result<void> initialize(const RenderBackendConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const noexcept override { return initialized_; }

    base::Result<void> begin_frame() override;
    base::Result<void> end_frame() override;
    base::Result<void> execute_pass(const RenderPassDescriptor& pass, const SceneSubmissionQueue& queue) override;

    [[nodiscard]] RenderCapabilities capabilities() const noexcept override;
    [[nodiscard]] RenderFrameStats current_frame_stats() const noexcept override { return stats_; }

private:
    bool initialized_ = false;
    RenderFrameStats stats_{};
};

class RenderEngine {
public:
    RenderEngine();
    explicit RenderEngine(std::unique_ptr<IRendererBackend> backend);
    ~RenderEngine();

    base::Result<void> initialize(const RenderBackendConfig& config);
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept;

    void set_backend(std::unique_ptr<IRendererBackend> backend);
    [[nodiscard]] IRendererBackend* backend() noexcept { return backend_.get(); }
    [[nodiscard]] const IRendererBackend* backend() const noexcept { return backend_.get(); }

    base::Result<void> begin_frame();
    base::Result<void> end_frame();
    base::Result<void> render_pass(const RenderPassDescriptor& pass, const SceneSubmissionQueue& queue);

    [[nodiscard]] RenderCapabilities capabilities() const noexcept;
    [[nodiscard]] RenderFrameStats frame_stats() const noexcept;

private:
    std::unique_ptr<IRendererBackend> backend_;
    bool in_frame_ = false;
};

// =============================================================================
// REN-02: Viewport Camera, Scene Submission, Lighting & Material Pipeline
// =============================================================================

enum class CameraProjectionType {
    Perspective,
    Orthographic
};

struct FrustumPlane {
    base::math::Vector3<double> normal{0.0, 1.0, 0.0};
    double distance = 0.0;

    [[nodiscard]] double signed_distance(const base::math::Vector3<double>& point) const noexcept {
        return normal.dot(point) + distance;
    }
};

struct Frustum {
    std::array<FrustumPlane, 6> planes; // Left, Right, Bottom, Top, Near, Far

    [[nodiscard]] bool intersects_aabb(const base::math::Vector3<double>& min_pt,
                                       const base::math::Vector3<double>& max_pt) const noexcept;
    [[nodiscard]] bool intersects_sphere(const base::math::Vector3<double>& center, double radius) const noexcept;
};

class ViewportCamera {
public:
    ViewportCamera() = default;

    CameraProjectionType projection_type = CameraProjectionType::Perspective;
    base::math::Vector3<double> position{0.0, 2.0, 5.0};
    base::math::Vector3<double> target{0.0, 0.0, 0.0};
    base::math::Vector3<double> up{0.0, 1.0, 0.0};

    double fov_degrees = 60.0;
    double near_plane = 0.1;
    double far_plane = 1000.0;
    double aspect_ratio = 16.0 / 9.0;
    double ortho_size = 10.0;

    [[nodiscard]] base::math::Vector3<double> forward() const noexcept;
    [[nodiscard]] base::math::Vector3<double> right() const noexcept;
    [[nodiscard]] base::math::Vector3<double> up_vector() const noexcept;

    [[nodiscard]] base::math::Matrix4x4<double> view_matrix() const noexcept;
    [[nodiscard]] base::math::Matrix4x4<double> projection_matrix() const noexcept;
    [[nodiscard]] base::math::Matrix4x4<double> view_projection_matrix() const noexcept;

    [[nodiscard]] Frustum extract_frustum() const noexcept;

    void orbit(double delta_yaw_rad, double delta_pitch_rad);
    void pan(double delta_x, double delta_y);
    void zoom(double delta_zoom);
    void look_at(const base::math::Vector3<double>& eye_pos, const base::math::Vector3<double>& target_pos);
};

enum class LightType {
    Ambient,
    Directional,
    Point,
    Spot
};

struct LightSource {
    std::string id = "light_main";
    LightType type = LightType::Directional;
    base::math::Vector3<double> color{1.0, 1.0, 1.0};
    double intensity = 1.0;
    base::math::Vector3<double> position{0.0, 10.0, 5.0};
    base::math::Vector3<double> direction{0.0, -1.0, -0.5};
    double range = 100.0;
    double spot_angle_degrees = 45.0;
    bool cast_shadows = false;
};

struct LightingEnvironment {
    base::math::Vector3<double> ambient_color{0.15, 0.15, 0.2};
    double ambient_intensity = 0.4;
    std::vector<LightSource> lights;

    void add_light(const LightSource& light) { lights.push_back(light); }
    [[nodiscard]] const LightSource* find_light(std::string_view id) const noexcept;
    bool remove_light(std::string_view id);
};

enum class ShaderModel {
    PBRMetallicRoughness,
    Phong,
    FlatUnlit,
    Wireframe
};

struct RenderMaterial {
    std::string material_id = "default_material";
    ShaderModel shader_model = ShaderModel::PBRMetallicRoughness;
    base::math::Vector3<double> albedo{0.8, 0.8, 0.8};
    double roughness = 0.5;
    double metallic = 0.0;
    base::math::Vector3<double> emissive{0.0, 0.0, 0.0};
    double opacity = 1.0;
    bool double_sided = false;
    bool wireframe = false;
};

struct RenderInstance {
    std::string instance_id;
    std::string mesh_id;
    std::string material_id;
    base::math::Transform<double> transform;
    base::math::Vector3<double> bounds_min{-0.5, -0.5, -0.5};
    base::math::Vector3<double> bounds_max{0.5, 0.5, 0.5};
    bool visible = true;
    uint32_t layer_mask = 0x01;

    [[nodiscard]] std::pair<base::math::Vector3<double>, base::math::Vector3<double>> world_bounds() const noexcept;
};

class SceneSubmissionQueue {
public:
    SceneSubmissionQueue() = default;

    void submit(const RenderInstance& instance);
    void clear() noexcept;

    [[nodiscard]] size_t instance_count() const noexcept { return instances_.size(); }
    [[nodiscard]] const std::vector<RenderInstance>& instances() const noexcept { return instances_; }

    [[nodiscard]] std::vector<RenderInstance> cull_and_sort(const ViewportCamera& camera,
                                                            uint32_t& out_culled_count) const;

    void register_material(const RenderMaterial& material);
    [[nodiscard]] const RenderMaterial* find_material(std::string_view material_id) const noexcept;

    void set_lighting(const LightingEnvironment& env) { lighting_ = env; }
    [[nodiscard]] const LightingEnvironment& lighting() const noexcept { return lighting_; }

private:
    std::vector<RenderInstance> instances_;
    std::unordered_map<std::string, RenderMaterial> materials_;
    LightingEnvironment lighting_;
};

// =============================================================================
// REN-03: 2D Schematic vs 3D Scene Presentation Modes
// =============================================================================

enum class PresentationMode {
    Mode2D_Schematic,
    Mode3D_Scene,
    Split_Hybrid
};

struct Schematic2DPrimitive {
    enum class Type { Wire, SymbolBox, Pin, TextLabel, BusLine, Junction };

    Type type = Type::SymbolBox;
    std::string id;
    base::math::Vector2<double> position{0.0, 0.0};
    base::math::Vector2<double> size{100.0, 50.0};
    std::vector<base::math::Vector2<double>> points; // For polylines / wires
    std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
    std::string label;
    int layer = 0;
};

class SchematicRenderPipeline {
public:
    SchematicRenderPipeline() = default;

    void set_viewport(double pan_x, double pan_y, double zoom_factor);
    [[nodiscard]] base::math::Vector2<double> canvas_to_screen(const base::math::Vector2<double>& pt) const noexcept;
    [[nodiscard]] base::math::Vector2<double> screen_to_canvas(const base::math::Vector2<double>& pt) const noexcept;

    void submit_primitive(const Schematic2DPrimitive& prim);
    void clear() noexcept;

    [[nodiscard]] size_t primitive_count() const noexcept { return primitives_.size(); }
    [[nodiscard]] const std::vector<Schematic2DPrimitive>& primitives() const noexcept { return primitives_; }

    [[nodiscard]] std::pair<base::math::Vector2<double>, base::math::Vector2<double>> bounding_box() const noexcept;

private:
    double pan_x_ = 0.0;
    double pan_y_ = 0.0;
    double zoom_ = 1.0;
    std::vector<Schematic2DPrimitive> primitives_;
};

class ViewportPresentationController {
public:
    ViewportPresentationController() = default;

    void set_mode(PresentationMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] PresentationMode mode() const noexcept { return mode_; }

    void set_split_ratio(double ratio) noexcept {
        split_ratio_ = base::math::clamp(ratio, 0.05, 0.95);
    }
    [[nodiscard]] double split_ratio() const noexcept { return split_ratio_; }

    [[nodiscard]] SchematicRenderPipeline& schematic_pipeline() noexcept { return schematic_pipeline_; }
    [[nodiscard]] const SchematicRenderPipeline& schematic_pipeline() const noexcept { return schematic_pipeline_; }

    [[nodiscard]] SceneSubmissionQueue& scene_3d_queue() noexcept { return scene_3d_queue_; }
    [[nodiscard]] const SceneSubmissionQueue& scene_3d_queue() const noexcept { return scene_3d_queue_; }

    [[nodiscard]] ViewportCamera& camera() noexcept { return camera_; }
    [[nodiscard]] const ViewportCamera& camera() const noexcept { return camera_; }

private:
    PresentationMode mode_ = PresentationMode::Mode3D_Scene;
    double split_ratio_ = 0.5;
    SchematicRenderPipeline schematic_pipeline_;
    SceneSubmissionQueue scene_3d_queue_;
    ViewportCamera camera_;
};

// =============================================================================
// REN-04: Object Picking, Ray Casting, Gizmos, Debug Draw
// =============================================================================

struct Ray {
    base::math::Vector3<double> origin{0.0, 0.0, 0.0};
    base::math::Vector3<double> direction{0.0, 0.0, -1.0};

    [[nodiscard]] base::math::Vector3<double> point_at(double t) const noexcept {
        return origin + direction * t;
    }
};

struct RayHit {
    bool hit = false;
    double distance = std::numeric_limits<double>::infinity();
    base::math::Vector3<double> point{0.0, 0.0, 0.0};
    base::math::Vector3<double> normal{0.0, 1.0, 0.0};
    std::string instance_id;
    uint32_t triangle_index = 0;
};

class RayCaster {
public:
    [[nodiscard]] static Ray screen_point_to_ray(const ViewportCamera& camera,
                                                 double screen_x, double screen_y,
                                                 double viewport_w, double viewport_h);

    [[nodiscard]] static bool intersect_ray_aabb(const Ray& ray,
                                                 const base::math::Vector3<double>& box_min,
                                                 const base::math::Vector3<double>& box_max,
                                                 double& out_t_min, double& out_t_max) noexcept;

    [[nodiscard]] static bool intersect_ray_triangle(const Ray& ray,
                                                     const base::math::Vector3<double>& v0,
                                                     const base::math::Vector3<double>& v1,
                                                     const base::math::Vector3<double>& v2,
                                                     double& out_t,
                                                     base::math::Vector3<double>& out_normal) noexcept;

    [[nodiscard]] static RayHit cast_ray_mesh(const Ray& ray,
                                              const TriangleMesh& mesh,
                                              const base::math::Transform<double>& transform,
                                              std::string_view instance_id);

    [[nodiscard]] static std::optional<RayHit> pick_closest(
        const Ray& ray,
        const SceneSubmissionQueue& queue,
        const std::unordered_map<std::string, TriangleMesh>& mesh_assets);
};

enum class GizmoMode { Translate, Rotate, Scale };
enum class GizmoAxis { None, X, Y, Z, All };

class TransformGizmo {
public:
    TransformGizmo() = default;

    GizmoMode mode = GizmoMode::Translate;
    base::math::Vector3<double> position{0.0, 0.0, 0.0};
    double axis_length = 1.0;
    double handle_radius = 0.1;

    [[nodiscard]] GizmoAxis test_hover(const Ray& ray) const;
    [[nodiscard]] base::math::Vector3<double> compute_translation_delta(
        GizmoAxis axis, const Ray& prev_ray, const Ray& curr_ray) const;
};

struct DebugLine {
    base::math::Vector3<double> start;
    base::math::Vector3<double> end;
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    int duration_frames = 1;
};

struct DebugBox {
    base::math::Vector3<double> min_pt;
    base::math::Vector3<double> max_pt;
    std::array<float, 4> color{0.0f, 1.0f, 0.0f, 1.0f};
    int duration_frames = 1;
};

class DebugDrawQueue {
public:
    DebugDrawQueue() = default;

    void add_line(const base::math::Vector3<double>& start,
                  const base::math::Vector3<double>& end,
                  const std::array<float, 4>& color = {1.0f, 1.0f, 1.0f, 1.0f},
                  int duration_frames = 1);

    void add_box(const base::math::Vector3<double>& min_pt,
                 const base::math::Vector3<double>& max_pt,
                 const std::array<float, 4>& color = {0.0f, 1.0f, 0.0f, 1.0f},
                 int duration_frames = 1);

    void add_axis_triad(const base::math::Vector3<double>& origin, double size = 1.0, int duration_frames = 1);

    void tick_frame();
    void clear() noexcept;

    [[nodiscard]] const std::vector<DebugLine>& lines() const noexcept { return lines_; }
    [[nodiscard]] const std::vector<DebugBox>& boxes() const noexcept { return boxes_; }

private:
    std::vector<DebugLine> lines_;
    std::vector<DebugBox> boxes_;
};

// =============================================================================
// REN-05: GPU Resource Lifetime, Asset Cache & Asynchronous Loading
// =============================================================================

enum class GpuResourceType {
    VertexBuffer,
    IndexBuffer,
    Texture2D,
    ShaderProgram
};

class GpuResource {
public:
    GpuResource(base::Uuid id, std::string name, GpuResourceType type, size_t size_bytes);
    ~GpuResource();

    GpuResource(const GpuResource&) = delete;
    GpuResource& operator=(const GpuResource&) = delete;
    GpuResource(GpuResource&&) noexcept;
    GpuResource& operator=(GpuResource&&) noexcept;

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] GpuResourceType type() const noexcept { return type_; }
    [[nodiscard]] size_t size_bytes() const noexcept { return size_bytes_; }
    [[nodiscard]] bool is_resident() const noexcept { return is_resident_; }

    void mark_resident(bool res) noexcept { is_resident_ = res; }

private:
    base::Uuid id_;
    std::string name_;
    GpuResourceType type_;
    size_t size_bytes_ = 0;
    bool is_resident_ = false;
};

class AssetCache {
public:
    explicit AssetCache(size_t max_memory_budget_bytes = 100 * 1024 * 1024);

    void insert_mesh(std::string key, TriangleMesh mesh);
    [[nodiscard]] const TriangleMesh* find_mesh(std::string_view key) const noexcept;
    bool remove_mesh(std::string_view key);

    [[nodiscard]] size_t current_memory_bytes() const noexcept { return current_memory_bytes_; }
    [[nodiscard]] size_t memory_budget_bytes() const noexcept { return max_memory_budget_bytes_; }
    [[nodiscard]] size_t mesh_count() const noexcept { return meshes_.size(); }

    void purge_unreferenced();

private:
    size_t max_memory_budget_bytes_;
    size_t current_memory_bytes_ = 0;
    std::unordered_map<std::string, TriangleMesh> meshes_;
    std::unordered_map<std::string, size_t> mesh_sizes_;
    std::vector<std::string> lru_order_;

    void evict_if_needed();
};

enum class AssetLoadStatus {
    Unloaded,
    Queued,
    Loading,
    Ready,
    Failed
};

class AsyncAssetLoader {
public:
    AsyncAssetLoader() = default;

    void queue_mesh_load(std::string asset_id, std::function<base::Result<TriangleMesh>()> load_fn);
    void update_progress();

    [[nodiscard]] AssetLoadStatus status(std::string_view asset_id) const noexcept;
    [[nodiscard]] double progress(std::string_view asset_id) const noexcept;
    [[nodiscard]] const TriangleMesh* get_loaded_mesh(std::string_view asset_id) const noexcept;

private:
    struct Task {
        std::string asset_id;
        std::function<base::Result<TriangleMesh>()> load_fn;
        AssetLoadStatus status = AssetLoadStatus::Queued;
        double progress = 0.0;
        std::optional<TriangleMesh> result;
        std::string error_msg;
    };

    std::unordered_map<std::string, Task> tasks_;
};

// =============================================================================
// REN-06: Scientific Visualization Layers (Non-Invasive Overlays)
// =============================================================================

enum class ColormapType {
    Viridis,
    Jet,
    CoolWarm,
    Plasma,
    Grayscale
};

class Colormap {
public:
    [[nodiscard]] static std::array<float, 4> map_value(double val, double min_val, double max_val,
                                                        ColormapType cmap = ColormapType::Viridis) noexcept;
};

struct ScalarFieldSample {
    base::math::Vector3<double> position;
    double value = 0.0;
};

class ScalarFieldContourLayer {
public:
    ScalarFieldContourLayer() = default;

    void set_samples(std::vector<ScalarFieldSample> samples);
    void set_value_range(double min_val, double max_val) noexcept;
    void auto_fit_range() noexcept;

    void set_colormap(ColormapType cmap) noexcept { colormap_ = cmap; }
    [[nodiscard]] ColormapType colormap() const noexcept { return colormap_; }

    [[nodiscard]] double min_value() const noexcept { return min_val_; }
    [[nodiscard]] double max_value() const noexcept { return max_val_; }
    [[nodiscard]] const std::vector<ScalarFieldSample>& samples() const noexcept { return samples_; }

    [[nodiscard]] std::vector<std::array<float, 4>> generate_sample_colors() const;

private:
    std::vector<ScalarFieldSample> samples_;
    double min_val_ = 0.0;
    double max_val_ = 1.0;
    ColormapType colormap_ = ColormapType::Viridis;
};

struct VectorFieldSample {
    base::math::Vector3<double> position;
    base::math::Vector3<double> vector;
};

class VectorFieldStreamlineLayer {
public:
    VectorFieldStreamlineLayer() = default;

    void set_field(std::vector<VectorFieldSample> samples);
    void set_arrow_scale(double scale) noexcept { arrow_scale_ = scale; }
    [[nodiscard]] double arrow_scale() const noexcept { return arrow_scale_; }

    [[nodiscard]] std::vector<std::pair<base::math::Vector3<double>, base::math::Vector3<double>>>
    generate_arrow_glyphs() const;

    [[nodiscard]] std::vector<std::vector<base::math::Vector3<double>>>
    trace_streamlines(const std::vector<base::math::Vector3<double>>& seeds,
                      double step_size = 0.1, uint32_t max_steps = 50) const;

private:
    std::vector<VectorFieldSample> samples_;
    double arrow_scale_ = 1.0;
};

struct ProbeMarker {
    std::string probe_id;
    base::math::Vector3<double> position;
    std::string label;
    double scalar_value = 0.0;
    std::string unit = "V";
};

class ProbeMarkerOverlay {
public:
    ProbeMarkerOverlay() = default;

    void add_probe(const ProbeMarker& probe);
    void update_value(std::string_view probe_id, double val);
    [[nodiscard]] const ProbeMarker* find_probe(std::string_view probe_id) const noexcept;
    [[nodiscard]] const std::vector<ProbeMarker>& probes() const noexcept { return probes_; }

private:
    std::vector<ProbeMarker> probes_;
};

class DeformationMeshOverlay {
public:
    DeformationMeshOverlay() = default;

    void set_magnification_factor(double factor) noexcept { factor_ = factor; }
    [[nodiscard]] double magnification_factor() const noexcept { return factor_; }

    // Pure read-only transformation: returns displaced mesh without mutating source mesh
    [[nodiscard]] TriangleMesh compute_deformed_mesh(
        const TriangleMesh& base_mesh,
        const std::vector<base::math::Vector3<double>>& nodal_displacements) const;

private:
    double factor_ = 100.0;
};

// =============================================================================
// REN-07: Render Profiling (Draw Calls, Memory, Shader Compilations, LOD)
// =============================================================================

class RenderProfiler {
public:
    RenderProfiler() = default;

    void record_frame(const RenderFrameStats& stats);
    [[nodiscard]] uint64_t total_frames() const noexcept { return total_frames_; }
    [[nodiscard]] double average_cpu_frame_time_ms() const noexcept;
    [[nodiscard]] double peak_cpu_frame_time_ms() const noexcept { return peak_cpu_time_; }
    [[nodiscard]] uint32_t peak_draw_calls() const noexcept { return peak_draw_calls_; }

    void record_shader_compilation(double compile_time_ms, bool cache_hit);
    [[nodiscard]] uint32_t compiled_shader_count() const noexcept { return shader_compiled_count_; }
    [[nodiscard]] uint32_t shader_cache_hits() const noexcept { return shader_cache_hits_; }

    void update_lod_metrics(uint32_t lod0_count, uint32_t lod1_count, uint32_t lod2_count);
    [[nodiscard]] std::array<uint32_t, 3> lod_counts() const noexcept { return lod_counts_; }

    [[nodiscard]] std::string to_json_report() const;

private:
    uint64_t total_frames_ = 0;
    double accumulated_cpu_time_ = 0.0;
    double peak_cpu_time_ = 0.0;
    uint32_t peak_draw_calls_ = 0;

    uint32_t shader_compiled_count_ = 0;
    uint32_t shader_cache_hits_ = 0;
    double accumulated_shader_compile_time_ = 0.0;

    std::array<uint32_t, 3> lod_counts_{0, 0, 0};
};

// =============================================================================
// REN-08: Headless Software Mode & Framebuffer
// =============================================================================

class HeadlessFramebuffer {
public:
    HeadlessFramebuffer(uint32_t width = 800, uint32_t height = 600);

    void resize(uint32_t width, uint32_t height);
    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }

    void clear(uint32_t argb_color = 0xFF101015, float depth = 1.0f);
    void set_pixel(uint32_t x, uint32_t y, uint32_t argb_color, float depth = 0.0f);
    [[nodiscard]] uint32_t get_pixel(uint32_t x, uint32_t y) const noexcept;
    [[nodiscard]] float get_depth(uint32_t x, uint32_t y) const noexcept;

    void draw_line(int x0, int y0, int x1, int y1, uint32_t argb_color);
    void draw_filled_rect(int x, int y, int w, int h, uint32_t argb_color);

    [[nodiscard]] uint64_t compute_buffer_checksum() const noexcept;
    [[nodiscard]] std::string export_ppm_ascii() const;

    [[nodiscard]] const std::vector<uint32_t>& color_buffer() const noexcept { return color_buffer_; }
    [[nodiscard]] const std::vector<float>& depth_buffer() const noexcept { return depth_buffer_; }

private:
    uint32_t width_;
    uint32_t height_;
    std::vector<uint32_t> color_buffer_;
    std::vector<float> depth_buffer_;
};

class HeadlessSoftwareBackend : public IRendererBackend {
public:
    HeadlessSoftwareBackend();
    explicit HeadlessSoftwareBackend(uint32_t width, uint32_t height);

    [[nodiscard]] RendererBackendType backend_type() const noexcept override {
        return RendererBackendType::HeadlessSoftware;
    }
    [[nodiscard]] std::string_view backend_name() const noexcept override {
        return "Headless Software Backend";
    }

    base::Result<void> initialize(const RenderBackendConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const noexcept override { return initialized_; }

    base::Result<void> begin_frame() override;
    base::Result<void> end_frame() override;
    base::Result<void> execute_pass(const RenderPassDescriptor& pass, const SceneSubmissionQueue& queue) override;

    [[nodiscard]] RenderCapabilities capabilities() const noexcept override;
    [[nodiscard]] RenderFrameStats current_frame_stats() const noexcept override { return stats_; }

    [[nodiscard]] HeadlessFramebuffer& framebuffer() noexcept { return framebuffer_; }
    [[nodiscard]] const HeadlessFramebuffer& framebuffer() const noexcept { return framebuffer_; }

private:
    bool initialized_ = false;
    HeadlessFramebuffer framebuffer_;
    RenderFrameStats stats_{};
};

// =============================================================================
// REN-09: Renderer Capability Detection & Fallback Path
// =============================================================================

enum class RenderFallbackPolicy {
    FailIfUnavailable,
    FallbackToSoftware,
    SelectBestAvailable
};

class MockHardwareRendererBackend : public IRendererBackend {
public:
    MockHardwareRendererBackend(RendererBackendType type, bool should_fail_init = false);

    [[nodiscard]] RendererBackendType backend_type() const noexcept override { return type_; }
    [[nodiscard]] std::string_view backend_name() const noexcept override;

    base::Result<void> initialize(const RenderBackendConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const noexcept override { return initialized_; }

    base::Result<void> begin_frame() override;
    base::Result<void> end_frame() override;
    base::Result<void> execute_pass(const RenderPassDescriptor& pass, const SceneSubmissionQueue& queue) override;

    [[nodiscard]] RenderCapabilities capabilities() const noexcept override;
    [[nodiscard]] RenderFrameStats current_frame_stats() const noexcept override { return stats_; }

    void set_should_fail_init(bool fail) noexcept { should_fail_init_ = fail; }

private:
    RendererBackendType type_;
    bool should_fail_init_ = false;
    bool initialized_ = false;
    RenderFrameStats stats_{};
};

class RenderDeviceManager {
public:
    explicit RenderDeviceManager(RenderFallbackPolicy policy = RenderFallbackPolicy::FallbackToSoftware);

    void set_fallback_policy(RenderFallbackPolicy policy) noexcept { policy_ = policy; }
    [[nodiscard]] RenderFallbackPolicy fallback_policy() const noexcept { return policy_; }

    base::Result<std::unique_ptr<IRendererBackend>> create_backend(
        RendererBackendType preferred_type,
        const RenderBackendConfig& config);

    [[nodiscard]] std::string_view last_fallback_reason() const noexcept { return fallback_reason_; }
    [[nodiscard]] bool used_fallback() const noexcept { return used_fallback_; }

private:
    RenderFallbackPolicy policy_;
    bool used_fallback_ = false;
    std::string fallback_reason_;
};

// =============================================================================
// REN-10: Independent Rendering Test Suite & Validation
// =============================================================================

class RenderTestHarness {
public:
    [[nodiscard]] static double compute_framebuffer_mse(const HeadlessFramebuffer& a,
                                                        const HeadlessFramebuffer& b);

    [[nodiscard]] static bool verify_frustum_culling_invariance(
        const SceneSubmissionQueue& queue,
        const ViewportCamera& camera);

    [[nodiscard]] static bool verify_physics_state_decoupling(
        const TriangleMesh& physics_mesh,
        const DeformationMeshOverlay& overlay,
        const std::vector<base::math::Vector3<double>>& displacements);
};

} // namespace shunya::core::domain
