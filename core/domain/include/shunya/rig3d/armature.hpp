#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::rig3d {

using namespace cad3d;
using core::base::math::Mat4;
using core::base::math::Quat;

struct Bone {
    uint32_t id{0};
    std::string name{"Bone"};
    Point3D head{0.0, 0.0, 0.0};        ///< Joint start position (Edit Mode)
    Point3D tail{0.0, 1.0, 0.0};        ///< Joint tip position (Edit Mode)
    double roll_rad{0.0};               ///< Axial roll angle around bone vector
    int32_t parent_index{-1};           ///< Index in Armature (-1 = root)
    std::vector<uint32_t> children_indices;
    bool connected{false};              ///< Connected head locked to parent tail

    [[nodiscard]] double length() const noexcept {
        return (tail - head).length();
    }

    [[nodiscard]] Vec3d direction() const noexcept {
        const double len = length();
        if (len > 1e-9) return (tail - head) / len;
        return Vec3d(0.0, 1.0, 0.0);
    }
};

struct BonePose {
    Point3D translation{0.0, 0.0, 0.0}; ///< Translation delta from rest pose
    Quat rotation{Quat::identity()};    ///< Rotation delta from rest pose
    Vec3d scale{1.0, 1.0, 1.0};         ///< Scale factor
};

enum class ArmatureMode {
    Edit,                               ///< Repositioning joints & rest hierarchy
    Pose                                ///< Posing joints relative to rest pose
};

/**
 * @brief Skeletal armature entity representing a tree of connected bones.
 * Supports Edit Mode (joint placement & rest poses) and Pose Mode (FK/IK transformations).
 */
class Armature {
public:
    Armature() = default;

    uint32_t add_bone(
        std::string name,
        Point3D head,
        Point3D tail,
        int32_t parent_index = -1,
        bool connected = false,
        double roll_rad = 0.0
    );

    bool remove_bone(uint32_t index);

    [[nodiscard]] int32_t find_bone_index(std::string_view name) const noexcept;
    [[nodiscard]] size_t bone_count() const noexcept { return bones_.size(); }
    [[nodiscard]] bool empty() const noexcept { return bones_.empty(); }

    [[nodiscard]] const Bone& bone(size_t index) const { return bones_[index]; }
    [[nodiscard]] Bone& bone(size_t index) { return bones_[index]; }

    [[nodiscard]] const BonePose& pose(size_t index) const { return poses_[index]; }
    [[nodiscard]] BonePose& pose(size_t index) { return poses_[index]; }

    void set_mode(ArmatureMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] ArmatureMode mode() const noexcept { return mode_; }

    void update_bone_edit(uint32_t index, Point3D new_head, Point3D new_tail, double roll_rad = 0.0);
    void set_bone_pose(uint32_t index, const BonePose& pose);
    void reset_pose() noexcept;

    /// PH4-11.03.004: World-space transformation matrix for a bone in rest bind pose
    [[nodiscard]] Mat4 get_bone_rest_world_matrix(uint32_t index) const;

    /// World-space transformation matrix for a bone in current active pose
    [[nodiscard]] Mat4 get_bone_world_matrix(uint32_t index) const;

    /// Skinning matrix palette: M_i = W_i * B_i^-1
    [[nodiscard]] Mat4 get_skinning_matrix(uint32_t index) const;

    /// PH4-11.03.002: Generate interactive 3D octahedron bone visualization mesh
    [[nodiscard]] SolidMesh3D generate_octahedron_mesh(uint32_t bone_index, double waist_ratio = 0.15) const;

    /// Generate combined visualization mesh of all bones in the armature
    [[nodiscard]] SolidMesh3D generate_armature_mesh() const;

private:
    std::vector<Bone> bones_;
    std::vector<BonePose> poses_;
    ArmatureMode mode_{ArmatureMode::Edit};
};

} // namespace shunya::rig3d
