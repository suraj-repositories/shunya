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
    Flatten,
    Mask
};

struct BrushConfig {
    BrushType type{BrushType::Clay};
    double radius{0.25};                ///< Sphere of influence radius
    double strength{0.5};               ///< Displacement strength [0.0..1.0]
    bool invert{false};                 ///< Invert displacement (e.g. carve vs build / unmask)
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

// =============================================================================
// Dynamic Vertex Masking (PH4-11.02.004)
// =============================================================================
class VertexMask {
public:
    VertexMask() = default;
    explicit VertexMask(size_t vertex_count, double initial_value = 0.0)
        : values_(vertex_count, std::clamp(initial_value, 0.0, 1.0)) {}

    void resize(size_t vertex_count, double default_val = 0.0) {
        values_.resize(vertex_count, std::clamp(default_val, 0.0, 1.0));
    }

    void clear() noexcept {
        std::fill(values_.begin(), values_.end(), 0.0);
    }

    void invert() noexcept {
        for (auto& v : values_) {
            v = 1.0 - v;
        }
    }

    [[nodiscard]] double get(size_t index) const noexcept {
        if (index < values_.size()) return values_[index];
        return 0.0;
    }

    void set(size_t index, double value) noexcept {
        if (index < values_.size()) {
            values_[index] = std::clamp(value, 0.0, 1.0);
        }
    }

    [[nodiscard]] size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
    [[nodiscard]] const std::vector<double>& values() const noexcept { return values_; }

    void paint(const SolidMesh3D& mesh, const StrokePoint& stroke, double radius, double strength, bool invert_stroke) noexcept;

private:
    std::vector<double> values_;
};

// =============================================================================
// Sculpt Engine with Masking & Symmetry
// =============================================================================
class SculptEngine {
public:
    explicit SculptEngine(BrushConfig config = {}) : config_(config) {}

    [[nodiscard]] const BrushConfig& config() const noexcept { return config_; }
    void set_config(const BrushConfig& cfg) noexcept { config_ = cfg; }

    /**
     * @brief Evaluates a brush stroke on the provided mesh.
     * When mask is provided, vertex displacement is attenuated by (1.0 - mask[i]).
     * When symmetry_x is enabled, automatically computes and applies mirrored deformation at (-x, y, z).
     */
    SculptResult apply_stroke(SolidMesh3D& mesh, const StrokePoint& stroke, VertexMask* mask = nullptr) const;

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
        double& out_max_disp,
        VertexMask* mask
    ) const;

    BrushConfig config_;
};

// =============================================================================
// Mesh Surface Smoothing & Normal Smoothing Passes (PH4-11.02.005)
// =============================================================================
class SurfaceSmoother {
public:
    /**
     * @brief Standard Laplacian smoothing pass.
     * @param lambda Blending factor in [0, 1].
     */
    static void laplacian_smooth(SolidMesh3D& mesh, uint32_t iterations = 1, double lambda = 0.5);

    /**
     * @brief Taubin non-shrinking smoothing pass (alternates shrinking +lambda and unshrinking -mu).
     * Preserves total mesh volume without shrinking.
     */
    static void taubin_smooth(SolidMesh3D& mesh, uint32_t iterations = 5, double lambda = 0.5, double mu = -0.53);

    /**
     * @brief Area/angle-weighted normal smoothing with crease angle threshold.
     * Edges with dihedral angle > crease_angle_rad are preserved as sharp creases.
     */
    static void normal_smooth_pass(SolidMesh3D& mesh, double crease_angle_rad = 0.785398);
};

} // namespace shunya::rig3d
