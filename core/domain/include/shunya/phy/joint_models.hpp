#pragma once

#include "shunya/core/domain/mechanics.hpp"

#include <cstddef>
#include <cstdint>

namespace shunya::phy {

enum class JointType : uint8_t {
    Spherical, // 3-DOF ball-and-socket (position anchor match)
    Revolute,  // Hinge (3-DOF position + 2-DOF axis alignment)
    Prismatic  // Slider (2-DOF perpendicular position + 3-DOF orientation lock)
};

struct ArticulatedJoint {
    JointType type{JointType::Spherical};
    size_t body_a{0};
    size_t body_b{0}; // If static_world_anchor is true, body_a is anchored to world_anchor directly
    bool static_world_anchor{false};

    core::domain::mechanics::Vector3d local_anchor_a{0.0, 0.0, 0.0};
    core::domain::mechanics::Vector3d local_anchor_b{0.0, 0.0, 0.0}; // Also world anchor if static_world_anchor
    core::domain::mechanics::Vector3d local_axis_a{0.0, 0.0, 1.0};
    core::domain::mechanics::Vector3d local_axis_b{0.0, 0.0, 1.0};
};

struct ContactConstraintSpec {
    size_t body_a{0};
    size_t body_b{0};
    bool body_b_static{false};
    core::domain::mechanics::Vector3d world_point{0.0, 0.0, 0.0};
    core::domain::mechanics::Vector3d normal{0.0, 1.0, 0.0}; // Points from B -> A
    double penetration_depth{0.0};
    double friction{0.5};
    double restitution{0.1};
};

} // namespace shunya::phy
