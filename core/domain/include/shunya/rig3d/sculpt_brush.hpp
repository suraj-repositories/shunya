#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace shunya::rig3d {

using namespace cad3d;

enum class BrushType {
    Clay,
    Smooth,
    Grab,
    Inflate,
    Flatten
};

struct BrushConfig {
    BrushType type{BrushType::Clay};
    double radius{0.25};                ///< Sphere of influence radius
    double strength{0.5};               ///< Displacement strength [0.0..1.0]
    bool invert{false};                 ///< Invert displacement (e.g. carve vs build)
    bool symmetry_x{false};             ///< Bilateral X-axis mirror symmetry
    double symmetry_tolerance{0.005};   ///< Threshold for center-plane clamping
};

struct StrokePoint {
    Point3D position{0.0, 0.0, 0.0};    ///< Hit point on surface
    Vec3d normal{0.0, 0.0, 1.0};        ///< Surface normal at hit point
    Vec3d motion_vector{0.0, 0.0, 0.0}; ///< For grab brush: translation delta
};

struct SculptResult {
    uint32_t vertices_modified{0};
    double max_displacement{0.0};
};

class SculptEngine {
public:
    explicit SculptEngine(BrushConfig config = {}) : config_(config) {}

    [[nodiscard]] const BrushConfig& config() const noexcept { return config_; }
    void set_config(const BrushConfig& cfg) noexcept { config_ = cfg; }

    /**
     * @brief Evaluates a brush stroke on the provided mesh.
     * Modifies vertex positions in-place and recomputes affected surface normals.
     * When symmetry_x is enabled, automatically computes and applies mirrored deformation at (-x, y, z).
     */
    SculptResult apply_stroke(SolidMesh3D& mesh, const StrokePoint& stroke) const;

    /**
     * @brief Computes smooth falloff weight for a vertex at distance d from stroke center.
     * Uses smooth cubic polynomial falloff: w(u) = (1 - u^2)^2 for u in [0, 1].
     */
    [[nodiscard]] static double compute_falloff(double distance, double radius) noexcept {
        if (distance >= radius || radius <= 0.0) return 0.0;
        const double u = distance / radius;
        const double one_minus_u2 = 1.0 - (u * u);
        return one_minus_u2 * one_minus_u2;
    }

private:
    void apply_single_pass(
        SolidMesh3D& mesh,
        const StrokePoint& stroke,
        std::vector<bool>& modified_mask,
        double& out_max_disp
    ) const;

    BrushConfig config_;
};

} // namespace shunya::rig3d
