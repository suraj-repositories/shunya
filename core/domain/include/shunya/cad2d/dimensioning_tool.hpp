#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/construction_layer.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace shunya::cad2d {

enum class DimensionCalloutType {
    LinearAligned,
    LinearHorizontal,
    LinearVertical,
    Angular,
    Radial,
    Diametral
};

struct Arrowhead2D {
    Point2D tip{};
    Point2D base_left{};
    Point2D base_right{};
};

struct MeasurementResult {
    double value{0.0};
    double dx{0.0};
    double dy{0.0};
    double angle_rad{0.0};
    double angle_deg{0.0};
    Point2D closest_pt{};
    std::string formatted_string;
};

/**
 * @brief Represents an associative dimension annotation containing dimension lines,
 * extension witness lines, arrowheads, and formatted measurement text.
 */
class DimensionCallout {
public:
    std::string id;
    DimensionCalloutType type{DimensionCalloutType::LinearAligned};
    std::vector<std::string> associated_entity_ids;

    // Defining points
    Point2D pt_a{};
    Point2D pt_b{};
    Point2D pt_vertex{}; // Used for angular dimensions
    Point2D text_position{};
    double offset_distance{1.0}; // Offset for dimension line from measured points

    // Value and formatting
    double measured_value{0.0};
    std::string unit_suffix{"mm"};
    int decimal_precision{3};
    std::string prefix{};
    std::string custom_text{};

    // Generated graphical components for rendering
    std::vector<LineSegment2D> extension_lines;
    std::vector<LineSegment2D> dimension_lines;
    std::vector<Arc2D> dimension_arcs;
    std::vector<Arrowhead2D> arrowheads;

    [[nodiscard]] std::string display_text() const;

    /**
     * @brief Regenerates extension lines, dimension lines, and arrowheads based on current points and offset.
     */
    void update_geometry();

    /**
     * @brief Re-evaluates measurement from associated entities in SketchDocument2D.
     * @return true if updated successfully, false if referenced entity was missing or invalid.
     */
    bool recompute_associative(const SketchDocument2D& doc);
};

/**
 * @brief High-precision analytical measurement engine for distance, angle, and curve queries.
 */
class PrecisionMeasurementEngine {
public:
    [[nodiscard]] static MeasurementResult measure_distance(const Point2D& a, const Point2D& b, const std::string& units = "mm", int precision = 3);
    [[nodiscard]] static MeasurementResult measure_angle(const Point2D& vertex, const Point2D& ray_a, const Point2D& ray_b, int precision = 2);
    [[nodiscard]] static MeasurementResult measure_curve(const Curve2D& curve, const std::string& units = "mm", int precision = 3);
    [[nodiscard]] static MeasurementResult measure_point_to_curve(const Point2D& pt, const Curve2D& curve, const std::string& units = "mm", int precision = 3);
};

/**
 * @brief Manager and interactive controller for associative dimension callouts on a 2D sketch.
 */
class DimensionManager {
public:
    DimensionCallout& add_linear_dimension(
        const Point2D& a,
        const Point2D& b,
        double offset = 1.0,
        DimensionCalloutType type = DimensionCalloutType::LinearAligned,
        const std::string& entity_a_id = "",
        const std::string& entity_b_id = ""
    );

    DimensionCallout& add_angular_dimension(
        const Point2D& vertex,
        const Point2D& ray_a,
        const Point2D& ray_b,
        double arc_radius = 2.0,
        const std::string& entity_id = ""
    );

    DimensionCallout& add_radial_dimension(
        const std::string& curve_entity_id,
        const Curve2D& curve,
        const Point2D& text_pos
    );

    DimensionCallout& add_diametral_dimension(
        const std::string& circle_entity_id,
        const Circle2D& circle,
        const Point2D& text_pos
    );

    [[nodiscard]] const std::vector<DimensionCallout>& callouts() const noexcept { return callouts_; }
    [[nodiscard]] std::vector<DimensionCallout>& mutable_callouts() noexcept { return callouts_; }
    [[nodiscard]] const DimensionCallout* find_callout(std::string_view id) const noexcept;
    bool remove_callout(std::string_view id);
    void clear();

    /**
     * @brief Updates all associative dimensions whose linked entities have modified geometries.
     */
    size_t update_all_associative(const SketchDocument2D& doc);

private:
    uint64_t next_id_{1};
    std::vector<DimensionCallout> callouts_;
};

} // namespace shunya::cad2d
