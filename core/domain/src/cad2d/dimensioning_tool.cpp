#include "shunya/cad2d/dimensioning_tool.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace shunya::cad2d {

namespace {

using shunya::core::base::math::PI;

[[nodiscard]] double dist(const Point2D& a, const Point2D& b) noexcept {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] std::string format_float(double val, int precision) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << val;
    return ss.str();
}

Arrowhead2D make_arrowhead(const Point2D& tip, const Vec2d& dir, double size = 0.5) {
    Vec2d u = dir.normalized();
    Vec2d perp{-u.y, u.x};
    Point2D base = tip - u * size;
    return Arrowhead2D{
        tip,
        base + perp * (size * 0.35),
        base - perp * (size * 0.35)
    };
}

} // namespace

// =============================================================================
// DimensionCallout
// =============================================================================

std::string DimensionCallout::display_text() const {
    if (!custom_text.empty()) return custom_text;
    std::string s = prefix;
    s += format_float(measured_value, decimal_precision);
    if (!unit_suffix.empty()) {
        s += " " + unit_suffix;
    }
    return s;
}

void DimensionCallout::update_geometry() {
    extension_lines.clear();
    dimension_lines.clear();
    dimension_arcs.clear();
    arrowheads.clear();

    const double arrow_size = 0.4;
    const double ext_overshoot = 0.3;
    const double ext_gap = 0.1;

    switch (type) {
        case DimensionCalloutType::LinearAligned: {
            measured_value = dist(pt_a, pt_b);
            if (measured_value < 1e-12) return;

            Vec2d dir = (pt_b - pt_a) / measured_value;
            Vec2d norm = perp_ccw(dir);
            double sign = (offset_distance >= 0.0) ? 1.0 : -1.0;
            double off = std::abs(offset_distance);

            Point2D dim_a = pt_a + norm * (sign * off);
            Point2D dim_b = pt_b + norm * (sign * off);

            // Extension lines
            Point2D ext_a_start = pt_a + norm * (sign * ext_gap);
            Point2D ext_a_end = dim_a + norm * (sign * ext_overshoot);
            Point2D ext_b_start = pt_b + norm * (sign * ext_gap);
            Point2D ext_b_end = dim_b + norm * (sign * ext_overshoot);

            extension_lines.push_back(LineSegment2D(ext_a_start, ext_a_end));
            extension_lines.push_back(LineSegment2D(ext_b_start, ext_b_end));

            // Dimension line
            dimension_lines.push_back(LineSegment2D(dim_a, dim_b));

            // Arrowheads pointing outward from ends or inward towards measurement
            arrowheads.push_back(make_arrowhead(dim_a, dir, arrow_size));
            arrowheads.push_back(make_arrowhead(dim_b, dir * -1.0, arrow_size));

            // Default text position
            text_position = (dim_a + dim_b) * 0.5 + norm * (sign * 0.25);
            break;
        }

        case DimensionCalloutType::LinearHorizontal: {
            measured_value = std::abs(pt_b.x - pt_a.x);
            double y_ref = std::max(pt_a.y, pt_b.y) + offset_distance;
            Point2D dim_a{pt_a.x, y_ref};
            Point2D dim_b{pt_b.x, y_ref};

            extension_lines.push_back(LineSegment2D(pt_a, dim_a + Point2D{0.0, ext_overshoot}));
            extension_lines.push_back(LineSegment2D(pt_b, dim_b + Point2D{0.0, ext_overshoot}));
            dimension_lines.push_back(LineSegment2D(dim_a, dim_b));

            Vec2d dir = (pt_b.x >= pt_a.x) ? Vec2d{1.0, 0.0} : Vec2d{-1.0, 0.0};
            arrowheads.push_back(make_arrowhead(dim_a, dir, arrow_size));
            arrowheads.push_back(make_arrowhead(dim_b, dir * -1.0, arrow_size));

            text_position = (dim_a + dim_b) * 0.5 + Point2D{0.0, 0.25};
            break;
        }

        case DimensionCalloutType::LinearVertical: {
            measured_value = std::abs(pt_b.y - pt_a.y);
            double x_ref = std::max(pt_a.x, pt_b.x) + offset_distance;
            Point2D dim_a{x_ref, pt_a.y};
            Point2D dim_b{x_ref, pt_b.y};

            extension_lines.push_back(LineSegment2D(pt_a, dim_a + Point2D{ext_overshoot, 0.0}));
            extension_lines.push_back(LineSegment2D(pt_b, dim_b + Point2D{ext_overshoot, 0.0}));
            dimension_lines.push_back(LineSegment2D(dim_a, dim_b));

            Vec2d dir = (pt_b.y >= pt_a.y) ? Vec2d{0.0, 1.0} : Vec2d{0.0, -1.0};
            arrowheads.push_back(make_arrowhead(dim_a, dir, arrow_size));
            arrowheads.push_back(make_arrowhead(dim_b, dir * -1.0, arrow_size));

            text_position = (dim_a + dim_b) * 0.5 + Point2D{0.25, 0.0};
            break;
        }

        case DimensionCalloutType::Angular: {
            Vec2d v1 = pt_a - pt_vertex;
            Vec2d v2 = pt_b - pt_vertex;
            double len1 = v1.length();
            double len2 = v2.length();
            if (len1 < 1e-12 || len2 < 1e-12) return;

            double a1 = std::atan2(v1.y, v1.x);
            double a2 = std::atan2(v2.y, v2.x);
            double sweep = a2 - a1;
            while (sweep < 0.0) sweep += 2.0 * PI;
            while (sweep >= 2.0 * PI) sweep -= 2.0 * PI;

            // Measured value in degrees
            measured_value = sweep * (180.0 / PI);
            unit_suffix = "°";

            double arc_r = std::abs(offset_distance);
            if (arc_r < 0.1) arc_r = 1.0;

            dimension_arcs.push_back(Arc2D(pt_vertex, arc_r, a1, sweep));

            Point2D arc_start = pt_vertex + Point2D{arc_r * std::cos(a1), arc_r * std::sin(a1)};
            Point2D arc_end = pt_vertex + Point2D{arc_r * std::cos(a2), arc_r * std::sin(a2)};

            Vec2d tan_start{-std::sin(a1), std::cos(a1)};
            Vec2d tan_end{-std::sin(a2), std::cos(a2)};

            arrowheads.push_back(make_arrowhead(arc_start, tan_start, arrow_size));
            arrowheads.push_back(make_arrowhead(arc_end, tan_end * -1.0, arrow_size));

            double mid_ang = a1 + sweep * 0.5;
            text_position = pt_vertex + Point2D{(arc_r + 0.3) * std::cos(mid_ang), (arc_r + 0.3) * std::sin(mid_ang)};
            break;
        }

        case DimensionCalloutType::Radial: {
            prefix = "R ";
            Point2D center = pt_vertex;
            Vec2d dir = (text_position - center);
            double d_len = dir.length();
            if (d_len < 1e-12) dir = {1.0, 0.0};
            else dir = dir / d_len;

            Point2D perim = center + dir * measured_value;
            dimension_lines.push_back(LineSegment2D(center, text_position));
            arrowheads.push_back(make_arrowhead(perim, dir, arrow_size));
            break;
        }

        case DimensionCalloutType::Diametral: {
            prefix = "Ø ";
            Point2D center = pt_vertex;
            Vec2d dir = (text_position - center);
            double d_len = dir.length();
            if (d_len < 1e-12) dir = {1.0, 0.0};
            else dir = dir / d_len;

            double r = measured_value * 0.5;
            Point2D p_neg = center - dir * r;
            Point2D p_pos = center + dir * r;

            dimension_lines.push_back(LineSegment2D(p_neg, text_position));
            arrowheads.push_back(make_arrowhead(p_neg, dir * -1.0, arrow_size));
            arrowheads.push_back(make_arrowhead(p_pos, dir, arrow_size));
            break;
        }
    }
}

bool DimensionCallout::recompute_associative(const SketchDocument2D& doc) {
    if (associated_entity_ids.empty()) return false;

    if (type == DimensionCalloutType::LinearAligned ||
        type == DimensionCalloutType::LinearHorizontal ||
        type == DimensionCalloutType::LinearVertical) {
        const auto* ent = doc.find_entity(associated_entity_ids[0]);
        if (!ent || !ent->curve) return false;

        if (auto* line = dynamic_cast<const LineSegment2D*>(ent->curve.get())) {
            pt_a = line->p0;
            pt_b = line->p1;
            update_geometry();
            return true;
        }
    } else if (type == DimensionCalloutType::Radial || type == DimensionCalloutType::Diametral) {
        const auto* ent = doc.find_entity(associated_entity_ids[0]);
        if (!ent || !ent->curve) return false;

        if (auto* circ = dynamic_cast<const Circle2D*>(ent->curve.get())) {
            pt_vertex = circ->center;
            measured_value = (type == DimensionCalloutType::Radial) ? circ->radius : (2.0 * circ->radius);
            update_geometry();
            return true;
        } else if (auto* arc = dynamic_cast<const Arc2D*>(ent->curve.get())) {
            pt_vertex = arc->center;
            measured_value = (type == DimensionCalloutType::Radial) ? arc->radius : (2.0 * arc->radius);
            update_geometry();
            return true;
        }
    }
    return false;
}

// =============================================================================
// PrecisionMeasurementEngine
// =============================================================================

MeasurementResult PrecisionMeasurementEngine::measure_distance(const Point2D& a, const Point2D& b, const std::string& units, int precision) {
    MeasurementResult res;
    res.dx = b.x - a.x;
    res.dy = b.y - a.y;
    res.value = std::sqrt(res.dx * res.dx + res.dy * res.dy);
    res.angle_rad = std::atan2(res.dy, res.dx);
    res.angle_deg = res.angle_rad * (180.0 / PI);
    res.closest_pt = b;

    res.formatted_string = "Distance: " + format_float(res.value, precision) + " " + units +
                           " [dX: " + format_float(res.dx, precision) +
                           ", dY: " + format_float(res.dy, precision) +
                           ", Angle: " + format_float(res.angle_deg, 2) + "°]";
    return res;
}

MeasurementResult PrecisionMeasurementEngine::measure_angle(const Point2D& vertex, const Point2D& ray_a, const Point2D& ray_b, int precision) {
    MeasurementResult res;
    Vec2d v1 = ray_a - vertex;
    Vec2d v2 = ray_b - vertex;
    double d1 = v1.length();
    double d2 = v2.length();
    if (d1 < 1e-12 || d2 < 1e-12) return res;

    double dot = (v1.x * v2.x + v1.y * v2.y) / (d1 * d2);
    dot = std::clamp(dot, -1.0, 1.0);
    res.angle_rad = std::acos(dot);
    res.angle_deg = res.angle_rad * (180.0 / PI);
    res.value = res.angle_deg;

    res.formatted_string = "Included Angle: " + format_float(res.angle_deg, precision) + "° (" +
                           format_float(res.angle_rad, precision + 2) + " rad)";
    return res;
}

MeasurementResult PrecisionMeasurementEngine::measure_curve(const Curve2D& curve, const std::string& units, int precision) {
    MeasurementResult res;
    res.value = curve.total_length();

    if (auto* circ = dynamic_cast<const Circle2D*>(&curve)) {
        res.formatted_string = "Circle: Radius " + format_float(circ->radius, precision) + " " + units +
                               ", Diameter " + format_float(circ->radius * 2.0, precision) + " " + units +
                               ", Circumference " + format_float(res.value, precision) + " " + units;
    } else if (auto* arc = dynamic_cast<const Arc2D*>(&curve)) {
        double deg = std::abs(arc->sweep_angle) * (180.0 / PI);
        res.formatted_string = "Arc: Radius " + format_float(arc->radius, precision) + " " + units +
                               ", Arc Length " + format_float(res.value, precision) + " " + units +
                               ", Sweep " + format_float(deg, 2) + "°";
    } else {
        res.formatted_string = "Curve Length: " + format_float(res.value, precision) + " " + units;
    }
    return res;
}

MeasurementResult PrecisionMeasurementEngine::measure_point_to_curve(const Point2D& pt, const Curve2D& curve, const std::string& units, int precision) {
    MeasurementResult res;
    double t = 0.0;
    res.closest_pt = curve.closest_point(pt, &t);
    res.value = dist(pt, res.closest_pt);

    res.formatted_string = "Point-to-Curve Distance: " + format_float(res.value, precision) + " " + units +
                           " at parameter t=" + format_float(t, 4);
    return res;
}

// =============================================================================
// DimensionManager
// =============================================================================

DimensionCallout& DimensionManager::add_linear_dimension(
    const Point2D& a,
    const Point2D& b,
    double offset,
    DimensionCalloutType type,
    const std::string& entity_a_id,
    const std::string& entity_b_id
) {
    DimensionCallout c;
    c.id = "DIM_" + std::to_string(next_id_++);
    c.type = type;
    c.pt_a = a;
    c.pt_b = b;
    c.offset_distance = offset;
    if (!entity_a_id.empty()) c.associated_entity_ids.push_back(entity_a_id);
    if (!entity_b_id.empty()) c.associated_entity_ids.push_back(entity_b_id);

    c.update_geometry();
    callouts_.push_back(std::move(c));
    return callouts_.back();
}

DimensionCallout& DimensionManager::add_angular_dimension(
    const Point2D& vertex,
    const Point2D& ray_a,
    const Point2D& ray_b,
    double arc_radius,
    const std::string& entity_id
) {
    DimensionCallout c;
    c.id = "DIM_" + std::to_string(next_id_++);
    c.type = DimensionCalloutType::Angular;
    c.pt_vertex = vertex;
    c.pt_a = ray_a;
    c.pt_b = ray_b;
    c.offset_distance = arc_radius;
    if (!entity_id.empty()) c.associated_entity_ids.push_back(entity_id);

    c.update_geometry();
    callouts_.push_back(std::move(c));
    return callouts_.back();
}

DimensionCallout& DimensionManager::add_radial_dimension(
    const std::string& curve_entity_id,
    const Curve2D& curve,
    const Point2D& text_pos
) {
    DimensionCallout c;
    c.id = "DIM_" + std::to_string(next_id_++);
    c.type = DimensionCalloutType::Radial;
    if (!curve_entity_id.empty()) c.associated_entity_ids.push_back(curve_entity_id);
    c.text_position = text_pos;

    if (auto* circ = dynamic_cast<const Circle2D*>(&curve)) {
        c.pt_vertex = circ->center;
        c.measured_value = circ->radius;
    } else if (auto* arc = dynamic_cast<const Arc2D*>(&curve)) {
        c.pt_vertex = arc->center;
        c.measured_value = arc->radius;
    }

    c.update_geometry();
    callouts_.push_back(std::move(c));
    return callouts_.back();
}

DimensionCallout& DimensionManager::add_diametral_dimension(
    const std::string& circle_entity_id,
    const Circle2D& circle,
    const Point2D& text_pos
) {
    DimensionCallout c;
    c.id = "DIM_" + std::to_string(next_id_++);
    c.type = DimensionCalloutType::Diametral;
    if (!circle_entity_id.empty()) c.associated_entity_ids.push_back(circle_entity_id);
    c.text_position = text_pos;
    c.pt_vertex = circle.center;
    c.measured_value = circle.radius * 2.0;

    c.update_geometry();
    callouts_.push_back(std::move(c));
    return callouts_.back();
}

const DimensionCallout* DimensionManager::find_callout(std::string_view id) const noexcept {
    for (const auto& c : callouts_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

bool DimensionManager::remove_callout(std::string_view id) {
    auto it = std::remove_if(callouts_.begin(), callouts_.end(),
                             [&](const DimensionCallout& c) { return c.id == id; });
    if (it != callouts_.end()) {
        callouts_.erase(it, callouts_.end());
        return true;
    }
    return false;
}

void DimensionManager::clear() {
    callouts_.clear();
}

size_t DimensionManager::update_all_associative(const SketchDocument2D& doc) {
    size_t count = 0;
    for (auto& c : callouts_) {
        if (c.recompute_associative(doc)) {
            ++count;
        }
    }
    return count;
}

} // namespace shunya::cad2d
