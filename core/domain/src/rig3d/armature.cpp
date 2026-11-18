#include "shunya/rig3d/armature.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::rig3d {

static Mat4 make_basis_matrix(const Vec3d& x, const Vec3d& y, const Vec3d& z, const Point3D& origin) {
    Mat4 res = Mat4::identity();
    res.m[0] = x.x; res.m[1] = y.x; res.m[2] = z.x; res.m[3] = origin.x;
    res.m[4] = x.y; res.m[5] = y.y; res.m[6] = z.y; res.m[7] = origin.y;
    res.m[8] = x.z; res.m[9] = y.z; res.m[10] = z.z; res.m[11] = origin.z;
    res.m[12] = 0.0; res.m[13] = 0.0; res.m[14] = 0.0; res.m[15] = 1.0;
    return res;
}

static Point3D transform_point(const Mat4& m, const Point3D& p) {
    return Point3D(
        m.m[0] * p.x + m.m[1] * p.y + m.m[2] * p.z + m.m[3],
        m.m[4] * p.x + m.m[5] * p.y + m.m[6] * p.z + m.m[7],
        m.m[8] * p.x + m.m[9] * p.y + m.m[10] * p.z + m.m[11]
    );
}

uint32_t Armature::add_bone(
    std::string name,
    Point3D head,
    Point3D tail,
    int32_t parent_index,
    bool connected,
    double roll_rad
) {
    const uint32_t new_idx = static_cast<uint32_t>(bones_.size());

    Bone b;
    b.id = new_idx;
    b.name = std::move(name);
    b.head = head;
    b.tail = tail;
    b.roll_rad = roll_rad;
    b.parent_index = parent_index;
    b.connected = connected;

    if (parent_index >= 0 && static_cast<size_t>(parent_index) < bones_.size()) {
        bones_[parent_index].children_indices.push_back(new_idx);
        if (connected) {
            b.head = bones_[parent_index].tail;
        }
    }

    bones_.push_back(b);
    poses_.push_back(BonePose{});
    return new_idx;
}

bool Armature::remove_bone(uint32_t index) {
    if (index >= bones_.size()) return false;
    const int32_t p_idx = bones_[index].parent_index;
    if (p_idx >= 0 && static_cast<size_t>(p_idx) < bones_.size()) {
        auto& ch = bones_[p_idx].children_indices;
        ch.erase(std::remove(ch.begin(), ch.end(), index), ch.end());
    }

    bones_.erase(bones_.begin() + index);
    poses_.erase(poses_.begin() + index);

    for (size_t i = 0; i < bones_.size(); ++i) {
        bones_[i].id = static_cast<uint32_t>(i);
        if (bones_[i].parent_index == static_cast<int32_t>(index)) {
            bones_[i].parent_index = -1;
            bones_[i].connected = false;
        } else if (bones_[i].parent_index > static_cast<int32_t>(index)) {
            bones_[i].parent_index--;
        }
        for (auto& child : bones_[i].children_indices) {
            if (child > index) child--;
        }
    }
    return true;
}

int32_t Armature::find_bone_index(std::string_view name) const noexcept {
    for (size_t i = 0; i < bones_.size(); ++i) {
        if (bones_[i].name == name) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

void Armature::update_bone_edit(uint32_t index, Point3D new_head, Point3D new_tail, double roll_rad) {
    if (index >= bones_.size()) return;
    bones_[index].head = new_head;
    bones_[index].tail = new_tail;
    bones_[index].roll_rad = roll_rad;

    for (uint32_t child_idx : bones_[index].children_indices) {
        if (bones_[child_idx].connected) {
            const Vec3d delta = new_tail - bones_[child_idx].head;
            bones_[child_idx].head = new_tail;
            bones_[child_idx].tail = bones_[child_idx].tail + delta;
        }
    }
}

void Armature::set_bone_pose(uint32_t index, const BonePose& pose) {
    if (index < poses_.size()) {
        poses_[index] = pose;
    }
}

void Armature::reset_pose() noexcept {
    for (auto& p : poses_) {
        p = BonePose{};
    }
}

Mat4 Armature::get_bone_rest_world_matrix(uint32_t index) const {
    if (index >= bones_.size()) return Mat4::identity();
    const auto& b = bones_[index];

    const double len = b.length();
    Vec3d y_axis = (len > 1e-9) ? (b.tail - b.head) / len : Vec3d(0, 1, 0);

    Vec3d ref = (std::abs(y_axis.z) < 0.9) ? Vec3d(0, 0, 1) : Vec3d(1, 0, 0);
    Vec3d x_axis = y_axis.cross(ref).normalized();
    Vec3d z_axis = x_axis.cross(y_axis).normalized();

    if (std::abs(b.roll_rad) > 1e-6) {
        const Quat q_roll = Quat::from_axis_angle(y_axis, b.roll_rad);
        x_axis = q_roll.rotate(x_axis);
        z_axis = q_roll.rotate(z_axis);
    }

    return make_basis_matrix(x_axis, y_axis, z_axis, b.head);
}

Mat4 Armature::get_bone_world_matrix(uint32_t index) const {
    if (index >= bones_.size()) return Mat4::identity();

    const Mat4 rest_world = get_bone_rest_world_matrix(index);
    const auto& p = poses_[index];

    Mat4 local_pose = Mat4::translation(p.translation) *
                      Mat4::from_quaternion(p.rotation) *
                      Mat4::scaling(p.scale);

    const int32_t parent_idx = bones_[index].parent_index;
    if (parent_idx < 0 || static_cast<size_t>(parent_idx) >= bones_.size()) {
        return rest_world * local_pose;
    }

    const Mat4 parent_rest = get_bone_rest_world_matrix(static_cast<uint32_t>(parent_idx));
    const Mat4 parent_rest_inv = parent_rest.inverse();
    const Mat4 rel_rest = parent_rest_inv * rest_world;

    const Mat4 parent_world = get_bone_world_matrix(static_cast<uint32_t>(parent_idx));
    return parent_world * rel_rest * local_pose;
}

Mat4 Armature::get_skinning_matrix(uint32_t index) const {
    if (index >= bones_.size()) return Mat4::identity();
    const Mat4 world = get_bone_world_matrix(index);
    const Mat4 rest = get_bone_rest_world_matrix(index);
    return world * rest.inverse();
}

SolidMesh3D Armature::generate_octahedron_mesh(uint32_t bone_index, double waist_ratio) const {
    SolidMesh3D mesh;
    if (bone_index >= bones_.size()) return mesh;

    const auto& b = bones_[bone_index];
    const double L = std::max(1e-4, b.length());
    const double yw = L * 0.2;
    const double rw = L * std::clamp(waist_ratio, 0.05, 0.5);

    const std::vector<Point3D> local_pts = {
        Point3D(0.0, 0.0, 0.0),   // 0: Head
        Point3D( rw,  yw, 0.0),   // 1: Waist +X
        Point3D(0.0,  yw,  rw),   // 2: Waist +Z
        Point3D(-rw,  yw, 0.0),   // 3: Waist -X
        Point3D(0.0,  yw, -rw),   // 4: Waist -Z
        Point3D(0.0,   L, 0.0)    // 5: Tail
    };

    const Mat4 world_mat = (mode_ == ArmatureMode::Edit)
                               ? get_bone_rest_world_matrix(bone_index)
                               : get_bone_world_matrix(bone_index);

    mesh.vertices.resize(6);
    for (size_t i = 0; i < 6; ++i) {
        mesh.vertices[i] = transform_point(world_mat, local_pts[i]);
    }

    mesh.triangles = {
        {0, 2, 1},
        {0, 3, 2},
        {0, 4, 3},
        {0, 1, 4},
        {5, 1, 2},
        {5, 2, 3},
        {5, 3, 4},
        {5, 4, 1}
    };

    return mesh;
}

SolidMesh3D Armature::generate_armature_mesh() const {
    SolidMesh3D combined;
    for (uint32_t i = 0; i < bones_.size(); ++i) {
        SolidMesh3D bone_mesh = generate_octahedron_mesh(i);
        const uint32_t offset = static_cast<uint32_t>(combined.vertices.size());
        for (const auto& v : bone_mesh.vertices) {
            combined.vertices.push_back(v);
        }
        for (const auto& tri : bone_mesh.triangles) {
            combined.triangles.push_back({
                tri[0] + offset,
                tri[1] + offset,
                tri[2] + offset
            });
        }
    }
    return combined;
}

} // namespace shunya::rig3d
