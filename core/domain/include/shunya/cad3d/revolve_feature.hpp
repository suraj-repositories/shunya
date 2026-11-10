#pragma once

#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/cad3d/polygon_tessellator.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

namespace shunya::cad3d {

struct Axis3D {
    Point3D origin{0.0, 0.0, 0.0};
    Vec3d direction{0.0, 1.0, 0.0};
};

struct RevolveParams {
    Axis3D axis{};
    double angle_rad{2.0 * core::base::math::PI};
    size_t angular_segments{64};
    SketchPlane3D plane{SketchPlane3D::xy()};
};

/**
 * @brief Generates watertight 3D solids by revolving closed 2D profiles around an arbitrary 3D axis.
 */
class RevolveEngine {
public:
    [[nodiscard]] static SolidMesh3D revolve(
        const cad2d::Path2D& outer,
        const cad2d::Paths2D& holes = {},
        const RevolveParams& params = {}
    );
};

} // namespace shunya::cad3d
