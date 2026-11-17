#include "shunya/cad2d/vector_text_tool.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <sstream>
#include <cctype>

namespace shunya::cad2d {

namespace {

using shunya::core::base::math::PI;

Point2D transform_pt(const Point2D& p, const Point2D& origin, double cos_r, double sin_r, double scale) {
    double sx = p.x * scale;
    double sy = p.y * scale;
    return Point2D{
        origin.x + (sx * cos_r - sy * sin_r),
        origin.y + (sx * sin_r + sy * cos_r)
    };
}

} // namespace

std::vector<std::vector<Point2D>> VectorFontEngine::get_glyph_strokes(char c) {
    // Normalization box: width ~0.6, height 1.0
    char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    switch (up) {
        case 'A':
            return {
                {{0.0, 0.0}, {0.3, 1.0}, {0.6, 0.0}},
                {{0.12, 0.4}, {0.48, 0.4}}
            };
        case 'B':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.45, 1.0}, {0.55, 0.85}, {0.45, 0.5}, {0.0, 0.5}},
                {{0.0, 0.5}, {0.48, 0.5}, {0.6, 0.3}, {0.48, 0.0}, {0.0, 0.0}}
            };
        case 'C':
            return {
                {{0.6, 0.85}, {0.45, 1.0}, {0.15, 1.0}, {0.0, 0.75}, {0.0, 0.25}, {0.15, 0.0}, {0.45, 0.0}, {0.6, 0.15}}
            };
        case 'D':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.35, 1.0}, {0.6, 0.75}, {0.6, 0.25}, {0.35, 0.0}, {0.0, 0.0}}
            };
        case 'E':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.55, 1.0}},
                {{0.0, 0.5}, {0.4, 0.5}},
                {{0.0, 0.0}, {0.55, 0.0}}
            };
        case 'F':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.55, 1.0}},
                {{0.0, 0.5}, {0.4, 0.5}}
            };
        case 'G':
            return {
                {{0.6, 0.85}, {0.45, 1.0}, {0.15, 1.0}, {0.0, 0.75}, {0.0, 0.25}, {0.15, 0.0}, {0.45, 0.0}, {0.6, 0.15}, {0.6, 0.5}, {0.35, 0.5}}
            };
        case 'H':
            return {
                {{0.0, 0.0}, {0.0, 1.0}},
                {{0.6, 0.0}, {0.6, 1.0}},
                {{0.0, 0.5}, {0.6, 0.5}}
            };
        case 'I':
            return {
                {{0.1, 1.0}, {0.5, 1.0}},
                {{0.3, 1.0}, {0.3, 0.0}},
                {{0.1, 0.0}, {0.5, 0.0}}
            };
        case 'J':
            return {
                {{0.45, 1.0}, {0.45, 0.2}, {0.35, 0.0}, {0.15, 0.0}, {0.0, 0.15}, {0.0, 0.35}}
            };
        case 'K':
            return {
                {{0.0, 0.0}, {0.0, 1.0}},
                {{0.55, 1.0}, {0.0, 0.45}, {0.6, 0.0}}
            };
        case 'L':
            return {
                {{0.0, 1.0}, {0.0, 0.0}, {0.55, 0.0}}
            };
        case 'M':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.3, 0.3}, {0.6, 1.0}, {0.6, 0.0}}
            };
        case 'N':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.6, 0.0}, {0.6, 1.0}}
            };
        case 'O':
            return {
                {{0.15, 0.0}, {0.45, 0.0}, {0.6, 0.25}, {0.6, 0.75}, {0.45, 1.0}, {0.15, 1.0}, {0.0, 0.75}, {0.0, 0.25}, {0.15, 0.0}}
            };
        case 'P':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.45, 1.0}, {0.6, 0.8}, {0.45, 0.5}, {0.0, 0.5}}
            };
        case 'Q':
            return {
                {{0.15, 0.0}, {0.45, 0.0}, {0.6, 0.25}, {0.6, 0.75}, {0.45, 1.0}, {0.15, 1.0}, {0.0, 0.75}, {0.0, 0.25}, {0.15, 0.0}},
                {{0.35, 0.25}, {0.65, -0.05}}
            };
        case 'R':
            return {
                {{0.0, 0.0}, {0.0, 1.0}, {0.45, 1.0}, {0.6, 0.8}, {0.45, 0.5}, {0.0, 0.5}},
                {{0.3, 0.5}, {0.6, 0.0}}
            };
        case 'S':
            return {
                {{0.55, 0.85}, {0.4, 1.0}, {0.15, 1.0}, {0.0, 0.8}, {0.1, 0.55}, {0.5, 0.45}, {0.6, 0.2}, {0.45, 0.0}, {0.15, 0.0}, {0.0, 0.15}}
            };
        case 'T':
            return {
                {{0.0, 1.0}, {0.6, 1.0}},
                {{0.3, 1.0}, {0.3, 0.0}}
            };
        case 'U':
            return {
                {{0.0, 1.0}, {0.0, 0.25}, {0.15, 0.0}, {0.45, 0.0}, {0.6, 0.25}, {0.6, 1.0}}
            };
        case 'V':
            return {
                {{0.0, 1.0}, {0.3, 0.0}, {0.6, 1.0}}
            };
        case 'W':
            return {
                {{0.0, 1.0}, {0.15, 0.0}, {0.3, 0.65}, {0.45, 0.0}, {0.6, 1.0}}
            };
        case 'X':
            return {
                {{0.0, 0.0}, {0.6, 1.0}},
                {{0.0, 1.0}, {0.6, 0.0}}
            };
        case 'Y':
            return {
                {{0.0, 1.0}, {0.3, 0.5}, {0.6, 1.0}},
                {{0.3, 0.5}, {0.3, 0.0}}
            };
        case 'Z':
            return {
                {{0.0, 1.0}, {0.6, 1.0}, {0.0, 0.0}, {0.6, 0.0}}
            };
        case '0':
            return {
                {{0.15, 0.0}, {0.45, 0.0}, {0.6, 0.25}, {0.6, 0.75}, {0.45, 1.0}, {0.15, 1.0}, {0.0, 0.75}, {0.0, 0.25}, {0.15, 0.0}},
                {{0.1, 0.2}, {0.5, 0.8}}
            };
        case '1':
            return {
                {{0.1, 0.75}, {0.3, 1.0}, {0.3, 0.0}},
                {{0.1, 0.0}, {0.5, 0.0}}
            };
        case '2':
            return {
                {{0.05, 0.8}, {0.2, 1.0}, {0.45, 1.0}, {0.6, 0.8}, {0.6, 0.6}, {0.0, 0.0}, {0.6, 0.0}}
            };
        case '3':
            return {
                {{0.05, 0.85}, {0.2, 1.0}, {0.5, 1.0}, {0.6, 0.75}, {0.4, 0.5}, {0.6, 0.25}, {0.5, 0.0}, {0.2, 0.0}, {0.05, 0.15}},
                {{0.2, 0.5}, {0.4, 0.5}}
            };
        case '4':
            return {
                {{0.45, 0.0}, {0.45, 1.0}, {0.0, 0.3}, {0.6, 0.3}}
            };
        case '5':
            return {
                {{0.55, 1.0}, {0.05, 1.0}, {0.05, 0.55}, {0.4, 0.55}, {0.6, 0.35}, {0.55, 0.0}, {0.15, 0.0}, {0.0, 0.15}}
            };
        case '6':
            return {
                {{0.5, 0.9}, {0.2, 0.6}, {0.0, 0.35}, {0.0, 0.2}, {0.15, 0.0}, {0.45, 0.0}, {0.6, 0.2}, {0.6, 0.35}, {0.45, 0.5}, {0.05, 0.5}}
            };
        case '7':
            return {
                {{0.0, 1.0}, {0.6, 1.0}, {0.2, 0.0}}
            };
        case '8':
            return {
                {{0.2, 0.5}, {0.05, 0.75}, {0.2, 1.0}, {0.4, 1.0}, {0.55, 0.75}, {0.2, 0.5}, {0.0, 0.25}, {0.2, 0.0}, {0.4, 0.0}, {0.6, 0.25}, {0.2, 0.5}}
            };
        case '9':
            return {
                {{0.55, 0.5}, {0.15, 0.5}, {0.0, 0.65}, {0.0, 0.8}, {0.15, 1.0}, {0.45, 1.0}, {0.6, 0.8}, {0.6, 0.65}, {0.4, 0.4}, {0.1, 0.1}}
            };
        case '+':
            return {
                {{0.1, 0.5}, {0.5, 0.5}},
                {{0.3, 0.2}, {0.3, 0.8}}
            };
        case '-':
            return {
                {{0.1, 0.5}, {0.5, 0.5}}
            };
        case '/':
            return {
                {{0.05, 0.0}, {0.55, 1.0}}
            };
        case '=':
            return {
                {{0.1, 0.6}, {0.5, 0.6}},
                {{0.1, 0.4}, {0.5, 0.4}}
            };
        case '.':
            return {
                {{0.25, 0.0}, {0.35, 0.0}, {0.35, 0.1}, {0.25, 0.1}, {0.25, 0.0}}
            };
        case '_':
            return {
                {{0.0, -0.1}, {0.6, -0.1}}
            };
        default:
            return {};
    }
}

std::vector<std::shared_ptr<Curve2D>> VectorFontEngine::generate_vector_strokes(const TextProperties& props) {
    std::vector<std::shared_ptr<Curve2D>> result;
    if (props.content.empty() || props.font_size <= 1e-9) return result;

    // Split text into lines
    std::vector<std::string> lines;
    std::stringstream ss(props.content);
    std::string item;
    while (std::getline(ss, item, '\n')) {
        lines.push_back(item);
    }
    if (lines.empty()) return result;

    double char_w = 0.6 * props.char_spacing_ratio;
    double line_h = props.line_spacing_ratio;

    // Compute layout vertical bounds
    double total_h = static_cast<double>(lines.size() - 1) * line_h + 1.0;
    double y_anchor_offset = 0.0;
    switch (props.v_align) {
        case TextVAlign::Baseline:
            y_anchor_offset = 0.0;
            break;
        case TextVAlign::Bottom:
            y_anchor_offset = -0.0;
            break;
        case TextVAlign::Middle:
            y_anchor_offset = total_h * 0.5;
            break;
        case TextVAlign::Top:
            y_anchor_offset = total_h;
            break;
    }

    double cos_r = std::cos(props.rotation_rad);
    double sin_r = std::sin(props.rotation_rad);

    for (size_t line_idx = 0; line_idx < lines.size(); ++line_idx) {
        const auto& line = lines[line_idx];
        double line_w = static_cast<double>(line.size()) * char_w;
        double x_offset = 0.0;

        switch (props.h_align) {
            case TextHAlign::Left:
                x_offset = 0.0;
                break;
            case TextHAlign::Center:
                x_offset = -line_w * 0.5;
                break;
            case TextHAlign::Right:
                x_offset = -line_w;
                break;
        }

        double line_y = -(static_cast<double>(line_idx) * line_h) + y_anchor_offset;

        for (size_t c_idx = 0; c_idx < line.size(); ++c_idx) {
            char c = line[c_idx];
            if (c == ' ') continue;

            auto glyph = get_glyph_strokes(c);
            double glyph_x = x_offset + static_cast<double>(c_idx) * char_w;

            for (const auto& stroke : glyph) {
                if (stroke.size() < 2) continue;

                std::vector<Point2D> transformed_pts;
                transformed_pts.reserve(stroke.size());
                for (const auto& pt : stroke) {
                    Point2D layout_pt{glyph_x + pt.x, line_y + pt.y};
                    transformed_pts.push_back(transform_pt(layout_pt, props.position, cos_r, sin_r, props.font_size));
                }

                if (transformed_pts.size() == 2) {
                    result.push_back(std::make_shared<LineSegment2D>(transformed_pts[0], transformed_pts[1]));
                } else {
                    result.push_back(std::make_shared<Polyline2D>(std::move(transformed_pts), false));
                }
            }
        }
    }

    return result;
}

BBox2D VectorFontEngine::compute_text_bounds(const TextProperties& props) {
    BBox2D bbox;
    auto curves = generate_vector_strokes(props);
    for (const auto& c : curves) {
        bbox.expand(c->bounding_box());
    }
    return bbox;
}

} // namespace shunya::cad2d
