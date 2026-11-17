#pragma once

#include "shunya/cad2d/snap_manager.hpp"
#include "shunya/cad2d/sketch_viewport_2d.hpp"

#include <string>
#include <vector>

namespace shunya::cad2d {

struct SnapGlyphGeometry {
    SnapType type{SnapType::None};
    Point2D world_pos{};
    Point2D screen_pos{};
    std::string color_hex{"#00ff7f"}; // CAD high-visibility green
    std::vector<ScreenLine2D> stroke_lines;
    std::string tooltip_text;
    Point2D tooltip_screen_pos{};
};

/**
 * @brief High-frame-rate visual snap glyph generator producing screen-space indicator geometries
 * (squares, triangles, circles, crosses, right-angles) and associative tooltips.
 */
class SnapGlyphRenderer {
public:
    /**
     * @brief Generates screen-space glyph line strokes and tooltip layout for a given snap candidate.
     */
    [[nodiscard]] static SnapGlyphGeometry generate_glyph(
        const SnapCandidate& candidate,
        const SketchViewport2D& viewport,
        double glyph_size_px = 12.0
    );
};

} // namespace shunya::cad2d
