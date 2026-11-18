#include "shunya/rig3d/kinematics.hpp"

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

// =============================================================================
// ForwardKinematics Implementation (PH4-11.04.003)
// =============================================================================
void ForwardKinematics::set_bone_rotation(Armature& armature, uint32_t bone_index, const Quat& rotation) {
    if (bone_index < armature.bone_count()) {
        auto pose = armature.pose(bone_index);
        pose.rotation = rotation.normalized();
        armature.set_bone_pose(bone_index, pose);
    }
}

void ForwardKinematics::apply_local_rotation(Armature& armature, uint32_t bone_index, const Quat& delta_rotation) {
    if (bone_index < armature.bone_count()) {
        auto pose = armature.pose(bone_index);
        pose.rotation = (delta_rotation * pose.rotation).normalized();
        armature.set_bone_pose(bone_index, pose);
    }
}

Point3D ForwardKinematics::get_bone_tip_position(const Armature& armature, uint32_t bone_index) {
    if (bone_index >= armature.bone_count()) return Point3D(0, 0, 0);
    const Mat4 world_mat = armature.get_bone_world_matrix(bone_index);
    const double L = armature.bone(bone_index).length();
    return transform_point(world_mat, Point3D(0.0, L, 0.0));
}

// =============================================================================
// InverseKinematics Implementation (PH4-11.04.004)
// =============================================================================
IKSolveResult InverseKinematics::solve_2bone(
    Armature& armature,
    uint32_t root_bone_idx,
    uint32_t mid_bone_idx,
    const Point3D& target_pos,
    const Point3D* pole_vector
) {
    (void)root_bone_idx;
    (void)pole_vector;

    IKGoal goal;
    goal.end_effector_bone = mid_bone_idx;
    goal.target_position = target_pos;
    goal.chain_length = 2;
    goal.max_iterations = 60;
    goal.tolerance = 0.02;

    return solve_ccd(armature, goal);
}

IKSolveResult InverseKinematics::solve_ccd(Armature& armature, const IKGoal& goal) {
    IKSolveResult result;
    if (goal.end_effector_bone >= armature.bone_count()) return result;

    // Collect chain of bone indices from end effector upwards
    std::vector<uint32_t> chain;
    uint32_t curr = goal.end_effector_bone;
    for (uint32_t i = 0; i < goal.chain_length; ++i) {
        chain.push_back(curr);
        int32_t parent = armature.bone(curr).parent_index;
        if (parent < 0 || static_cast<size_t>(parent) >= armature.bone_count()) break;
        curr = static_cast<uint32_t>(parent);
    }

    for (uint32_t iter = 0; iter < goal.max_iterations; ++iter) {
        result.iterations_used = iter + 1;

        Point3D tip_pos = ForwardKinematics::get_bone_tip_position(armature, goal.end_effector_bone);
        result.final_error = (tip_pos - goal.target_position).length();
        if (result.final_error <= goal.tolerance) {
            result.converged = true;
            return result;
        }

        // Iterate up the chain from effector to root
        for (uint32_t b_idx : chain) {
            tip_pos = ForwardKinematics::get_bone_tip_position(armature, goal.end_effector_bone);
            const Mat4 b_world = armature.get_bone_world_matrix(b_idx);
            const Point3D joint_pos = transform_point(b_world, Point3D(0, 0, 0));

            Vec3d to_tip = tip_pos - joint_pos;
            Vec3d to_target = goal.target_position - joint_pos;

            const double tip_len = to_tip.length();
            const double target_len = to_target.length();
            if (tip_len < 1e-6 || target_len < 1e-6) continue;

            to_tip = to_tip / tip_len;
            to_target = to_target / target_len;

            Vec3d rot_axis = to_tip.cross(to_target);
            const double axis_len = rot_axis.length();
            if (axis_len < 1e-6) continue;
            rot_axis = rot_axis / axis_len;

            const double dot = std::clamp(to_tip.dot(to_target), -1.0, 1.0);
            const double angle = std::acos(dot);
            if (angle < 1e-5) continue;

            // Damped rotation step
            const double damped_angle = std::min(angle, 0.5);
            const Quat delta_q = Quat::from_axis_angle(rot_axis, damped_angle);

            ForwardKinematics::apply_local_rotation(armature, b_idx, delta_q);
        }
    }

    Point3D tip_pos = ForwardKinematics::get_bone_tip_position(armature, goal.end_effector_bone);
    result.final_error = (tip_pos - goal.target_position).length();
    result.converged = (result.final_error <= goal.tolerance);
    return result;
}

} // namespace shunya::rig3d
