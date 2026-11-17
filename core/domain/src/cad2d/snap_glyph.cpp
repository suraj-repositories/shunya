#include "shunya/cad2d/snap_glyph.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>

namespace shunya::cad2d {

namespace {

using shunya::core::base::math::PI;

} // namespace

SnapGlyphGeometry SnapGlyphRenderer::generate_glyph(
    const SnapCandidate& candidate,
    const SketchViewport2D& viewport,
    double glyph_size_px
) {
    SnapGlyphGeometry geom;
    geom.type = candidate.type;
    geom.world_pos = candidate.world_pos;
    geom.screen_pos = viewport.world_to_screen(candidate.world_pos);

    double h = glyph_size_px * 0.5;
    Point2D c = geom.screen_pos;

    // Tooltip positioning
    geom.tooltip_text = std::string(snap_type_to_string(candidate.type));
    if (!candidate.entity_id.empty() && candidate.entity_id != "GRID") {
        geom.tooltip_text += " [" + candidate.entity_id + "]";
    }
    geom.tooltip_screen_pos = {c.x + h + 8.0, c.y - h};

    switch (candidate.type) {
        case SnapType::Endpoint: {
            geom.color_hex = "#00ff7f"; // Spring green
            Point2D p0{c.x - h, c.y - h};
            Point2D p1{c.x + h, c.y - h};
            Point2D p2{c.x + h, c.y + h};
            Point2D p3{c.x - h, c.y + h};
            geom.stroke_lines.push_back({p0, p1, true});
            geom.stroke_lines.push_back({p1, p2, true});
            geom.stroke_lines.push_back({p2, p3, true});
            geom.stroke_lines.push_back({p3, p0, true});
            break;
        }

        case SnapType::Midpoint: {
            geom.color_hex = "#00e5ff"; // Cyan
            Point2D top{c.x, c.y - h};
            Point2D br{c.x + h, c.y + h};
            Point2D bl{c.x - h, c.y + h};
            geom.stroke_lines.push_back({top, br, true});
            geom.stroke_lines.push_back({br, bl, true});
            geom.stroke_lines.push_back({bl, top, true});
            break;
        }

        case SnapType::Center: {
            geom.color_hex = "#ffea00"; // Bright yellow
            const size_t segs = 12;
            for (size_t i = 0; i < segs; ++i) {
                double a0 = 2.0 * PI * static_cast<double>(i) / static_cast<double>(segs);
                double a1 = 2.0 * PI * static_cast<double>(i + 1) / static_cast<double>(segs);
                Point2D p0{c.x + h * std::cos(a0), c.y + h * std::sin(a0)};
                Point2D p1{c.x + h * std::cos(a1), c.y + h * std::sin(a1)};
                geom.stroke_lines.push_back({p0, p1, true});
            }
            // Center crosshair
            geom.stroke_lines.push_back({{c.x - h * 0.4, c.y}, {c.x + h * 0.4, c.y}, false});
            geom.stroke_lines.push_back({{c.x, c.y - h * 0.4}, {c.x, c.y + h * 0.4}, false});
            break;
        }

        case SnapType::Quadrant: {
            geom.color_hex = "#ff9100"; // Orange
            Point2D top{c.x, c.y - h};
            Point2D right{c.x + h, c.y};
            Point2D bottom{c.x, c.y + h};
            Point2D left{c.x - h, c.y};
            geom.stroke_lines.push_back({top, right, true});
            geom.stroke_lines.push_back({right, bottom, true});
            geom.stroke_lines.push_back({bottom, left, true});
            geom.stroke_lines.push_back({left, top, true});
            break;
        }

        case SnapType::Intersection: {
            geom.color_hex = "#ff1744"; // Vivid red
            geom.stroke_lines.push_back({{c.x - h, c.y - h}, {c.x + h, c.y + h}, true});
            geom.stroke_lines.push_back({{c.x - h, c.y + h}, {c.x + h, c.y - h}, true});
            break;
        }

        case SnapType::Tangent: {
            geom.color_hex = "#d500f9"; // Magenta
            const size_t segs = 12;
            double r = h * 0.8;
            for (size_t i = 0; i < segs; ++i) {
                double a0 = 2.0 * PI * static_cast<double>(i) / static_cast<double>(segs);
                double a1 = 2.0 * PI * static_cast<double>(i + 1) / static_cast<double>(segs);
                Point2D p0{c.x + r * std::cos(a0), c.y + r * std::sin(a0)};
                Point2D p1{c.x + r * std::cos(a1), c.y + r * std::sin(a1)};
                geom.stroke_lines.push_back({p0, p1, true});
            }
            // Tangent bar at top
            geom.stroke_lines.push_back({{c.x - h, c.y - r}, {c.x + h, c.y - r}, true});
            break;
        }

        case SnapType::Perpendicular: {
            geom.color_hex = "#651fff"; // Deep purple
            // Right-angle symbol _|
            Point2D p_corner{c.x - h, c.y + h};
            Point2D p_top{c.x - h, c.y - h};
            Point2D p_right{c.x + h, c.y + h};
            geom.stroke_lines.push_back({p_top, p_corner, true});
            geom.stroke_lines.push_back({p_corner, p_right, true});

            // Inner corner
            Point2D m_top{c.x - h, c.y};
            Point2D m_corner{c.x, c.y};
            Point2D m_right{c.x, c.y + h};
            geom.stroke_lines.push_back({m_top, m_corner, false});
            geom.stroke_lines.push_back({m_corner, m_right, false});
            break;
        }

        case SnapType::Grid: {
            geom.color_hex = "#76ff03"; // Lime green
            double s = h * 0.7;
            geom.stroke_lines.push_back({{c.x - s, c.y}, {c.x + s, c.y}, true});
            geom.stroke_lines.push_back({{c.x, c.y - s}, {c.x, c.y + s}, true});
            break;
        }

        default:
            break;
    }

    return geom;
}

} // namespace shunya::cad2d
