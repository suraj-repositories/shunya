#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/construction_layer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace shunya::cad2d {

enum class TextHAlign {
    Left,
    Center,
    Right
};

enum class TextVAlign {
    Baseline,
    Bottom,
    Middle,
    Top
};

struct TextProperties {
    std::string content{"TEXT"};
    Point2D position{0.0, 0.0};
    double font_size{5.0};             // Height in world units (e.g. mm)
    double rotation_rad{0.0};          // Orientation angle
    double char_spacing_ratio{1.1};    // Character pitch multiplier
    double line_spacing_ratio{1.4};    // Line pitch multiplier
    TextHAlign h_align{TextHAlign::Left};
    TextVAlign v_align{TextVAlign::Baseline};
};

/**
 * @brief High-precision vector typography engine generating stroke and outline curves from engineering fonts.
 */
class VectorFontEngine {
public:
    /**
     * @brief Generates vector strokes (LineSegment2D and Polyline2D) for a given text string.
     */
    [[nodiscard]] static std::vector<std::shared_ptr<Curve2D>> generate_vector_strokes(const TextProperties& props);

    /**
     * @brief Computes tight axis-aligned or oriented bounding box for text layout.
     */
    [[nodiscard]] static BBox2D compute_text_bounds(const TextProperties& props);

    /**
     * @brief Normalized vector stroke data for a single ASCII character in [0, 1] unit box.
     */
    [[nodiscard]] static std::vector<std::vector<Point2D>> get_glyph_strokes(char c);
};

/**
 * @brief Persistent vector text entity retaining editable typography properties and regenerating curves on change.
 */
class TextEntity2D {
public:
    std::string id;
    TextProperties properties;
    std::string layer{"0"};

    explicit TextEntity2D(TextProperties props = {}) : properties(std::move(props)) {}

    [[nodiscard]] std::vector<std::shared_ptr<Curve2D>> generate_curves() const {
        return VectorFontEngine::generate_vector_strokes(properties);
    }

    [[nodiscard]] BBox2D bounding_box() const {
        return VectorFontEngine::compute_text_bounds(properties);
    }
};

/**
 * @brief Interactive placement and editing tool for 2D vector text annotations.
 */
class VectorTextTool {
public:
    TextProperties current_properties;
    bool is_placing{false};

    void start_placement(Point2D pos, std::string text = "TEXT", double size = 5.0) {
        current_properties.position = pos;
        current_properties.content = std::move(text);
        current_properties.font_size = size;
        is_placing = true;
    }

    void update_cursor(Point2D pos) {
        if (is_placing) {
            current_properties.position = pos;
        }
    }

    [[nodiscard]] std::vector<std::shared_ptr<Curve2D>> preview_curves() const {
        if (!is_placing || current_properties.content.empty()) return {};
        return VectorFontEngine::generate_vector_strokes(current_properties);
    }

    std::vector<std::string> commit_to_document(SketchDocument2D& doc, bool is_construction = false, const std::string& layer = "0") {
        if (!is_placing || current_properties.content.empty()) return {};
        auto curves = VectorFontEngine::generate_vector_strokes(current_properties);
        std::vector<std::string> entity_ids;
        entity_ids.reserve(curves.size());
        for (auto& c : curves) {
            entity_ids.push_back(doc.add_entity(std::move(c), is_construction, layer));
        }
        is_placing = false;
        return entity_ids;
    }
};

} // namespace shunya::cad2d
