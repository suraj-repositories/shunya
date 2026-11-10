#pragma once

#include "shunya/cad2d/curve_2d.hpp"

#include <cstdint>
#include <vector>

namespace shunya::cad2d {

enum class BooleanOp {
    Union,
    Intersection,
    Difference,
    Xor
};

enum class FillRule {
    EvenOdd,
    NonZero
};

using Path2D = std::vector<Point2D>;
using Paths2D = std::vector<Path2D>;

struct PolygonWithHoles2D {
    Path2D outer;
    Paths2D holes;
};

/**
 * @brief Robust integer-quantized 2D planar polygon Boolean & cleanup engine.
 *
 * Maps double-precision floating-point coordinates onto a 1e6-scaled int64_t integer grid
 * for topological arrangement, exact segment splitting, and winding-based Boolean extraction.
 */
class PolygonBooleanEngine {
public:
    static constexpr double DEFAULT_SCALE = 1e6;

    /**
     * @brief Performs Union, Intersection, Difference (subject - clip), or Xor on polygon sets.
     */
    [[nodiscard]] static Paths2D execute(
        BooleanOp op,
        const Paths2D& subject,
        const Paths2D& clip,
        FillRule fill_rule = FillRule::NonZero,
        double scale = DEFAULT_SCALE
    );

    /**
     * @brief Simplifies self-intersecting polygons into clean CCW outers and CW holes.
     */
    [[nodiscard]] static Paths2D simplify(
        const Paths2D& polygons,
        FillRule fill_rule = FillRule::NonZero,
        double scale = DEFAULT_SCALE
    );

    /**
     * @brief Computes signed shoelace area (> 0 for CCW outer boundary, < 0 for CW hole).
     */
    [[nodiscard]] static double signed_area(const Path2D& path) noexcept;
    [[nodiscard]] static double area(const Path2D& path) noexcept { return std::abs(signed_area(path)); }
    [[nodiscard]] static double net_area(const Paths2D& paths) noexcept;

    /**
     * @brief Checks whether a point lies inside a polygon path using EvenOdd/NonZero winding.
     */
    [[nodiscard]] static bool point_in_path(const Point2D& pt, const Path2D& path) noexcept;

    /**
     * @brief Groups flat CCW outer + CW hole Paths2D into structured PolygonWithHoles2D hierarchies.
     */
    [[nodiscard]] static std::vector<PolygonWithHoles2D> extract_hierarchy(const Paths2D& paths);
};

} // namespace shunya::cad2d
