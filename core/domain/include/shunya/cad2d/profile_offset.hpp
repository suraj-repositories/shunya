#pragma once

#include "shunya/cad2d/polygon_boolean.hpp"

namespace shunya::cad2d {

enum class JoinType {
    Miter,
    Round,
    Square
};

/**
 * @brief Generates interior and exterior offset curves/polygons with Miter, Round, and Square joins.
 */
class ProfileOffsetEngine {
public:
    /**
     * @brief Offsets a collection of closed polygons by delta (+ outward expansion, - inward erosion).
     */
    [[nodiscard]] static Paths2D offset_polygons(
        const Paths2D& polygons,
        double delta,
        JoinType join_type = JoinType::Miter,
        double miter_limit = 2.0,
        size_t arc_segments = 16
    );

    /**
     * @brief Offsets an open polyline to left/right by signed distance delta.
     */
    [[nodiscard]] static Path2D offset_open_polyline(
        const Path2D& polyline,
        double delta,
        JoinType join_type = JoinType::Miter,
        double miter_limit = 2.0,
        size_t arc_segments = 16
    );
};

} // namespace shunya::cad2d
