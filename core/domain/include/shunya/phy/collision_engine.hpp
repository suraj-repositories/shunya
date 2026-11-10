#pragma once

#include "shunya/core/domain/mechanics.hpp"
#include "shunya/phy/gjk_epa.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace shunya::phy {

struct BroadphaseNode {
    core::domain::mechanics::AABB aabb;
    int parent{-1};
    int left{-1};
    int right{-1};
    int body_index{-1};
    bool is_leaf() const noexcept { return left == -1 && right == -1; }
};

/**
 * @brief Dynamic AABB bounding volume tree for fast O(N log N) broadphase pair culling + GJK/EPA narrowphase.
 */
class CollisionEngine {
public:
    [[nodiscard]] static std::vector<std::pair<size_t, size_t>> find_broadphase_pairs(
        const std::vector<ConvexCollider>& colliders,
        double aabb_margin = 0.01);

    [[nodiscard]] static std::vector<core::domain::mechanics::ContactManifold> detect_all_contacts(
        const std::vector<ConvexCollider>& colliders,
        double aabb_margin = 0.01);
};

} // namespace shunya::phy
