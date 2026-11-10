#pragma once

#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/cad3d/polygon_tessellator.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <optional>
#include <vector>

namespace shunya::cad3d {

/**
 * @brief Orthonormal 3D construction/sketch plane mapping 2D (u, v) coordinates into 3D space.
 */
struct SketchPlane3D {
    Point3D origin{0.0, 0.0, 0.0};
    Vec3d axis_u{1.0, 0.0, 0.0};
    Vec3d axis_v{0.0, 1.0, 0.0};

    [[nodiscard]] static SketchPlane3D xy(double z_offset = 0.0) noexcept {
        return {{0.0, 0.0, z_offset}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    }

    [[nodiscard]] Vec3d normal() const noexcept {
        return axis_u.cross(axis_v).normalized();
    }

    [[nodiscard]] Point3D to_3d(const cad2d::Point2D& p2, double height = 0.0) const noexcept {
        return origin + axis_u * p2.x + axis_v * p2.y + normal() * height;
    }
};

enum class EndCondition {
    Blind,
    Symmetric,
    ThroughAll,
    MidPlane
};

struct ExtrusionParams {
    double depth{10.0};
    double through_all_extent{100.0};
    double draft_angle_rad{0.0}; // positive = flare outward along extrusion direction
    EndCondition end_condition{EndCondition::Blind};
    SketchPlane3D plane{SketchPlane3D::xy()};
};

/**
 * @brief Generates watertight 2-manifold 3D prismatic & drafted solid meshes from closed 2D profiles.
 */
class ExtrusionEngine {
public:
    [[nodiscard]] static SolidMesh3D extrude(
        const cad2d::Path2D& outer,
        const cad2d::Paths2D& holes = {},
        const ExtrusionParams& params = {}
    );

    /**
     * @brief Extrudes an already-classified multi-loop profile set (outer CCW loops + CW holes via PolygonBooleanEngine).
     */
    [[nodiscard]] static std::vector<SolidMesh3D> extrude_paths(
        const cad2d::Paths2D& profile_paths,
        const ExtrusionParams& params = {}
    );
};

} // namespace shunya::cad3d
