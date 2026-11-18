#include "shunya/rig3d/skinning.hpp"
#include "shunya/rig3d/sculpt_brush.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::rig3d {

static Point3D transform_point(const Mat4& m, const Point3D& p) {
    return Point3D(
        m.m[0] * p.x + m.m[1] * p.y + m.m[2] * p.z + m.m[3],
        m.m[4] * p.x + m.m[5] * p.y + m.m[6] * p.z + m.m[7],
        m.m[8] * p.x + m.m[9] * p.y + m.m[10] * p.z + m.m[11]
    );
}

static double distance_point_to_segment(const Point3D& p, const Point3D& a, const Point3D& b) {
    const Vec3d ab = b - a;
    const double len_sq = ab.length_squared();
    if (len_sq < 1e-12) {
        return (p - a).length();
    }
    const double t = std::clamp((p - a).dot(ab) / len_sq, 0.0, 1.0);
    const Point3D proj = a + ab * t;
    return (p - proj).length();
}

void SkinningManager::compute_distance_weights(
    const SolidMesh3D& mesh,
    const Armature& armature,
    double falloff_power
) {
    const size_t vcount = mesh.vertices.size();
    vertex_data_.resize(vcount);
    if (armature.empty() || vcount == 0) return;

    const size_t bcount = armature.bone_count();

    for (size_t i = 0; i < vcount; ++i) {
        const Point3D& v = mesh.vertices[i];
        vertex_data_[i].weights.clear();

        std::vector<std::pair<double, uint32_t>> dists;
        dists.reserve(bcount);

        for (uint32_t b = 0; b < bcount; ++b) {
            const auto& bone = armature.bone(b);
            const double d = distance_point_to_segment(v, bone.head, bone.tail);
            dists.push_back({d, b});
        }

        std::sort(dists.begin(), dists.end());

        const size_t keep = std::min(dists.size(), size_t{4});
        for (size_t k = 0; k < keep; ++k) {
            const double dist = dists[k].first;
            const uint32_t b_idx = dists[k].second;
            const double raw_w = 1.0 / std::pow(dist + 0.05, falloff_power);
            vertex_data_[i].weights.push_back({b_idx, raw_w});
        }

        vertex_data_[i].normalize();
    }
}

SolidMesh3D SkinningManager::evaluate_skinning(const SolidMesh3D& rest_mesh, const Armature& armature) const {
    SolidMesh3D skinned = rest_mesh;
    const size_t vcount = rest_mesh.vertices.size();
    if (vcount == 0 || armature.empty()) return skinned;

    const size_t bcount = armature.bone_count();
    std::vector<Mat4> skin_matrices(bcount);
    for (uint32_t b = 0; b < bcount; ++b) {
        skin_matrices[b] = armature.get_skinning_matrix(b);
    }

    for (size_t i = 0; i < vcount; ++i) {
        if (i >= vertex_data_.size() || vertex_data_[i].weights.empty()) {
            continue;
        }

        const Point3D& p_rest = rest_mesh.vertices[i];
        Point3D p_skinned(0.0, 0.0, 0.0);

        for (const auto& vw : vertex_data_[i].weights) {
            if (vw.bone_index < bcount && vw.weight > 1e-6) {
                const Point3D trans = transform_point(skin_matrices[vw.bone_index], p_rest);
                p_skinned = p_skinned + trans * vw.weight;
            }
        }

        skinned.vertices[i] = p_skinned;
    }

    skinned.normals.assign(vcount, Vec3d(0, 0, 0));
    for (const auto& tri : skinned.triangles) {
        if (tri[0] >= vcount || tri[1] >= vcount || tri[2] >= vcount) continue;
        const auto& p0 = skinned.vertices[tri[0]];
        const auto& p1 = skinned.vertices[tri[1]];
        const auto& p2 = skinned.vertices[tri[2]];
        const Vec3d fn = (p1 - p0).cross(p2 - p0);
        skinned.normals[tri[0]] += fn;
        skinned.normals[tri[1]] += fn;
        skinned.normals[tri[2]] += fn;
    }
    for (auto& n : skinned.normals) {
        if (n.length_squared() > 1e-12) n = n.normalized();
        else n = Vec3d(0, 0, 1);
    }

    return skinned;
}

void SkinningManager::paint_weight(
    const SolidMesh3D& mesh,
    uint32_t target_bone,
    const Point3D& stroke_pos,
    double radius,
    double strength,
    bool subtract
) {
    if (mesh.vertices.empty() || radius <= 0.0 || strength <= 0.0) return;
    if (vertex_data_.size() != mesh.vertices.size()) {
        vertex_data_.resize(mesh.vertices.size());
    }

    const double delta_sign = subtract ? -1.0 : 1.0;

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const double dist = (mesh.vertices[i] - stroke_pos).length();
        if (dist > radius) continue;

        const double falloff = SculptEngine::compute_falloff(dist, radius);
        const double delta = falloff * strength * delta_sign;

        double cur_w = vertex_data_[i].get_bone_weight(target_bone);
        cur_w = std::clamp(cur_w + delta, 0.0, 1.0);
        vertex_data_[i].set_bone_weight(target_bone, cur_w);
        vertex_data_[i].normalize();
    }
}

Vec3d SkinningManager::weight_to_color(double weight) noexcept {
    const double w = std::clamp(weight, 0.0, 1.0);
    if (w < 0.25) {
        const double t = w / 0.25;
        return Vec3d(0.0, t, 1.0); // Blue to Cyan
    } else if (w < 0.50) {
        const double t = (w - 0.25) / 0.25;
        return Vec3d(0.0, 1.0, 1.0 - t); // Cyan to Green
    } else if (w < 0.75) {
        const double t = (w - 0.50) / 0.25;
        return Vec3d(t, 1.0, 0.0); // Green to Yellow
    } else {
        const double t = (w - 0.75) / 0.25;
        return Vec3d(1.0, 1.0 - t, 0.0); // Yellow to Red
    }
}

} // namespace shunya::rig3d
