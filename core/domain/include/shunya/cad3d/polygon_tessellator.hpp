#pragma once

#include "shunya/cad2d/polygon_boolean.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace shunya::cad3d {

struct TessellatedPolygon2D {
    std::vector<cad2d::Point2D> vertices;
    std::vector<uint32_t> loop_sizes; // [outer_count, hole0_count, hole1_count, ...] matching vertices layout
    std::vector<std::array<uint32_t, 3>> triangles; // CCW triangles
};

/**
 * @brief Planar 2D polygon triangulator supporting arbitrary concave boundaries with multiple holes.
 *
 * Bridges internal hole loops into a single simple polygon using Eberly's mutually-visible vertex bridge
 * and triangulates via robust ear clipping.
 */
class PolygonTessellator {
public:
    /**
     * @brief Triangulates a single outer boundary and zero or more internal holes.
     *
     * Automatically normalizes outer loop to CCW and holes to CW before bridging.
     * Returned triangle indices address `out_vertices` (if combined) or `TessellatedPolygon2D::vertices`.
     */
    [[nodiscard]] static TessellatedPolygon2D triangulate(
        const cad2d::Path2D& outer,
        const cad2d::Paths2D& holes = {}
    );
};

} // namespace shunya::cad3d
