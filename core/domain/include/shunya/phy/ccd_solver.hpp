#pragma once

#include "shunya/phy/gjk_epa.hpp"
#include "shunya/phy/swept_volume.hpp"

namespace shunya::phy {

struct CcdResult {
    bool hit{false};
    double toi{1.0}; // Time of impact in [0, 1]
    core::domain::mechanics::Vector3d normal{0.0, 1.0, 0.0};
    core::domain::mechanics::Vector3d hit_point{0.0, 0.0, 0.0};
};

/**
 * @brief Continuous Collision Detection (CCD) via conservative advancement preventing high-speed tunneling.
 */
class CcdSolver {
public:
    static constexpr int kMaxCcdIterations = 64;

    [[nodiscard]] static CcdResult compute_toi(
        const ConvexCollider& shape_a,
        const MotionTransformSweep& sweep_a,
        const ConvexCollider& shape_b,
        const MotionTransformSweep& sweep_b,
        double tolerance = 1e-4) noexcept;
};

} // namespace shunya::phy
