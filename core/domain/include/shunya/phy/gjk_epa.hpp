#pragma once

#include "shunya/core/domain/mechanics.hpp"

#include <cstdint>
#include <vector>

namespace shunya::phy {

enum class ColliderKind : uint8_t {
    Sphere,
    Box,
    Capsule,
    ConvexHull
};

struct ConvexCollider {
    ColliderKind kind{ColliderKind::Sphere};
    double radius{0.5};                                         // Sphere / Capsule radius
    double half_height{0.5};                                    // Capsule Y half-height (cylinder segment)
    core::domain::mechanics::Vector3d half_extents{0.5, 0.5, 0.5}; // Box half-extents
    std::vector<core::domain::mechanics::Vector3d> hull_vertices;  // Local convex hull points

    core::domain::mechanics::Vector3d position{0.0, 0.0, 0.0};
    core::domain::mechanics::Quaterniond orientation{1.0, 0.0, 0.0, 0.0};

    [[nodiscard]] core::domain::mechanics::Vector3d support_world(
        const core::domain::mechanics::Vector3d& dir_world) const noexcept;

    [[nodiscard]] core::domain::mechanics::AABB world_aabb(double margin = 0.0) const noexcept;
    [[nodiscard]] double max_bounding_radius() const noexcept;
};

struct GjkEpaResult {
    bool intersecting{false};
    double distance{0.0};            // Positive when disjoint, 0 when intersecting
    double penetration_depth{0.0};   // Positive when intersecting
    core::domain::mechanics::Vector3d normal{0.0, 1.0, 0.0}; // Points from B to A (or closest dir B -> A)
    core::domain::mechanics::Vector3d point_a{0.0, 0.0, 0.0};
    core::domain::mechanics::Vector3d point_b{0.0, 0.0, 0.0};
};

/**
 * @brief Gilbert-Johnson-Keerthi (GJK) distance & intersection detector plus Expanding Polytope Algorithm (EPA).
 */
class GjkEpaSolver {
public:
    static constexpr int kMaxGjkIterations = 32;
    static constexpr int kMaxEpaIterations = 48;

    [[nodiscard]] static GjkEpaResult detect(
        const ConvexCollider& a,
        const ConvexCollider& b) noexcept;
};

} // namespace shunya::phy
