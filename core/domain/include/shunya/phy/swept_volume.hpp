#pragma once

#include "shunya/core/domain/mechanics.hpp"
#include "shunya/phy/gjk_epa.hpp"

namespace shunya::phy {

struct MotionTransformSweep {
    core::domain::mechanics::Vector3d p0{0.0, 0.0, 0.0};
    core::domain::mechanics::Vector3d p1{0.0, 0.0, 0.0};
    core::domain::mechanics::Quaterniond q0{1.0, 0.0, 0.0, 0.0};
    core::domain::mechanics::Quaterniond q1{1.0, 0.0, 0.0, 0.0};

    [[nodiscard]] ConvexCollider interpolate(const ConvexCollider& base, double t) const noexcept {
        ConvexCollider c = base;
        c.position = p0 + (p1 - p0) * t;
        c.orientation = core::domain::mechanics::Quaterniond::slerp(q0, q1, t);
        return c;
    }

    [[nodiscard]] core::domain::mechanics::AABB swept_aabb(const ConvexCollider& base, double margin = 0.01) const noexcept {
        ConvexCollider c0 = base;
        c0.position = p0;
        c0.orientation = q0;
        ConvexCollider c1 = base;
        c1.position = p1;
        c1.orientation = q1;
        core::domain::mechanics::AABB a0 = c0.world_aabb(margin);
        core::domain::mechanics::AABB a1 = c1.world_aabb(margin);
        a0.expand(a1.min_point);
        a0.expand(a1.max_point);
        return a0;
    }
};

} // namespace shunya::phy
