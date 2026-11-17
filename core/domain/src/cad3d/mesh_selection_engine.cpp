#include "shunya/cad3d/mesh_selection_engine.hpp"

#include <cmath>

namespace shunya::cad3d {

namespace {

inline std::pair<uint32_t, uint32_t> normalize_edge(uint32_t u, uint32_t v) noexcept {
    return (u < v) ? std::make_pair(u, v) : std::make_pair(v, u);
}

double dist_point_to_segment(const Point3D& p, const Point3D& a, const Point3D& b) noexcept {
    Vec3d ab = b - a;
    double len_sq = ab.length_squared();
    if (len_sq < 1e-12) {
        return (p - a).length();
    }
    double t = std::clamp((p - a).dot(ab) / len_sq, 0.0, 1.0);
    Point3D proj = a + ab * t;
    return (p - proj).length();
}

} // namespace

void MeshSelection::clear() noexcept {
    object_selected = false;
    selected_vertices.clear();
    selected_edges.clear();
    selected_faces.clear();
}

bool MeshSelection::empty() const noexcept {
    return !object_selected && selected_vertices.empty() && selected_edges.empty() && selected_faces.empty();
}

void MeshSelection::select_vertex(uint32_t v_idx, bool toggle) {
    auto it = std::find(selected_vertices.begin(), selected_vertices.end(), v_idx);
    if (it != selected_vertices.end()) {
        if (toggle) {
            selected_vertices.erase(it);
        }
    } else {
        selected_vertices.push_back(v_idx);
    }
}

void MeshSelection::select_edge(uint32_t u, uint32_t v, bool toggle) {
    auto edge = normalize_edge(u, v);
    auto it = std::find(selected_edges.begin(), selected_edges.end(), edge);
    if (it != selected_edges.end()) {
        if (toggle) {
            selected_edges.erase(it);
        }
    } else {
        selected_edges.push_back(edge);
    }
}

void MeshSelection::select_face(uint32_t f_idx, bool toggle) {
    auto it = std::find(selected_faces.begin(), selected_faces.end(), f_idx);
    if (it != selected_faces.end()) {
        if (toggle) {
            selected_faces.erase(it);
        }
    } else {
        selected_faces.push_back(f_idx);
    }
}

void MeshSelection::select_all(const SolidMesh3D& mesh) {
    clear();
    switch (mode) {
        case MeshSelectionMode::Object:
            object_selected = true;
            break;
        case MeshSelectionMode::Vertex:
            selected_vertices.resize(mesh.vertices.size());
            for (uint32_t i = 0; i < mesh.vertices.size(); ++i) {
                selected_vertices[i] = i;
            }
            break;
        case MeshSelectionMode::Edge: {
            for (const auto& tri : mesh.triangles) {
                for (int i = 0; i < 3; ++i) {
                    auto e = normalize_edge(tri[i], tri[(i + 1) % 3]);
                    if (std::find(selected_edges.begin(), selected_edges.end(), e) == selected_edges.end()) {
                        selected_edges.push_back(e);
                    }
                }
            }
            break;
        }
        case MeshSelectionMode::Face:
            selected_faces.resize(mesh.triangles.size());
            for (uint32_t i = 0; i < mesh.triangles.size(); ++i) {
                selected_faces[i] = i;
            }
            break;
    }
}

bool MeshSelection::is_vertex_selected(uint32_t v_idx) const noexcept {
    return std::find(selected_vertices.begin(), selected_vertices.end(), v_idx) != selected_vertices.end();
}

bool MeshSelection::is_edge_selected(uint32_t u, uint32_t v) const noexcept {
    auto e = normalize_edge(u, v);
    return std::find(selected_edges.begin(), selected_edges.end(), e) != selected_edges.end();
}

bool MeshSelection::is_face_selected(uint32_t f_idx) const noexcept {
    return std::find(selected_faces.begin(), selected_faces.end(), f_idx) != selected_faces.end();
}

PickResult MeshSelectionEngine::pick(
    const SolidMesh3D& mesh,
    const Ray3D& ray,
    MeshSelectionMode mode,
    double max_distance
) {
    PickResult res;
    res.mode = mode;
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        return res;
    }

    double closest_t = max_distance;
    uint32_t best_tri_idx = 0;
    bool found_hit = false;

    for (uint32_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& tri = mesh.triangles[i];
        if (tri[0] >= mesh.vertices.size() || tri[1] >= mesh.vertices.size() || tri[2] >= mesh.vertices.size()) {
            continue;
        }
        const auto& v0 = mesh.vertices[tri[0]];
        const auto& v1 = mesh.vertices[tri[1]];
        const auto& v2 = mesh.vertices[tri[2]];

        double t = 0.0, u = 0.0, v = 0.0;
        if (intersect_ray_triangle_mt(ray, v0, v1, v2, closest_t, t, u, v)) {
            if (t > 0.0 && t < closest_t) {
                closest_t = t;
                best_tri_idx = i;
                found_hit = true;
            }
        }
    }

    if (!found_hit) {
        return res;
    }

    res.hit = true;
    res.distance = closest_t;
    res.hit_point = ray.origin + ray.direction * closest_t;

    const auto& best_tri = mesh.triangles[best_tri_idx];
    const auto& p0 = mesh.vertices[best_tri[0]];
    const auto& p1 = mesh.vertices[best_tri[1]];
    const auto& p2 = mesh.vertices[best_tri[2]];

    switch (mode) {
        case MeshSelectionMode::Object:
            res.element_index = 0;
            break;

        case MeshSelectionMode::Face:
            res.element_index = best_tri_idx;
            break;

        case MeshSelectionMode::Vertex: {
            double d0 = (res.hit_point - p0).length_squared();
            double d1 = (res.hit_point - p1).length_squared();
            double d2 = (res.hit_point - p2).length_squared();

            if (d0 <= d1 && d0 <= d2) {
                res.element_index = best_tri[0];
            } else if (d1 <= d0 && d1 <= d2) {
                res.element_index = best_tri[1];
            } else {
                res.element_index = best_tri[2];
            }
            break;
        }

        case MeshSelectionMode::Edge: {
            double de0 = dist_point_to_segment(res.hit_point, p0, p1);
            double de1 = dist_point_to_segment(res.hit_point, p1, p2);
            double de2 = dist_point_to_segment(res.hit_point, p2, p0);

            if (de0 <= de1 && de0 <= de2) {
                res.edge = normalize_edge(best_tri[0], best_tri[1]);
            } else if (de1 <= de0 && de1 <= de2) {
                res.edge = normalize_edge(best_tri[1], best_tri[2]);
            } else {
                res.edge = normalize_edge(best_tri[2], best_tri[0]);
            }
            break;
        }
    }

    return res;
}

bool MeshSelectionEngine::pick_and_select(
    const SolidMesh3D& mesh,
    const Ray3D& ray,
    MeshSelection& selection,
    bool toggle,
    double max_distance
) {
    auto res = pick(mesh, ray, selection.mode, max_distance);
    if (!res.hit) {
        if (!toggle) {
            selection.clear();
        }
        return false;
    }

    if (!toggle) {
        selection.clear();
    }

    switch (selection.mode) {
        case MeshSelectionMode::Object:
            selection.select_object(true);
            break;
        case MeshSelectionMode::Face:
            selection.select_face(res.element_index, toggle);
            break;
        case MeshSelectionMode::Vertex:
            selection.select_vertex(res.element_index, toggle);
            break;
        case MeshSelectionMode::Edge:
            selection.select_edge(res.edge.first, res.edge.second, toggle);
            break;
    }

    return true;
}

void MeshSelectionEngine::select_in_box(
    const SolidMesh3D& mesh,
    const BBox3D& box,
    MeshSelection& selection,
    bool additive
) {
    if (!additive) {
        selection.clear();
    }

    auto is_inside = [&](const Point3D& p) noexcept {
        return p.x >= box.min.x && p.x <= box.max.x &&
               p.y >= box.min.y && p.y <= box.max.y &&
               p.z >= box.min.z && p.z <= box.max.z;
    };

    switch (selection.mode) {
        case MeshSelectionMode::Object: {
            auto mesh_box = mesh.compute_bbox();
            if (box.is_valid() && mesh_box.is_valid()) {
                bool overlap = !(box.max.x < mesh_box.min.x || box.min.x > mesh_box.max.x ||
                                 box.max.y < mesh_box.min.y || box.min.y > mesh_box.max.y ||
                                 box.max.z < mesh_box.min.z || box.min.z > mesh_box.max.z);
                if (overlap) {
                    selection.select_object(true);
                }
            }
            break;
        }

        case MeshSelectionMode::Vertex: {
            for (uint32_t i = 0; i < mesh.vertices.size(); ++i) {
                if (is_inside(mesh.vertices[i])) {
                    selection.select_vertex(i, false);
                }
            }
            break;
        }

        case MeshSelectionMode::Edge: {
            for (const auto& tri : mesh.triangles) {
                for (int i = 0; i < 3; ++i) {
                    uint32_t u = tri[i];
                    uint32_t v = tri[(i + 1) % 3];
                    if (is_inside(mesh.vertices[u]) && is_inside(mesh.vertices[v])) {
                        selection.select_edge(u, v, false);
                    }
                }
            }
            break;
        }

        case MeshSelectionMode::Face: {
            for (uint32_t i = 0; i < mesh.triangles.size(); ++i) {
                const auto& tri = mesh.triangles[i];
                if (is_inside(mesh.vertices[tri[0]]) &&
                    is_inside(mesh.vertices[tri[1]]) &&
                    is_inside(mesh.vertices[tri[2]])) {
                    selection.select_face(i, false);
                }
            }
            break;
        }
    }
}

} // namespace shunya::cad3d
