#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

namespace shunya::cad2d {

struct FilletCornerResult {
    LineSegment2D trimmed_leg1;
    Arc2D arc;
    LineSegment2D trimmed_leg2;
    double trim_distance{0.0};
};

struct ChamferCornerResult {
    LineSegment2D trimmed_leg1;
    LineSegment2D bevel;
    LineSegment2D trimmed_leg2;
};

/**
 * @brief Analytical corner filleting (G1 circular arc blending) and chamfering engine for 2D profiles.
 */
class FilletChamferEngine {
public:
    /**
     * @brief Fillets corner (a -> vertex -> b) with circular arc of specified radius R.
     */
    [[nodiscard]] static core::base::Result<FilletCornerResult> fillet_corner(
        const Point2D& a,
        const Point2D& vertex,
        const Point2D& b,
        double radius
    );

    /**
     * @brief Chamfers corner (a -> vertex -> b) with setback distances dist_a (along vertex->a) and dist_b (along vertex->b).
     */
    [[nodiscard]] static core::base::Result<ChamferCornerResult> chamfer_corner(
        const Point2D& a,
        const Point2D& vertex,
        const Point2D& b,
        double dist_a,
        double dist_b
    );

    /**
     * @brief Applies filleting of uniform radius to all corners of a closed polygon profile.
     */
    [[nodiscard]] static Path2D fillet_polygon(
        const Path2D& polygon,
        double radius,
        size_t arc_segments = 12
    );
};

} // namespace shunya::cad2d
