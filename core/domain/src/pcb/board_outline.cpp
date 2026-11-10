#include "shunya/pcb/board_outline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shunya::pcb {

namespace {

bool point_in_polygon(const core::base::math::Vector2<double>& pt,
                      const std::vector<core::base::math::Vector2<double>>& poly) noexcept {
    if (poly.size() < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const auto& a = poly[i];
        const auto& b = poly[j];
        const bool cross = ((a.y > pt.y) != (b.y > pt.y)) &&
                           (pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y) + a.x);
        if (cross) inside = !inside;
    }
    return inside;
}

double point_to_polygon_edge_dist(const core::base::math::Vector2<double>& pt,
                                  const std::vector<core::base::math::Vector2<double>>& poly) noexcept {
    if (poly.size() < 2) return std::numeric_limits<double>::infinity();
    double best = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < poly.size(); ++i) {
        const auto& a = poly[i];
        const auto& b = poly[(i + 1) % poly.size()];
        const auto ab = b - a;
        const double l2 = ab.dot(ab);
        double t = (l2 > 1e-12) ? ((pt - a).dot(ab) / l2) : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        const auto proj = a + ab * t;
        best = std::min(best, (pt - proj).length());
    }
    return best;
}

} // namespace

void BoardOutline::set_outer_contour(std::vector<core::base::math::Vector2<double>> poly_mm) {
    outer_contour_ = std::move(poly_mm);
}

BoardOutline BoardOutline::make_rectangle(double width_mm, double height_mm) {
    BoardOutline bo;
    bo.set_outer_contour({
        {0.0, 0.0},
        {width_mm, 0.0},
        {width_mm, height_mm},
        {0.0, height_mm}
    });
    return bo;
}

void BoardOutline::add_cutout_polygon(std::vector<core::base::math::Vector2<double>> cutout_mm) {
    cutouts_.push_back(std::move(cutout_mm));
}

void BoardOutline::add_mounting_hole(MountingHole hole) {
    holes_.push_back(hole);
}

void BoardOutline::add_keepout_zone(KeepoutZone zone) {
    keepouts_.push_back(std::move(zone));
}

bool BoardOutline::contains_copper_region(const core::base::math::Vector2<double>& pt_mm) const noexcept {
    if (!point_in_polygon(pt_mm, outer_contour_)) return false;
    for (const auto& cut : cutouts_) {
        if (point_in_polygon(pt_mm, cut)) return false;
    }
    for (const auto& hole : holes_) {
        const double r = 0.5 * hole.drill_diameter_mm;
        if ((pt_mm - hole.center_mm).length() <= r) return false;
    }
    return true;
}

double BoardOutline::min_edge_clearance_mm(const core::base::math::Vector2<double>& pt_mm) const noexcept {
    double best = point_to_polygon_edge_dist(pt_mm, outer_contour_);
    for (const auto& cut : cutouts_) {
        best = std::min(best, point_to_polygon_edge_dist(pt_mm, cut));
    }
    for (const auto& hole : holes_) {
        const double r = 0.5 * hole.drill_diameter_mm + hole.keepout_annular_margin_mm;
        best = std::min(best, (pt_mm - hole.center_mm).length() - r);
    }
    return best;
}

bool BoardOutline::is_component_placement_allowed(const core::base::math::Vector2<double>& pt_mm,
                                                  double radius_mm,
                                                  double edge_clearance_mm) const noexcept {
    if (!contains_copper_region(pt_mm)) return false;
    if (min_edge_clearance_mm(pt_mm) < (radius_mm + edge_clearance_mm)) return false;
    for (const auto& kz : keepouts_) {
        if (!kz.block_components) continue;
        if (point_in_polygon(pt_mm, kz.polygon_mm) ||
            point_to_polygon_edge_dist(pt_mm, kz.polygon_mm) < radius_mm) {
            return false;
        }
    }
    return true;
}

bool BoardOutline::is_trace_point_allowed(const core::base::math::Vector2<double>& pt_mm,
                                          double trace_half_width_mm,
                                          double edge_clearance_mm) const noexcept {
    if (!contains_copper_region(pt_mm)) return false;
    if (min_edge_clearance_mm(pt_mm) < (trace_half_width_mm + edge_clearance_mm)) return false;
    for (const auto& kz : keepouts_) {
        if (!kz.block_traces) continue;
        if (point_in_polygon(pt_mm, kz.polygon_mm) ||
            point_to_polygon_edge_dist(pt_mm, kz.polygon_mm) < trace_half_width_mm) {
            return false;
        }
    }
    return true;
}

} // namespace shunya::pcb
