#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/rig3d/armature.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace shunya::rig3d {

using core::base::math::Quat;

/**
 * @brief Forward Kinematics (FK) solver (PH4-11.04.003).
 * Allows rotational posing of individual bones with hierarchical downstream propagation.
 */
class ForwardKinematics {
public:
    static void set_bone_rotation(Armature& armature, uint32_t bone_index, const Quat& rotation);
    static void apply_local_rotation(Armature& armature, uint32_t bone_index, const Quat& delta_rotation);
    [[nodiscard]] static Point3D get_bone_tip_position(const Armature& armature, uint32_t bone_index);
};

struct IKGoal {
    uint32_t end_effector_bone{0};      ///< Bone whose tip should touch the target
    Point3D target_position{0, 0, 0};   ///< Desired world position
    Point3D pole_vector{0, 0, 1};       ///< Direction hint for elbow/knee bend plane
    bool use_pole_vector{false};
    uint32_t chain_length{2};           ///< Number of bones up the hierarchy in the chain
    uint32_t max_iterations{30};        ///< Max solver iterations
    double tolerance{1e-4};             ///< Target proximity threshold (meters)
};

struct IKSolveResult {
    bool converged{false};
    uint32_t iterations_used{0};
    double final_error{0.0};
};

/**
 * @brief 2-Bone Analytical & Multi-Bone CCD Inverse Kinematics (IK) solver (PH4-11.04.004).
 */
class InverseKinematics {
public:
    /**
     * @brief Closed-form analytical 2-bone IK solver using Law of Cosines.
     * Ideal for robotic arms, character legs, and arms.
     */
    static IKSolveResult solve_2bone(
        Armature& armature,
        uint32_t root_bone_idx,
        uint32_t mid_bone_idx,
        const Point3D& target_pos,
        const Point3D* pole_vector = nullptr
    );

    /**
     * @brief Multi-bone Cyclic Coordinate Descent (CCD) IK solver for arbitrary chain lengths.
     */
    static IKSolveResult solve_ccd(Armature& armature, const IKGoal& goal);
};

} // namespace shunya::rig3d
