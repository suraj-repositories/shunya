#pragma once

#include "shunya/physim/rigid_body.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace shunya::physim {

using AABB = core::domain::mechanics::AABB;

// =============================================================================
// Collision Shape Primitives
// =============================================================================
enum class ShapeKind : uint8_t {
    Sphere,
    Box,
    Capsule,
    Cylinder,
    ConvexPolyhedron
};

struct ColliderShape {
    ShapeKind kind{ShapeKind::Sphere};
    double radius{0.5};                                 ///< Sphere, capsule, cylinder radius (m)
    double half_height{0.5};                            ///< Capsule, cylinder half-height along local Y (m)
    Vector3d half_extents{0.5, 0.5, 0.5};               ///< Box half-extents (m)
    std::vector<Vector3d> vertices;                     ///< Convex hull local vertices

    [[nodiscard]] Vector3d support_local(const Vector3d& dir_local) const noexcept;
    [[nodiscard]] Vector3d support_world(
        const Vector3d& dir_world,
        const Vector3d& pos,
        const Quaterniond& orient) const noexcept;

    [[nodiscard]] AABB compute_world_aabb(
        const Vector3d& pos,
        const Quaterniond& orient,
        double margin = 0.02) const noexcept;

    [[nodiscard]] double bounding_radius() const noexcept;
};

// =============================================================================
// Broadphase AABB BVH and Spatial Hashing (PH4-09.02.001)
// =============================================================================
struct BvhNode {
    AABB aabb;
    int parent{-1};
    int left{-1};
    int right{-1};
    int body_index{-1};
    [[nodiscard]] bool is_leaf() const noexcept { return left == -1 && right == -1; }
};

class DynamicAabbBvh {
public:
    DynamicAabbBvh() = default;

    void build(const std::vector<AABB>& boxes);
    void clear() noexcept;

    [[nodiscard]] std::vector<std::pair<size_t, size_t>> query_overlapping_pairs() const;

private:
    void insert_leaf(const AABB& box, int body_index);

    std::vector<BvhNode> nodes_;
    int root_{-1};
};

class SpatialHashGrid {
public:
    explicit SpatialHashGrid(double cell_size = 1.0) : cell_size_(cell_size) {}

    void clear() noexcept;
    void insert(size_t body_index, const AABB& aabb);

    [[nodiscard]] std::vector<std::pair<size_t, size_t>> query_overlapping_pairs() const;

private:
    [[nodiscard]] static uint64_t hash_cell(int cx, int cy, int cz) noexcept;

    double cell_size_{1.0};
    std::unordered_map<uint64_t, std::vector<size_t>> grid_;
};

// =============================================================================
// GJK & EPA Narrowphase Detector (PH4-09.02.002)
// =============================================================================
struct GjkEpaOutput {
    bool intersecting{false};
    double distance{0.0};                           ///< Positive when disjoint
    double penetration_depth{0.0};                  ///< Positive when colliding
    Vector3d normal{0.0, 1.0, 0.0};                 ///< Normal pointing from Body B to Body A
    Vector3d point_a{0.0, 0.0, 0.0};                ///< Closest contact point on A (world)
    Vector3d point_b{0.0, 0.0, 0.0};                ///< Closest contact point on B (world)
};

class GjkEpaNarrowphase {
public:
    static constexpr int kMaxGjkIterations = 32;
    static constexpr int kMaxEpaIterations = 48;

    [[nodiscard]] static GjkEpaOutput detect(
        const ColliderShape& shape_a, const Vector3d& pos_a, const Quaterniond& orient_a,
        const ColliderShape& shape_b, const Vector3d& pos_b, const Quaterniond& orient_b) noexcept;
};

// =============================================================================
// Continuous Collision Detection (CCD) via Conservative Advancement (PH4-09.02.003)
// =============================================================================
struct MotionTransformSweep {
    Vector3d start_position{0.0, 0.0, 0.0};
    Quaterniond start_orientation{1.0, 0.0, 0.0, 0.0};
    Vector3d end_position{0.0, 0.0, 0.0};
    Quaterniond end_orientation{1.0, 0.0, 0.0, 0.0};
    Vector3d linear_velocity{0.0, 0.0, 0.0};
    Vector3d angular_velocity{0.0, 0.0, 0.0};
};

struct CcdOutput {
    bool hit{false};
    double toi{1.0};                                ///< Time of impact in [0, 1]
    Vector3d hit_normal{0.0, 1.0, 0.0};             ///< Normal pointing from B to A at impact
    Vector3d hit_point{0.0, 0.0, 0.0};              ///< World contact location at impact
};

class ContinuousCollisionDetector {
public:
    static constexpr int kMaxCcdSteps = 64;

    [[nodiscard]] static CcdOutput compute_toi(
        const ColliderShape& shape_a, const MotionTransformSweep& sweep_a,
        const ColliderShape& shape_b, const MotionTransformSweep& sweep_b,
        double tolerance = 1e-4) noexcept;
};

// =============================================================================
// Contact Manifold Generation & Caching (PH4-09.02.004)
// =============================================================================
struct PhysimContactPoint {
    Vector3d world_position{0.0, 0.0, 0.0};
    Vector3d normal{0.0, 1.0, 0.0};                 ///< Normal pointing from B to A
    double penetration_depth{0.0};
    Vector3d local_anchor_a{0.0, 0.0, 0.0};
    Vector3d local_anchor_b{0.0, 0.0, 0.0};
    double normal_impulse{0.0};
    double tangent_impulse{0.0};
};

struct PhysimContactManifold {
    std::string body_a_id;
    std::string body_b_id;
    std::vector<PhysimContactPoint> contacts;
    double restitution{0.2};
    double static_friction{0.5};
    double dynamic_friction{0.4};
};

class ContactManifoldGenerator {
public:
    /**
     * @brief Generates complete contact manifold with local anchors and multi-point contact patch.
     */
    [[nodiscard]] static PhysimContactManifold generate_manifold(
        const std::string& id_a, const ColliderShape& shape_a, const RigidBodyState& state_a,
        const std::string& id_b, const ColliderShape& shape_b, const RigidBodyState& state_b,
        double restitution = 0.2,
        double static_friction = 0.5,
        double dynamic_friction = 0.4) noexcept;

    /**
     * @brief Refreshes and updates contact points across successive frames for temporal coherence.
     */
    static void update_manifold_coherence(
        PhysimContactManifold& manifold,
        const RigidBodyState& state_a,
        const RigidBodyState& state_b,
        double max_separation_drift = 0.05) noexcept;
};

} // namespace shunya::physim
