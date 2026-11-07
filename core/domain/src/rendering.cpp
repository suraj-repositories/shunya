#include "shunya/core/domain/rendering.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::core::domain {

namespace {
base::Error make_render_error(std::string message, int32_t code = 4001) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::System,
        .severity = base::Severity::Error,
        .message = std::move(message),
        .details = ""
    };
}
}

// =============================================================================
// REN-01: Renderer Abstraction & Backend Strategy
// =============================================================================

std::string_view backend_type_to_string(RendererBackendType type) noexcept {
    switch (type) {
        case RendererBackendType::Null: return "Null";
        case RendererBackendType::HeadlessSoftware: return "HeadlessSoftware";
        case RendererBackendType::OpenGL: return "OpenGL";
        case RendererBackendType::Vulkan: return "Vulkan";
        case RendererBackendType::Direct3D12: return "Direct3D12";
        case RendererBackendType::Metal: return "Metal";
    }
    return "Unknown";
}

std::optional<RendererBackendType> backend_type_from_string(std::string_view str) noexcept {
    if (str == "Null") return RendererBackendType::Null;
    if (str == "HeadlessSoftware") return RendererBackendType::HeadlessSoftware;
    if (str == "OpenGL") return RendererBackendType::OpenGL;
    if (str == "Vulkan") return RendererBackendType::Vulkan;
    if (str == "Direct3D12") return RendererBackendType::Direct3D12;
    if (str == "Metal") return RendererBackendType::Metal;
    return std::nullopt;
}

base::Result<void> NullRendererBackend::initialize(const RenderBackendConfig&) {
    initialized_ = true;
    stats_.reset();
    return base::Result<void>::ok();
}

void NullRendererBackend::shutdown() {
    initialized_ = false;
}

base::Result<void> NullRendererBackend::begin_frame() {
    if (!initialized_) {
        return base::Result<void>::err(make_render_error("NullRendererBackend not initialized"));
    }
    stats_.reset();
    return base::Result<void>::ok();
}

base::Result<void> NullRendererBackend::end_frame() {
    if (!initialized_) {
        return base::Result<void>::err(make_render_error("NullRendererBackend not initialized"));
    }
    return base::Result<void>::ok();
}

base::Result<void> NullRendererBackend::execute_pass(const RenderPassDescriptor&, const SceneSubmissionQueue& queue) {
    if (!initialized_) {
        return base::Result<void>::err(make_render_error("NullRendererBackend not initialized"));
    }
    stats_.draw_calls += static_cast<uint32_t>(queue.instance_count());
    stats_.instances_rendered += static_cast<uint32_t>(queue.instance_count());
    return base::Result<void>::ok();
}

RenderCapabilities NullRendererBackend::capabilities() const noexcept {
    RenderCapabilities caps;
    caps.gpu_vendor = "None";
    caps.renderer_name = "Null Device";
    caps.api_version = "0.0";
    caps.is_hardware_accelerated = false;
    return caps;
}

RenderEngine::RenderEngine()
    : backend_(std::make_unique<NullRendererBackend>()) {}

RenderEngine::RenderEngine(std::unique_ptr<IRendererBackend> backend)
    : backend_(backend ? std::move(backend) : std::make_unique<NullRendererBackend>()) {}

RenderEngine::~RenderEngine() {
    shutdown();
}

base::Result<void> RenderEngine::initialize(const RenderBackendConfig& config) {
    if (!backend_) {
        backend_ = std::make_unique<NullRendererBackend>();
    }
    return backend_->initialize(config);
}

void RenderEngine::shutdown() {
    if (backend_ && backend_->is_initialized()) {
        backend_->shutdown();
    }
}

bool RenderEngine::is_initialized() const noexcept {
    return backend_ && backend_->is_initialized();
}

void RenderEngine::set_backend(std::unique_ptr<IRendererBackend> backend) {
    if (backend_ && backend_->is_initialized()) {
        backend_->shutdown();
    }
    backend_ = backend ? std::move(backend) : std::make_unique<NullRendererBackend>();
}

base::Result<void> RenderEngine::begin_frame() {
    if (!backend_) {
        return base::Result<void>::err(make_render_error("No backend attached to RenderEngine"));
    }
    auto res = backend_->begin_frame();
    if (res.is_ok()) {
        in_frame_ = true;
    }
    return res;
}

base::Result<void> RenderEngine::end_frame() {
    if (!backend_) {
        return base::Result<void>::err(make_render_error("No backend attached to RenderEngine"));
    }
    in_frame_ = false;
    return backend_->end_frame();
}

base::Result<void> RenderEngine::render_pass(const RenderPassDescriptor& pass, const SceneSubmissionQueue& queue) {
    if (!backend_) {
        return base::Result<void>::err(make_render_error("No backend attached to RenderEngine"));
    }
    return backend_->execute_pass(pass, queue);
}

RenderCapabilities RenderEngine::capabilities() const noexcept {
    if (!backend_) return {};
    return backend_->capabilities();
}

RenderFrameStats RenderEngine::frame_stats() const noexcept {
    if (!backend_) return {};
    return backend_->current_frame_stats();
}

// =============================================================================
// REN-02: Viewport Camera, Scene Submission, Lighting & Material Pipeline
// =============================================================================

bool Frustum::intersects_aabb(const base::math::Vector3<double>& min_pt,
                              const base::math::Vector3<double>& max_pt) const noexcept {
    for (const auto& plane : planes) {
        // Find p-vertex: extreme point in direction of normal
        base::math::Vector3<double> p{
            plane.normal.x >= 0.0 ? max_pt.x : min_pt.x,
            plane.normal.y >= 0.0 ? max_pt.y : min_pt.y,
            plane.normal.z >= 0.0 ? max_pt.z : min_pt.z
        };
        if (plane.signed_distance(p) < 0.0) {
            return false;
        }
    }
    return true;
}

bool Frustum::intersects_sphere(const base::math::Vector3<double>& center, double radius) const noexcept {
    for (const auto& plane : planes) {
        if (plane.signed_distance(center) < -radius) {
            return false;
        }
    }
    return true;
}

base::math::Vector3<double> ViewportCamera::forward() const noexcept {
    base::math::Vector3<double> f = target - position;
    return f.normalized();
}

base::math::Vector3<double> ViewportCamera::right() const noexcept {
    base::math::Vector3<double> f = forward();
    base::math::Vector3<double> r = f.cross(up);
    return r.normalized();
}

base::math::Vector3<double> ViewportCamera::up_vector() const noexcept {
    base::math::Vector3<double> r = right();
    base::math::Vector3<double> f = forward();
    return r.cross(f).normalized();
}

base::math::Matrix4x4<double> ViewportCamera::view_matrix() const noexcept {
    base::math::Vector3<double> f = forward();
    base::math::Vector3<double> r = right();
    base::math::Vector3<double> u = up_vector();

    base::math::Matrix4x4<double> view;
    // Row 0
    view.m[0] = r.x;
    view.m[1] = r.y;
    view.m[2] = r.z;
    view.m[3] = -r.dot(position);

    // Row 1
    view.m[4] = u.x;
    view.m[5] = u.y;
    view.m[6] = u.z;
    view.m[7] = -u.dot(position);

    // Row 2
    view.m[8] = -f.x;
    view.m[9] = -f.y;
    view.m[10] = -f.z;
    view.m[11] = f.dot(position);

    // Row 3
    view.m[12] = 0.0;
    view.m[13] = 0.0;
    view.m[14] = 0.0;
    view.m[15] = 1.0;

    return view;
}

base::math::Matrix4x4<double> ViewportCamera::projection_matrix() const noexcept {
    base::math::Matrix4x4<double> proj;
    for (size_t i = 0; i < 16; ++i) proj.m[i] = 0.0;

    if (projection_type == CameraProjectionType::Perspective) {
        double fov_rad = fov_degrees * base::math::PI / 180.0;
        double f = 1.0 / std::tan(fov_rad * 0.5);
        double range = near_plane - far_plane;

        proj.m[0] = f / aspect_ratio;
        proj.m[5] = f;
        proj.m[10] = (far_plane + near_plane) / range;
        proj.m[11] = (2.0 * far_plane * near_plane) / range;
        proj.m[14] = -1.0;
    } else {
        double h = ortho_size;
        double w = ortho_size * aspect_ratio;
        double range = far_plane - near_plane;

        proj.m[0] = 2.0 / w;
        proj.m[5] = 2.0 / h;
        proj.m[10] = -2.0 / range;
        proj.m[11] = -(far_plane + near_plane) / range;
        proj.m[15] = 1.0;
    }

    return proj;
}

base::math::Matrix4x4<double> ViewportCamera::view_projection_matrix() const noexcept {
    return projection_matrix() * view_matrix();
}

Frustum ViewportCamera::extract_frustum() const noexcept {
    auto vp = view_projection_matrix();
    Frustum f;

    // Helper lambda to normalize a plane (A, B, C, D)
    auto make_plane = [](double a, double b, double c, double d) -> FrustumPlane {
        double len = std::sqrt(a * a + b * b + c * c);
        if (len < 1e-9) len = 1.0;
        return FrustumPlane{
            .normal = {a / len, b / len, c / len},
            .distance = d / len
        };
    };

    // Left: row 3 + row 0
    f.planes[0] = make_plane(vp.m[12] + vp.m[0], vp.m[13] + vp.m[1], vp.m[14] + vp.m[2], vp.m[15] + vp.m[3]);
    // Right: row 3 - row 0
    f.planes[1] = make_plane(vp.m[12] - vp.m[0], vp.m[13] - vp.m[1], vp.m[14] - vp.m[2], vp.m[15] - vp.m[3]);
    // Bottom: row 3 + row 1
    f.planes[2] = make_plane(vp.m[12] + vp.m[4], vp.m[13] + vp.m[5], vp.m[14] + vp.m[6], vp.m[15] + vp.m[7]);
    // Top: row 3 - row 1
    f.planes[3] = make_plane(vp.m[12] - vp.m[4], vp.m[13] - vp.m[5], vp.m[14] - vp.m[6], vp.m[15] - vp.m[7]);
    // Near: row 3 + row 2
    f.planes[4] = make_plane(vp.m[12] + vp.m[8], vp.m[13] + vp.m[9], vp.m[14] + vp.m[10], vp.m[15] + vp.m[11]);
    // Far: row 3 - row 2
    f.planes[5] = make_plane(vp.m[12] - vp.m[8], vp.m[13] - vp.m[9], vp.m[14] - vp.m[10], vp.m[15] - vp.m[11]);

    return f;
}

void ViewportCamera::orbit(double delta_yaw_rad, double delta_pitch_rad) {
    base::math::Vector3<double> rel = position - target;
    double radius = rel.length();
    if (radius < 1e-6) radius = 1.0;

    double yaw = std::atan2(rel.x, rel.z);
    double pitch = std::asin(std::clamp(rel.y / radius, -0.99, 0.99));

    yaw += delta_yaw_rad;
    pitch = std::clamp(pitch + delta_pitch_rad, -base::math::PI * 0.48, base::math::PI * 0.48);

    position.x = target.x + radius * std::cos(pitch) * std::sin(yaw);
    position.y = target.y + radius * std::sin(pitch);
    position.z = target.z + radius * std::cos(pitch) * std::cos(yaw);
}

void ViewportCamera::pan(double delta_x, double delta_y) {
    base::math::Vector3<double> r = right();
    base::math::Vector3<double> u = up_vector();
    base::math::Vector3<double> shift = r * delta_x + u * delta_y;
    position += shift;
    target += shift;
}

void ViewportCamera::zoom(double delta_zoom) {
    base::math::Vector3<double> f = forward();
    double dist = (target - position).length();
    double move_dist = delta_zoom * (dist * 0.2);
    if (dist - move_dist > 0.05) {
        position += f * move_dist;
    }
}

void ViewportCamera::look_at(const base::math::Vector3<double>& eye_pos, const base::math::Vector3<double>& target_pos) {
    position = eye_pos;
    target = target_pos;
}

const LightSource* LightingEnvironment::find_light(std::string_view id) const noexcept {
    for (const auto& light : lights) {
        if (light.id == id) return &light;
    }
    return nullptr;
}

bool LightingEnvironment::remove_light(std::string_view id) {
    auto it = std::remove_if(lights.begin(), lights.end(), [id](const LightSource& l) {
        return l.id == id;
    });
    if (it != lights.end()) {
        lights.erase(it, lights.end());
        return true;
    }
    return false;
}

std::pair<base::math::Vector3<double>, base::math::Vector3<double>> RenderInstance::world_bounds() const noexcept {
    // Transform all 8 local corners and compute bounding box
    std::array<base::math::Vector3<double>, 8> corners = {
        base::math::Vector3<double>{bounds_min.x, bounds_min.y, bounds_min.z},
        base::math::Vector3<double>{bounds_min.x, bounds_min.y, bounds_max.z},
        base::math::Vector3<double>{bounds_min.x, bounds_max.y, bounds_min.z},
        base::math::Vector3<double>{bounds_min.x, bounds_max.y, bounds_max.z},
        base::math::Vector3<double>{bounds_max.x, bounds_min.y, bounds_min.z},
        base::math::Vector3<double>{bounds_max.x, bounds_min.y, bounds_max.z},
        base::math::Vector3<double>{bounds_max.x, bounds_max.y, bounds_min.z},
        base::math::Vector3<double>{bounds_max.x, bounds_max.y, bounds_max.z}
    };

    base::math::Vector3<double> w_min{1e30, 1e30, 1e30};
    base::math::Vector3<double> w_max{-1e30, -1e30, -1e30};

    for (const auto& c : corners) {
        auto p = transform.transform_point(c);
        w_min.x = std::min(w_min.x, p.x);
        w_min.y = std::min(w_min.y, p.y);
        w_min.z = std::min(w_min.z, p.z);
        w_max.x = std::max(w_max.x, p.x);
        w_max.y = std::max(w_max.y, p.y);
        w_max.z = std::max(w_max.z, p.z);
    }
    return {w_min, w_max};
}

void SceneSubmissionQueue::submit(const RenderInstance& instance) {
    instances_.push_back(instance);
}

void SceneSubmissionQueue::clear() noexcept {
    instances_.clear();
}

void SceneSubmissionQueue::register_material(const RenderMaterial& material) {
    materials_[material.material_id] = material;
}

const RenderMaterial* SceneSubmissionQueue::find_material(std::string_view material_id) const noexcept {
    auto it = materials_.find(std::string(material_id));
    if (it != materials_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<RenderInstance> SceneSubmissionQueue::cull_and_sort(
    const ViewportCamera& camera, uint32_t& out_culled_count) const {

    Frustum frustum = camera.extract_frustum();
    std::vector<RenderInstance> visible;
    visible.reserve(instances_.size());
    out_culled_count = 0;

    for (const auto& inst : instances_) {
        if (!inst.visible) {
            out_culled_count++;
            continue;
        }
        auto [w_min, w_max] = inst.world_bounds();
        if (frustum.intersects_aabb(w_min, w_max)) {
            visible.push_back(inst);
        } else {
            out_culled_count++;
        }
    }

    // Sort front-to-back by distance from camera
    auto cam_pos = camera.position;
    std::sort(visible.begin(), visible.end(), [&cam_pos](const RenderInstance& a, const RenderInstance& b) {
        double dist_a = (a.transform.position - cam_pos).length_squared();
        double dist_b = (b.transform.position - cam_pos).length_squared();
        return dist_a < dist_b;
    });

    return visible;
}

// =============================================================================
// REN-03: 2D Schematic vs 3D Scene Presentation Modes
// =============================================================================

void SchematicRenderPipeline::set_viewport(double pan_x, double pan_y, double zoom_factor) {
    pan_x_ = pan_x;
    pan_y_ = pan_y;
    zoom_ = std::max(zoom_factor, 0.01);
}

base::math::Vector2<double> SchematicRenderPipeline::canvas_to_screen(const base::math::Vector2<double>& pt) const noexcept {
    return { (pt.x + pan_x_) * zoom_, (pt.y + pan_y_) * zoom_ };
}

base::math::Vector2<double> SchematicRenderPipeline::screen_to_canvas(const base::math::Vector2<double>& pt) const noexcept {
    return { (pt.x / zoom_) - pan_x_, (pt.y / zoom_) - pan_y_ };
}

void SchematicRenderPipeline::submit_primitive(const Schematic2DPrimitive& prim) {
    primitives_.push_back(prim);
}

void SchematicRenderPipeline::clear() noexcept {
    primitives_.clear();
}

std::pair<base::math::Vector2<double>, base::math::Vector2<double>> SchematicRenderPipeline::bounding_box() const noexcept {
    if (primitives_.empty()) {
        return {{0.0, 0.0}, {0.0, 0.0}};
    }
    base::math::Vector2<double> bmin{1e30, 1e30};
    base::math::Vector2<double> bmax{-1e30, -1e30};

    for (const auto& p : primitives_) {
        bmin.x = std::min(bmin.x, p.position.x);
        bmin.y = std::min(bmin.y, p.position.y);
        bmax.x = std::max(bmax.x, p.position.x + p.size.x);
        bmax.y = std::max(bmax.y, p.position.y + p.size.y);
        for (const auto& pt : p.points) {
            bmin.x = std::min(bmin.x, pt.x);
            bmin.y = std::min(bmin.y, pt.y);
            bmax.x = std::max(bmax.x, pt.x);
            bmax.y = std::max(bmax.y, pt.y);
        }
    }
    return {bmin, bmax};
}

// =============================================================================
// REN-04: Object Picking, Ray Casting, Gizmos, Debug Draw
// =============================================================================

Ray RayCaster::screen_point_to_ray(const ViewportCamera& camera,
                                   double screen_x, double screen_y,
                                   double viewport_w, double viewport_h) {
    if (viewport_w <= 0.0) viewport_w = 1.0;
    if (viewport_h <= 0.0) viewport_h = 1.0;

    double ndc_x = (2.0 * screen_x) / viewport_w - 1.0;
    double ndc_y = 1.0 - (2.0 * screen_y) / viewport_h;

    double fov_rad = camera.fov_degrees * base::math::PI / 180.0;
    double tan_half_fov = std::tan(fov_rad * 0.5);

    base::math::Vector3<double> f = camera.forward();
    base::math::Vector3<double> r = camera.right();
    base::math::Vector3<double> u = camera.up_vector();

    if (camera.projection_type == CameraProjectionType::Perspective) {
        base::math::Vector3<double> dir = f + r * (ndc_x * camera.aspect_ratio * tan_half_fov) + u * (ndc_y * tan_half_fov);
        return Ray{.origin = camera.position, .direction = dir.normalized()};
    } else {
        double half_h = camera.ortho_size * 0.5;
        double half_w = half_h * camera.aspect_ratio;
        base::math::Vector3<double> origin = camera.position + r * (ndc_x * half_w) + u * (ndc_y * half_h);
        return Ray{.origin = origin, .direction = f};
    }
}

bool RayCaster::intersect_ray_aabb(const Ray& ray,
                                   const base::math::Vector3<double>& box_min,
                                   const base::math::Vector3<double>& box_max,
                                   double& out_t_min, double& out_t_max) noexcept {
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax = std::numeric_limits<double>::infinity();

    const double origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const double dir[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const double bmin[3] = {box_min.x, box_min.y, box_min.z};
    const double bmax[3] = {box_max.x, box_max.y, box_max.z};

    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-9) {
            if (origin[i] < bmin[i] || origin[i] > bmax[i]) return false;
        } else {
            double ood = 1.0 / dir[i];
            double t1 = (bmin[i] - origin[i]) * ood;
            double t2 = (bmax[i] - origin[i]) * ood;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }

    out_t_min = tmin;
    out_t_max = tmax;
    return tmax >= 0.0;
}

bool RayCaster::intersect_ray_triangle(const Ray& ray,
                                       const base::math::Vector3<double>& v0,
                                       const base::math::Vector3<double>& v1,
                                       const base::math::Vector3<double>& v2,
                                       double& out_t,
                                       base::math::Vector3<double>& out_normal) noexcept {
    // Möller-Trumbore intersection algorithm
    const double EPS = 1e-8;
    base::math::Vector3<double> edge1 = v1 - v0;
    base::math::Vector3<double> edge2 = v2 - v0;
    base::math::Vector3<double> pvec = ray.direction.cross(edge2);
    double det = edge1.dot(pvec);

    if (std::abs(det) < EPS) return false;
    double inv_det = 1.0 / det;

    base::math::Vector3<double> tvec = ray.origin - v0;
    double u = tvec.dot(pvec) * inv_det;
    if (u < 0.0 || u > 1.0) return false;

    base::math::Vector3<double> qvec = tvec.cross(edge1);
    double v = ray.direction.dot(qvec) * inv_det;
    if (v < 0.0 || u + v > 1.0) return false;

    double t = edge2.dot(qvec) * inv_det;
    if (t < EPS) return false;

    out_t = t;
    out_normal = edge1.cross(edge2).normalized();
    if (det < 0.0) out_normal = -out_normal;
    return true;
}

RayHit RayCaster::cast_ray_mesh(const Ray& ray,
                                const TriangleMesh& mesh,
                                const base::math::Transform<double>& transform,
                                std::string_view instance_id) {
    RayHit best_hit;
    best_hit.hit = false;
    best_hit.distance = std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < mesh.faces.size(); ++i) {
        const auto& f = mesh.faces[i];
        if (f.v0 >= mesh.vertices.size() || f.v1 >= mesh.vertices.size() || f.v2 >= mesh.vertices.size()) {
            continue;
        }

        auto p0 = transform.transform_point(mesh.vertices[f.v0]);
        auto p1 = transform.transform_point(mesh.vertices[f.v1]);
        auto p2 = transform.transform_point(mesh.vertices[f.v2]);

        double t = 0.0;
        base::math::Vector3<double> norm;
        if (intersect_ray_triangle(ray, p0, p1, p2, t, norm)) {
            if (t < best_hit.distance) {
                best_hit.hit = true;
                best_hit.distance = t;
                best_hit.point = ray.point_at(t);
                best_hit.normal = norm;
                best_hit.instance_id = std::string(instance_id);
                best_hit.triangle_index = static_cast<uint32_t>(i);
            }
        }
    }

    return best_hit;
}

std::optional<RayHit> RayCaster::pick_closest(
    const Ray& ray,
    const SceneSubmissionQueue& queue,
    const std::unordered_map<std::string, TriangleMesh>& mesh_assets) {

    RayHit closest;
    closest.hit = false;
    closest.distance = std::numeric_limits<double>::infinity();

    for (const auto& inst : queue.instances()) {
        if (!inst.visible) continue;

        auto [w_min, w_max] = inst.world_bounds();
        double t_min = 0.0, t_max = 0.0;
        if (!intersect_ray_aabb(ray, w_min, w_max, t_min, t_max)) {
            continue;
        }

        auto it = mesh_assets.find(inst.mesh_id);
        if (it != mesh_assets.end()) {
            auto hit = cast_ray_mesh(ray, it->second, inst.transform, inst.instance_id);
            if (hit.hit && hit.distance < closest.distance) {
                closest = hit;
            }
        } else {
            // Fallback to bounding box hit
            if (t_min > 0.0 && t_min < closest.distance) {
                closest.hit = true;
                closest.distance = t_min;
                closest.point = ray.point_at(t_min);
                closest.normal = {0.0, 1.0, 0.0};
                closest.instance_id = inst.instance_id;
            }
        }
    }

    if (closest.hit) return closest;
    return std::nullopt;
}

GizmoAxis TransformGizmo::test_hover(const Ray& ray) const {
    // Check closest distance to X, Y, Z axis segments
    auto dist_to_segment = [](const Ray& r, const base::math::Vector3<double>& p0, const base::math::Vector3<double>& p1) -> double {
        base::math::Vector3<double> u = p1 - p0;
        base::math::Vector3<double> v = r.direction;
        base::math::Vector3<double> w = p0 - r.origin;
        double a = u.dot(u);
        double b = u.dot(v);
        double c = v.dot(v);
        double d = u.dot(w);
        double e = v.dot(w);
        double denom = a * c - b * b;
        if (denom < 1e-9) return 1e9;
        double s = std::clamp((b * e - c * d) / denom, 0.0, 1.0);
        double t = (a * e - b * d) / denom;
        if (t < 0.0) return 1e9;
        base::math::Vector3<double> pt_seg = p0 + u * s;
        base::math::Vector3<double> pt_ray = r.origin + v * t;
        return (pt_seg - pt_ray).length();
    };

    double d_x = dist_to_segment(ray, position, position + base::math::Vector3<double>{axis_length, 0.0, 0.0});
    double d_y = dist_to_segment(ray, position, position + base::math::Vector3<double>{0.0, axis_length, 0.0});
    double d_z = dist_to_segment(ray, position, position + base::math::Vector3<double>{0.0, 0.0, axis_length});

    double min_d = std::min({d_x, d_y, d_z});
    if (min_d > handle_radius) return GizmoAxis::None;

    if (min_d == d_x) return GizmoAxis::X;
    if (min_d == d_y) return GizmoAxis::Y;
    return GizmoAxis::Z;
}

base::math::Vector3<double> TransformGizmo::compute_translation_delta(
    GizmoAxis axis, const Ray& prev_ray, const Ray& curr_ray) const {

    base::math::Vector3<double> dir{0.0, 0.0, 0.0};
    if (axis == GizmoAxis::X) dir = {1.0, 0.0, 0.0};
    else if (axis == GizmoAxis::Y) dir = {0.0, 1.0, 0.0};
    else if (axis == GizmoAxis::Z) dir = {0.0, 0.0, 1.0};
    else return {0.0, 0.0, 0.0};

    // Project ray onto axis line
    auto project_to_axis = [this, &dir](const Ray& r) -> double {
        base::math::Vector3<double> w = r.origin - position;
        double a = dir.dot(dir);
        double b = dir.dot(r.direction);
        double d = dir.dot(w);
        double e = r.direction.dot(w);
        double denom = a - b * b;
        if (std::abs(denom) < 1e-9) return 0.0;
        return (b * e - d) / denom;
    };

    double t0 = project_to_axis(prev_ray);
    double t1 = project_to_axis(curr_ray);
    return dir * (t1 - t0);
}

void DebugDrawQueue::add_line(const base::math::Vector3<double>& start,
                              const base::math::Vector3<double>& end,
                              const std::array<float, 4>& color,
                              int duration_frames) {
    lines_.push_back(DebugLine{start, end, color, duration_frames});
}

void DebugDrawQueue::add_box(const base::math::Vector3<double>& min_pt,
                             const base::math::Vector3<double>& max_pt,
                             const std::array<float, 4>& color,
                             int duration_frames) {
    boxes_.push_back(DebugBox{min_pt, max_pt, color, duration_frames});
}

void DebugDrawQueue::add_axis_triad(const base::math::Vector3<double>& origin, double size, int duration_frames) {
    add_line(origin, origin + base::math::Vector3<double>{size, 0.0, 0.0}, {1.0f, 0.0f, 0.0f, 1.0f}, duration_frames);
    add_line(origin, origin + base::math::Vector3<double>{0.0, size, 0.0}, {0.0f, 1.0f, 0.0f, 1.0f}, duration_frames);
    add_line(origin, origin + base::math::Vector3<double>{0.0, 0.0, size}, {0.0f, 0.0f, 1.0f, 1.0f}, duration_frames);
}

void DebugDrawQueue::tick_frame() {
    for (auto& l : lines_) l.duration_frames--;
    for (auto& b : boxes_) b.duration_frames--;

    auto it_l = std::remove_if(lines_.begin(), lines_.end(), [](const DebugLine& l) {
        return l.duration_frames <= 0;
    });
    lines_.erase(it_l, lines_.end());

    auto it_b = std::remove_if(boxes_.begin(), boxes_.end(), [](const DebugBox& b) {
        return b.duration_frames <= 0;
    });
    boxes_.erase(it_b, boxes_.end());
}

void DebugDrawQueue::clear() noexcept {
    lines_.clear();
    boxes_.clear();
}

// =============================================================================
// REN-05: GPU Resource Lifetime, Asset Cache & Asynchronous Loading
// =============================================================================

GpuResource::GpuResource(base::Uuid id, std::string name, GpuResourceType type, size_t size_bytes)
    : id_(id), name_(std::move(name)), type_(type), size_bytes_(size_bytes), is_resident_(true) {}

GpuResource::~GpuResource() {
    is_resident_ = false;
}

GpuResource::GpuResource(GpuResource&& other) noexcept
    : id_(other.id_), name_(std::move(other.name_)), type_(other.type_),
      size_bytes_(other.size_bytes_), is_resident_(other.is_resident_) {
    other.is_resident_ = false;
    other.size_bytes_ = 0;
}

GpuResource& GpuResource::operator=(GpuResource&& other) noexcept {
    if (this != &other) {
        id_ = other.id_;
        name_ = std::move(other.name_);
        type_ = other.type_;
        size_bytes_ = other.size_bytes_;
        is_resident_ = other.is_resident_;
        other.is_resident_ = false;
        other.size_bytes_ = 0;
    }
    return *this;
}

AssetCache::AssetCache(size_t max_memory_budget_bytes)
    : max_memory_budget_bytes_(max_memory_budget_bytes) {}

void AssetCache::insert_mesh(std::string key, TriangleMesh mesh) {
    size_t size = mesh.vertices.size() * sizeof(base::math::Vector3<double>) +
                  mesh.normals.size() * sizeof(base::math::Vector3<double>) +
                  mesh.faces.size() * sizeof(MeshFace);

    // If key already existed, subtract old size
    auto it = mesh_sizes_.find(key);
    if (it != mesh_sizes_.end()) {
        current_memory_bytes_ -= it->second;
    }

    current_memory_bytes_ += size;
    mesh_sizes_[key] = size;
    meshes_[key] = std::move(mesh);

    // Update LRU
    auto lru_it = std::find(lru_order_.begin(), lru_order_.end(), key);
    if (lru_it != lru_order_.end()) {
        lru_order_.erase(lru_it);
    }
    lru_order_.push_back(key);

    evict_if_needed();
}

const TriangleMesh* AssetCache::find_mesh(std::string_view key) const noexcept {
    auto it = meshes_.find(std::string(key));
    if (it != meshes_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool AssetCache::remove_mesh(std::string_view key) {
    std::string skey(key);
    auto it = meshes_.find(skey);
    if (it != meshes_.end()) {
        current_memory_bytes_ -= mesh_sizes_[skey];
        mesh_sizes_.erase(skey);
        meshes_.erase(it);

        auto lru_it = std::find(lru_order_.begin(), lru_order_.end(), skey);
        if (lru_it != lru_order_.end()) {
            lru_order_.erase(lru_it);
        }
        return true;
    }
    return false;
}

void AssetCache::purge_unreferenced() {
    // Purge half of the oldest entries if over budget
    evict_if_needed();
}

void AssetCache::evict_if_needed() {
    while (current_memory_bytes_ > max_memory_budget_bytes_ && !lru_order_.empty()) {
        std::string oldest = lru_order_.front();
        lru_order_.erase(lru_order_.begin());
        current_memory_bytes_ -= mesh_sizes_[oldest];
        mesh_sizes_.erase(oldest);
        meshes_.erase(oldest);
    }
}

void AsyncAssetLoader::queue_mesh_load(std::string asset_id, std::function<base::Result<TriangleMesh>()> load_fn) {
    Task task;
    task.asset_id = asset_id;
    task.load_fn = std::move(load_fn);
    task.status = AssetLoadStatus::Queued;
    task.progress = 0.0;
    tasks_[std::move(asset_id)] = std::move(task);
}

void AsyncAssetLoader::update_progress() {
    for (auto& [id, task] : tasks_) {
        if (task.status == AssetLoadStatus::Queued) {
            task.status = AssetLoadStatus::Loading;
            task.progress = 0.5;
        } else if (task.status == AssetLoadStatus::Loading) {
            if (task.load_fn) {
                auto res = task.load_fn();
                if (res.is_ok()) {
                    task.result = res.value();
                    task.status = AssetLoadStatus::Ready;
                    task.progress = 1.0;
                } else {
                    task.status = AssetLoadStatus::Failed;
                    task.error_msg = res.error().message;
                }
            } else {
                task.status = AssetLoadStatus::Failed;
            }
        }
    }
}

AssetLoadStatus AsyncAssetLoader::status(std::string_view asset_id) const noexcept {
    auto it = tasks_.find(std::string(asset_id));
    if (it != tasks_.end()) {
        return it->second.status;
    }
    return AssetLoadStatus::Unloaded;
}

double AsyncAssetLoader::progress(std::string_view asset_id) const noexcept {
    auto it = tasks_.find(std::string(asset_id));
    if (it != tasks_.end()) {
        return it->second.progress;
    }
    return 0.0;
}

const TriangleMesh* AsyncAssetLoader::get_loaded_mesh(std::string_view asset_id) const noexcept {
    auto it = tasks_.find(std::string(asset_id));
    if (it != tasks_.end() && it->second.status == AssetLoadStatus::Ready && it->second.result.has_value()) {
        return &(*it->second.result);
    }
    return nullptr;
}

// =============================================================================
// REN-06: Scientific Visualization Layers (Non-Invasive Overlays)
// =============================================================================

std::array<float, 4> Colormap::map_value(double val, double min_val, double max_val, ColormapType cmap) noexcept {
    double t = 0.0;
    if (std::abs(max_val - min_val) > 1e-9) {
        t = std::clamp((val - min_val) / (max_val - min_val), 0.0, 1.0);
    }

    float tf = static_cast<float>(t);

    switch (cmap) {
        case ColormapType::Grayscale:
            return {tf, tf, tf, 1.0f};

        case ColormapType::Jet: {
            float r = std::clamp(1.5f - std::abs(4.0f * tf - 3.0f), 0.0f, 1.0f);
            float g = std::clamp(1.5f - std::abs(4.0f * tf - 2.0f), 0.0f, 1.0f);
            float b = std::clamp(1.5f - std::abs(4.0f * tf - 1.0f), 0.0f, 1.0f);
            return {r, g, b, 1.0f};
        }

        case ColormapType::CoolWarm: {
            float r = tf;
            float b = 1.0f - tf;
            float g = 1.0f - 2.0f * std::abs(tf - 0.5f);
            return {r, g, b, 1.0f};
        }

        case ColormapType::Plasma: {
            float r = std::clamp(0.05f + 1.2f * tf, 0.0f, 1.0f);
            float g = std::clamp(tf * tf, 0.0f, 1.0f);
            float b = std::clamp(1.0f - tf, 0.0f, 1.0f);
            return {r, g, b, 1.0f};
        }

        case ColormapType::Viridis:
        default: {
            // High quality approximation of Viridis
            float r = std::clamp(0.267f + 0.004f * tf + 0.72f * tf * tf, 0.0f, 1.0f);
            float g = std::clamp(0.003f + 1.25f * tf - 0.35f * tf * tf, 0.0f, 1.0f);
            float b = std::clamp(0.329f + 0.65f * (1.0f - tf) - 0.5f * (1.0f - tf) * (1.0f - tf), 0.0f, 1.0f);
            return {r, g, b, 1.0f};
        }
    }
}

void ScalarFieldContourLayer::set_samples(std::vector<ScalarFieldSample> samples) {
    samples_ = std::move(samples);
}

void ScalarFieldContourLayer::set_value_range(double min_val, double max_val) noexcept {
    min_val_ = min_val;
    max_val_ = max_val;
}

void ScalarFieldContourLayer::auto_fit_range() noexcept {
    if (samples_.empty()) return;
    min_val_ = samples_.front().value;
    max_val_ = samples_.front().value;
    for (const auto& s : samples_) {
        min_val_ = std::min(min_val_, s.value);
        max_val_ = std::max(max_val_, s.value);
    }
}

std::vector<std::array<float, 4>> ScalarFieldContourLayer::generate_sample_colors() const {
    std::vector<std::array<float, 4>> colors;
    colors.reserve(samples_.size());
    for (const auto& s : samples_) {
        colors.push_back(Colormap::map_value(s.value, min_val_, max_val_, colormap_));
    }
    return colors;
}

void VectorFieldStreamlineLayer::set_field(std::vector<VectorFieldSample> samples) {
    samples_ = std::move(samples);
}

std::vector<std::pair<base::math::Vector3<double>, base::math::Vector3<double>>>
VectorFieldStreamlineLayer::generate_arrow_glyphs() const {
    std::vector<std::pair<base::math::Vector3<double>, base::math::Vector3<double>>> arrows;
    arrows.reserve(samples_.size());
    for (const auto& s : samples_) {
        arrows.push_back({s.position, s.position + s.vector * arrow_scale_});
    }
    return arrows;
}

std::vector<std::vector<base::math::Vector3<double>>>
VectorFieldStreamlineLayer::trace_streamlines(
    const std::vector<base::math::Vector3<double>>& seeds,
    double step_size, uint32_t max_steps) const {

    std::vector<std::vector<base::math::Vector3<double>>> streamlines;
    streamlines.reserve(seeds.size());

    // Helper to find nearest field sample velocity
    auto sample_velocity = [this](const base::math::Vector3<double>& pos) -> base::math::Vector3<double> {
        if (samples_.empty()) return {0.0, 0.0, 0.0};
        double min_dist_sq = 1e30;
        base::math::Vector3<double> best_v = samples_.front().vector;
        for (const auto& s : samples_) {
            double d = (s.position - pos).length_squared();
            if (d < min_dist_sq) {
                min_dist_sq = d;
                best_v = s.vector;
            }
        }
        return best_v;
    };

    for (const auto& seed : seeds) {
        std::vector<base::math::Vector3<double>> line;
        line.reserve(max_steps);
        base::math::Vector3<double> curr = seed;
        line.push_back(curr);

        for (uint32_t step = 0; step < max_steps; ++step) {
            auto v = sample_velocity(curr);
            double v_len = v.length();
            if (v_len < 1e-6) break;
            curr += (v / v_len) * step_size;
            line.push_back(curr);
        }
        streamlines.push_back(std::move(line));
    }

    return streamlines;
}

void ProbeMarkerOverlay::add_probe(const ProbeMarker& probe) {
    probes_.push_back(probe);
}

void ProbeMarkerOverlay::update_value(std::string_view probe_id, double val) {
    for (auto& p : probes_) {
        if (p.probe_id == probe_id) {
            p.scalar_value = val;
            return;
        }
    }
}

const ProbeMarker* ProbeMarkerOverlay::find_probe(std::string_view probe_id) const noexcept {
    for (const auto& p : probes_) {
        if (p.probe_id == probe_id) return &p;
    }
    return nullptr;
}

TriangleMesh DeformationMeshOverlay::compute_deformed_mesh(
    const TriangleMesh& base_mesh,
    const std::vector<base::math::Vector3<double>>& nodal_displacements) const {

    TriangleMesh deformed = base_mesh; // copy
    size_t count = std::min(deformed.vertices.size(), nodal_displacements.size());

    for (size_t i = 0; i < count; ++i) {
        deformed.vertices[i] += nodal_displacements[i] * factor_;
    }

    return deformed;
}

// =============================================================================
// REN-07: Render Profiling
// =============================================================================

void RenderProfiler::record_frame(const RenderFrameStats& stats) {
    total_frames_++;
    accumulated_cpu_time_ += stats.cpu_frame_time_ms;
    peak_cpu_time_ = std::max(peak_cpu_time_, stats.cpu_frame_time_ms);
    peak_draw_calls_ = std::max(peak_draw_calls_, stats.draw_calls);
}

double RenderProfiler::average_cpu_frame_time_ms() const noexcept {
    if (total_frames_ == 0) return 0.0;
    return accumulated_cpu_time_ / static_cast<double>(total_frames_);
}

void RenderProfiler::record_shader_compilation(double compile_time_ms, bool cache_hit) {
    if (cache_hit) {
        shader_cache_hits_++;
    } else {
        shader_compiled_count_++;
        accumulated_shader_compile_time_ += compile_time_ms;
    }
}

void RenderProfiler::update_lod_metrics(uint32_t lod0_count, uint32_t lod1_count, uint32_t lod2_count) {
    lod_counts_[0] = lod0_count;
    lod_counts_[1] = lod1_count;
    lod_counts_[2] = lod2_count;
}

std::string RenderProfiler::to_json_report() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n"
       << "  \"total_frames\": " << total_frames_ << ",\n"
       << "  \"avg_cpu_time_ms\": " << average_cpu_frame_time_ms() << ",\n"
       << "  \"peak_cpu_time_ms\": " << peak_cpu_time_ << ",\n"
       << "  \"peak_draw_calls\": " << peak_draw_calls_ << ",\n"
       << "  \"shader_compiled\": " << shader_compiled_count_ << ",\n"
       << "  \"shader_cache_hits\": " << shader_cache_hits_ << ",\n"
       << "  \"lod_counts\": [" << lod_counts_[0] << ", " << lod_counts_[1] << ", " << lod_counts_[2] << "]\n"
       << "}";
    return ss.str();
}

// =============================================================================
// REN-08: Headless Software Mode & Framebuffer
// =============================================================================

HeadlessFramebuffer::HeadlessFramebuffer(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
    color_buffer_.resize(width * height, 0xFF101015);
    depth_buffer_.resize(width * height, 1.0f);
}

void HeadlessFramebuffer::resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    color_buffer_.assign(width * height, 0xFF101015);
    depth_buffer_.assign(width * height, 1.0f);
}

void HeadlessFramebuffer::clear(uint32_t argb_color, float depth) {
    std::fill(color_buffer_.begin(), color_buffer_.end(), argb_color);
    std::fill(depth_buffer_.begin(), depth_buffer_.end(), depth);
}

void HeadlessFramebuffer::set_pixel(uint32_t x, uint32_t y, uint32_t argb_color, float depth) {
    if (x >= width_ || y >= height_) return;
    size_t idx = y * width_ + x;
    if (depth <= depth_buffer_[idx]) {
        depth_buffer_[idx] = depth;
        color_buffer_[idx] = argb_color;
    }
}

uint32_t HeadlessFramebuffer::get_pixel(uint32_t x, uint32_t y) const noexcept {
    if (x >= width_ || y >= height_) return 0;
    return color_buffer_[y * width_ + x];
}

float HeadlessFramebuffer::get_depth(uint32_t x, uint32_t y) const noexcept {
    if (x >= width_ || y >= height_) return 1.0f;
    return depth_buffer_[y * width_ + x];
}

void HeadlessFramebuffer::draw_line(int x0, int y0, int x1, int y1, uint32_t argb_color) {
    // Bresenham's line algorithm
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        if (x0 >= 0 && x0 < static_cast<int>(width_) && y0 >= 0 && y0 < static_cast<int>(height_)) {
            set_pixel(static_cast<uint32_t>(x0), static_cast<uint32_t>(y0), argb_color, 0.0f);
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void HeadlessFramebuffer::draw_filled_rect(int x, int y, int w, int h, uint32_t argb_color) {
    int x_end = std::min(x + w, static_cast<int>(width_));
    int y_end = std::min(y + h, static_cast<int>(height_));
    int x_start = std::max(0, x);
    int y_start = std::max(0, y);

    for (int py = y_start; py < y_end; ++py) {
        for (int px = x_start; px < x_end; ++px) {
            set_pixel(static_cast<uint32_t>(px), static_cast<uint32_t>(py), argb_color, 0.0f);
        }
    }
}

uint64_t HeadlessFramebuffer::compute_buffer_checksum() const noexcept {
    // 64-bit FNV-1a hash over color buffer
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t pixel : color_buffer_) {
        for (int i = 0; i < 4; ++i) {
            uint8_t byte = static_cast<uint8_t>((pixel >> (i * 8)) & 0xFF);
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

std::string HeadlessFramebuffer::export_ppm_ascii() const {
    std::ostringstream ss;
    ss << "P3\n" << width_ << " " << height_ << "\n255\n";
    for (uint32_t pixel : color_buffer_) {
        uint32_t r = (pixel >> 16) & 0xFF;
        uint32_t g = (pixel >> 8) & 0xFF;
        uint32_t b = pixel & 0xFF;
        ss << r << " " << g << " " << b << "\n";
    }
    return ss.str();
}

HeadlessSoftwareBackend::HeadlessSoftwareBackend()
    : framebuffer_(800, 600) {}

HeadlessSoftwareBackend::HeadlessSoftwareBackend(uint32_t width, uint32_t height)
    : framebuffer_(width, height) {}

base::Result<void> HeadlessSoftwareBackend::initialize(const RenderBackendConfig& config) {
    framebuffer_.resize(config.initial_width, config.initial_height);
    initialized_ = true;
    stats_.reset();
    return base::Result<void>::ok();
}

void HeadlessSoftwareBackend::shutdown() {
    initialized_ = false;
}

base::Result<void> HeadlessSoftwareBackend::begin_frame() {
    if (!initialized_) {
        return base::Result<void>::err(make_render_error("Headless backend not initialized"));
    }
    stats_.reset();
    return base::Result<void>::ok();
}

base::Result<void> HeadlessSoftwareBackend::end_frame() {
    if (!initialized_) {
        return base::Result<void>::err(make_render_error("Headless backend not initialized"));
    }
    return base::Result<void>::ok();
}

base::Result<void> HeadlessSoftwareBackend::execute_pass(const RenderPassDescriptor& pass, const SceneSubmissionQueue& queue) {
    if (!initialized_) {
        return base::Result<void>::err(make_render_error("Headless backend not initialized"));
    }

    uint32_t r = static_cast<uint32_t>(pass.clear_color[0] * 255.0f);
    uint32_t g = static_cast<uint32_t>(pass.clear_color[1] * 255.0f);
    uint32_t b = static_cast<uint32_t>(pass.clear_color[2] * 255.0f);
    uint32_t a = static_cast<uint32_t>(pass.clear_color[3] * 255.0f);
    uint32_t clear_argb = (a << 24) | (r << 16) | (g << 8) | b;

    framebuffer_.clear(clear_argb, pass.clear_depth_value);

    for (const auto& inst : queue.instances()) {
        if (!inst.visible) continue;
        stats_.draw_calls++;
        stats_.instances_rendered++;

        // Render mock representation: simple colored rectangle in framebuffer
        auto [w_min, w_max] = inst.world_bounds();
        int px = static_cast<int>((w_min.x + 5.0) * 40.0);
        int py = static_cast<int>((w_min.y + 5.0) * 40.0);
        framebuffer_.draw_filled_rect(px, py, 20, 20, 0xFF4080FF);
    }

    return base::Result<void>::ok();
}

RenderCapabilities HeadlessSoftwareBackend::capabilities() const noexcept {
    RenderCapabilities caps;
    caps.gpu_vendor = "Software";
    caps.renderer_name = "Shunya Software Rasterizer";
    caps.api_version = "1.0";
    caps.is_hardware_accelerated = false;
    caps.supports_compute_shaders = false;
    caps.supports_ray_tracing = false;
    caps.supports_mesh_shaders = false;
    caps.supports_multisampling = true;
    return caps;
}

// =============================================================================
// REN-09: Renderer Capability Detection & Fallback Path
// =============================================================================

MockHardwareRendererBackend::MockHardwareRendererBackend(RendererBackendType type, bool should_fail_init)
    : type_(type), should_fail_init_(should_fail_init) {}

std::string_view MockHardwareRendererBackend::backend_name() const noexcept {
    switch (type_) {
        case RendererBackendType::Vulkan: return "Mock Vulkan Hardware Backend";
        case RendererBackendType::OpenGL: return "Mock OpenGL Hardware Backend";
        case RendererBackendType::Direct3D12: return "Mock Direct3D12 Hardware Backend";
        default: return "Mock Hardware Backend";
    }
}

base::Result<void> MockHardwareRendererBackend::initialize(const RenderBackendConfig&) {
    if (should_fail_init_) {
        return base::Result<void>::err(make_render_error("Hardware device initialization failed: GPU not found", 4002));
    }
    initialized_ = true;
    stats_.reset();
    return base::Result<void>::ok();
}

void MockHardwareRendererBackend::shutdown() {
    initialized_ = false;
}

base::Result<void> MockHardwareRendererBackend::begin_frame() {
    if (!initialized_) return base::Result<void>::err(make_render_error("Backend not initialized"));
    stats_.reset();
    return base::Result<void>::ok();
}

base::Result<void> MockHardwareRendererBackend::end_frame() {
    if (!initialized_) return base::Result<void>::err(make_render_error("Backend not initialized"));
    return base::Result<void>::ok();
}

base::Result<void> MockHardwareRendererBackend::execute_pass(const RenderPassDescriptor&, const SceneSubmissionQueue& queue) {
    if (!initialized_) return base::Result<void>::err(make_render_error("Backend not initialized"));
    stats_.draw_calls += static_cast<uint32_t>(queue.instance_count());
    stats_.instances_rendered += static_cast<uint32_t>(queue.instance_count());
    return base::Result<void>::ok();
}

RenderCapabilities MockHardwareRendererBackend::capabilities() const noexcept {
    RenderCapabilities caps;
    caps.gpu_vendor = "Hardware Vendor";
    caps.renderer_name = std::string(backend_name());
    caps.api_version = "2.0";
    caps.is_hardware_accelerated = true;
    caps.supports_compute_shaders = true;
    caps.supports_ray_tracing = (type_ == RendererBackendType::Vulkan);
    caps.supports_mesh_shaders = (type_ == RendererBackendType::Vulkan);
    caps.supports_multisampling = true;
    return caps;
}

RenderDeviceManager::RenderDeviceManager(RenderFallbackPolicy policy)
    : policy_(policy) {}

base::Result<std::unique_ptr<IRendererBackend>> RenderDeviceManager::create_backend(
    RendererBackendType preferred_type,
    const RenderBackendConfig& config) {

    used_fallback_ = false;
    fallback_reason_.clear();

    std::unique_ptr<IRendererBackend> candidate;

    if (preferred_type == RendererBackendType::Null) {
        candidate = std::make_unique<NullRendererBackend>();
    } else if (preferred_type == RendererBackendType::HeadlessSoftware) {
        candidate = std::make_unique<HeadlessSoftwareBackend>();
    } else {
        // Hardware candidates
        candidate = std::make_unique<MockHardwareRendererBackend>(preferred_type);
    }

    auto init_res = candidate->initialize(config);
    if (init_res.is_ok()) {
        return base::Result<std::unique_ptr<IRendererBackend>>::ok(std::move(candidate));
    }

    // Initialization failed! Check fallback policy
    if (policy_ == RenderFallbackPolicy::FailIfUnavailable) {
        return base::Result<std::unique_ptr<IRendererBackend>>::err(init_res.error());
    }

    // Fall back to software headless
    used_fallback_ = true;
    fallback_reason_ = "Primary backend (" + std::string(backend_type_to_string(preferred_type)) +
                       ") failed: " + init_res.error().message + "; falling back to HeadlessSoftware.";

    auto fallback_backend = std::make_unique<HeadlessSoftwareBackend>();
    auto fb_res = fallback_backend->initialize(config);
    if (fb_res.is_ok()) {
        return base::Result<std::unique_ptr<IRendererBackend>>::ok(std::move(fallback_backend));
    }

    return base::Result<std::unique_ptr<IRendererBackend>>::err(fb_res.error());
}

// =============================================================================
// REN-10: Independent Rendering Test Suite & Validation
// =============================================================================

double RenderTestHarness::compute_framebuffer_mse(const HeadlessFramebuffer& a, const HeadlessFramebuffer& b) {
    if (a.width() != b.width() || a.height() != b.height() || a.color_buffer().empty()) {
        return std::numeric_limits<double>::infinity();
    }

    double total_sq_err = 0.0;
    size_t count = a.color_buffer().size();

    for (size_t i = 0; i < count; ++i) {
        uint32_t pa = a.color_buffer()[i];
        uint32_t pb = b.color_buffer()[i];

        int ra = (pa >> 16) & 0xFF, rb = (pb >> 16) & 0xFF;
        int ga = (pa >> 8) & 0xFF,  gb = (pb >> 8) & 0xFF;
        int ba = pa & 0xFF,         bb = pb & 0xFF;

        double dr = ra - rb;
        double dg = ga - gb;
        double db = ba - bb;
        total_sq_err += (dr * dr + dg * dg + db * db) / 3.0;
    }

    return total_sq_err / static_cast<double>(count);
}

bool RenderTestHarness::verify_frustum_culling_invariance(
    const SceneSubmissionQueue& queue,
    const ViewportCamera& camera) {

    uint32_t culled = 0;
    auto visible = queue.cull_and_sort(camera, culled);

    // Visible + culled must equal total submitted instances
    return (visible.size() + culled) == queue.instance_count();
}

bool RenderTestHarness::verify_physics_state_decoupling(
    const TriangleMesh& physics_mesh,
    const DeformationMeshOverlay& overlay,
    const std::vector<base::math::Vector3<double>>& displacements) {

    // Store original coordinates
    std::vector<base::math::Vector3<double>> original_vertices = physics_mesh.vertices;

    // Run visualization overlay deformation
    TriangleMesh deformed = overlay.compute_deformed_mesh(physics_mesh, displacements);

    // Verify deformed mesh has changed (if displacements non-zero)
    bool has_displaced = false;
    for (size_t i = 0; i < std::min(deformed.vertices.size(), displacements.size()); ++i) {
        if ((deformed.vertices[i] - original_vertices[i]).length_squared() > 1e-9) {
            has_displaced = true;
            break;
        }
    }

    // Verify physics_mesh was NOT modified in-place
    bool physics_unchanged = (physics_mesh.vertices == original_vertices);

    return has_displaced && physics_unchanged;
}

} // namespace shunya::core::domain
