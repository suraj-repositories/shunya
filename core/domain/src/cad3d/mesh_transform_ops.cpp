#include "shunya/cad3d/mesh_transform_ops.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace shunya::cad3d {

namespace {

std::vector<uint32_t> get_affected_vertices(
    const SolidMesh3D& mesh,
    const MeshSelection& selection
) {
    if (selection.object_selected || selection.mode == MeshSelectionMode::Object) {
        std::vector<uint32_t> all_indices(mesh.vertices.size());
        for (uint32_t i = 0; i < mesh.vertices.size(); ++i) {
            all_indices[i] = i;
        }
        return all_indices;
    }

    std::unordered_set<uint32_t> unique_verts;
    switch (selection.mode) {
        case MeshSelectionMode::Vertex:
            for (uint32_t v : selection.selected_vertices) {
                if (v < mesh.vertices.size()) unique_verts.insert(v);
            }
            break;

        case MeshSelectionMode::Edge:
            for (const auto& e : selection.selected_edges) {
                if (e.first < mesh.vertices.size()) unique_verts.insert(e.first);
                if (e.second < mesh.vertices.size()) unique_verts.insert(e.second);
            }
            break;

        case MeshSelectionMode::Face:
            for (uint32_t f : selection.selected_faces) {
                if (f < mesh.triangles.size()) {
                    unique_verts.insert(mesh.triangles[f][0]);
                    unique_verts.insert(mesh.triangles[f][1]);
                    unique_verts.insert(mesh.triangles[f][2]);
                }
            }
            break;

        case MeshSelectionMode::Object:
            break;
    }

    std::vector<uint32_t> result(unique_verts.begin(), unique_verts.end());
    std::sort(result.begin(), result.end());
    return result;
}

Vec3d rodrigues_rotate(const Vec3d& v, const Vec3d& axis, double angle) noexcept {
    double cos_th = std::cos(angle);
    double sin_th = std::sin(angle);
    return v * cos_th + axis.cross(v) * sin_th + axis * (axis.dot(v)) * (1.0 - cos_th);
}

} // namespace

Point3D MeshTransformOps::compute_pivot(
    const SolidMesh3D& mesh,
    const MeshSelection& selection,
    TransformPivot pivot_mode,
    const Point3D& custom_pivot
) {
    if (pivot_mode == TransformPivot::WorldOrigin) {
        return Point3D{0.0, 0.0, 0.0};
    }
    if (pivot_mode == TransformPivot::ActiveElement) {
        return custom_pivot;
    }

    auto affected = get_affected_vertices(mesh, selection);
    if (affected.empty()) {
        return Point3D{0.0, 0.0, 0.0};
    }

    if (pivot_mode == TransformPivot::BoundingBoxCenter) {
        BBox3D box;
        for (uint32_t idx : affected) {
            box.expand(mesh.vertices[idx]);
        }
        return (box.min + box.max) * 0.5;
    }

    // Default: MedianPoint
    Point3D sum{0.0, 0.0, 0.0};
    for (uint32_t idx : affected) {
        sum = sum + mesh.vertices[idx];
    }
    return sum * (1.0 / static_cast<double>(affected.size()));
}

Vec3d MeshTransformOps::apply_axis_lock(
    const Vec3d& v,
    TransformAxisLock axis_lock
) noexcept {
    switch (axis_lock) {
        case TransformAxisLock::None:
            return v;
        case TransformAxisLock::X:
            return Vec3d{v.x, 0.0, 0.0};
        case TransformAxisLock::Y:
            return Vec3d{0.0, v.y, 0.0};
        case TransformAxisLock::Z:
            return Vec3d{0.0, 0.0, v.z};
        case TransformAxisLock::PlaneXY:
            return Vec3d{v.x, v.y, 0.0};
        case TransformAxisLock::PlaneXZ:
            return Vec3d{v.x, 0.0, v.z};
        case TransformAxisLock::PlaneYZ:
            return Vec3d{0.0, v.y, v.z};
    }
    return v;
}

void MeshTransformOps::translate(
    SolidMesh3D& mesh,
    const MeshSelection& selection,
    const Vec3d& delta,
    TransformAxisLock axis_lock
) {
    Vec3d locked_delta = apply_axis_lock(delta, axis_lock);
    auto affected = get_affected_vertices(mesh, selection);

    for (uint32_t idx : affected) {
        mesh.vertices[idx] = mesh.vertices[idx] + locked_delta;
    }
}

void MeshTransformOps::rotate(
    SolidMesh3D& mesh,
    const MeshSelection& selection,
    double angle_radians,
    TransformAxisLock axis_lock,
    const Vec3d& custom_axis,
    TransformPivot pivot_mode,
    const Point3D& custom_pivot
) {
    Vec3d rot_axis{0.0, 0.0, 1.0};
    switch (axis_lock) {
        case TransformAxisLock::X:
            rot_axis = Vec3d{1.0, 0.0, 0.0};
            break;
        case TransformAxisLock::Y:
            rot_axis = Vec3d{0.0, 1.0, 0.0};
            break;
        case TransformAxisLock::Z:
            rot_axis = Vec3d{0.0, 0.0, 1.0};
            break;
        case TransformAxisLock::None:
        default:
            rot_axis = custom_axis.normalized();
            if (rot_axis.length_squared() < 1e-6) {
                rot_axis = Vec3d{0.0, 0.0, 1.0};
            }
            break;
    }

    Point3D pivot = compute_pivot(mesh, selection, pivot_mode, custom_pivot);
    auto affected = get_affected_vertices(mesh, selection);

    for (uint32_t idx : affected) {
        Vec3d local_p = mesh.vertices[idx] - pivot;
        mesh.vertices[idx] = pivot + rodrigues_rotate(local_p, rot_axis, angle_radians);

        if (idx < mesh.normals.size()) {
            mesh.normals[idx] = rodrigues_rotate(mesh.normals[idx], rot_axis, angle_radians).normalized();
        }
    }
}

void MeshTransformOps::scale(
    SolidMesh3D& mesh,
    const MeshSelection& selection,
    const Vec3d& scale_factors,
    TransformAxisLock axis_lock,
    TransformPivot pivot_mode,
    const Point3D& custom_pivot
) {
    Vec3d locked_scale{1.0, 1.0, 1.0};
    switch (axis_lock) {
        case TransformAxisLock::None:
            locked_scale = scale_factors;
            break;
        case TransformAxisLock::X:
            locked_scale = Vec3d{scale_factors.x, 1.0, 1.0};
            break;
        case TransformAxisLock::Y:
            locked_scale = Vec3d{1.0, scale_factors.y, 1.0};
            break;
        case TransformAxisLock::Z:
            locked_scale = Vec3d{1.0, 1.0, scale_factors.z};
            break;
        case TransformAxisLock::PlaneXY:
            locked_scale = Vec3d{scale_factors.x, scale_factors.y, 1.0};
            break;
        case TransformAxisLock::PlaneXZ:
            locked_scale = Vec3d{scale_factors.x, 1.0, scale_factors.z};
            break;
        case TransformAxisLock::PlaneYZ:
            locked_scale = Vec3d{1.0, scale_factors.y, scale_factors.z};
            break;
    }

    Point3D pivot = compute_pivot(mesh, selection, pivot_mode, custom_pivot);
    auto affected = get_affected_vertices(mesh, selection);

    for (uint32_t idx : affected) {
        Vec3d local_p = mesh.vertices[idx] - pivot;
        mesh.vertices[idx] = Point3D{
            pivot.x + local_p.x * locked_scale.x,
            pivot.y + local_p.y * locked_scale.y,
            pivot.z + local_p.z * locked_scale.z
        };
    }
}

} // namespace shunya::cad3d
