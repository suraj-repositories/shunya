#include "shunya/cad2d/schematic_wire_router.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

namespace {

bool is_near(const Point2D& a, const Point2D& b, double tol = 1e-4) noexcept {
    return std::abs(a.x - b.x) <= tol && std::abs(a.y - b.y) <= tol;
}

double dist_to_segment_sq(const Point2D& p, const Point2D& a, const Point2D& b) noexcept {
    const double l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
    if (l2 < 1e-12) return (p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y);
    const double t = std::clamp(((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2, 0.0, 1.0);
    const double proj_x = a.x + t * (b.x - a.x);
    const double proj_y = a.y + t * (b.y - a.y);
    return (p.x - proj_x) * (p.x - proj_x) + (p.y - proj_y) * (p.y - proj_y);
}

bool point_on_segment(const Point2D& p, const Point2D& a, const Point2D& b, double tol = 1e-3) noexcept {
    return dist_to_segment_sq(p, a, b) <= tol * tol;
}

bool point_interior_to_segment(const Point2D& p, const Point2D& a, const Point2D& b, double tol = 1e-3) noexcept {
    if (is_near(p, a, tol) || is_near(p, b, tol)) return false;
    return point_on_segment(p, a, b, tol);
}

} // namespace

SchematicWireRouter::SchematicWireRouter(SchematicDraftingEngine* symbol_engine)
    : symbol_engine_(symbol_engine) {}

std::vector<Point2D> SchematicWireRouter::calculate_route(
    Point2D start,
    Point2D target,
    WireRouteStyle style,
    const std::vector<BBox2D>& obstacles
) {
    if (is_near(start, target)) return {start};

    if (style == WireRouteStyle::Direct) {
        return {start, target};
    }

    if (style == WireRouteStyle::Orthogonal_HV) {
        if (std::abs(start.x - target.x) < 1e-4 || std::abs(start.y - target.y) < 1e-4) {
            return {start, target};
        }
        Point2D knee = {target.x, start.y};
        // Check obstacle collision on knee
        bool collided = false;
        for (const auto& obs : obstacles) {
            if (obs.contains(knee, 0.5)) {
                collided = true;
                break;
            }
        }
        if (collided) {
            // Try alternate knee (Vertical then Horizontal)
            Point2D alt_knee = {start.x, target.y};
            return {start, alt_knee, target};
        }
        return {start, knee, target};
    }

    if (style == WireRouteStyle::Orthogonal_VH) {
        if (std::abs(start.x - target.x) < 1e-4 || std::abs(start.y - target.y) < 1e-4) {
            return {start, target};
        }
        Point2D knee = {start.x, target.y};
        return {start, knee, target};
    }

    if (style == WireRouteStyle::Octilinear_45) {
        const double dx = target.x - start.x;
        const double dy = target.y - start.y;
        if (std::abs(dx) < 1e-4 || std::abs(dy) < 1e-4 || std::abs(std::abs(dx) - std::abs(dy)) < 1e-4) {
            return {start, target};
        }

        const double d = std::min(std::abs(dx), std::abs(dy));
        const double sx = (dx > 0.0) ? 1.0 : -1.0;
        const double sy = (dy > 0.0) ? 1.0 : -1.0;

        Point2D mid = {start.x + d * sx, start.y + d * sy};
        return {start, mid, target};
    }

    return {start, target};
}

std::string SchematicWireRouter::add_wire(
    SketchDocument2D& doc,
    const std::vector<Point2D>& waypoints,
    std::string preferred_net_name
) {
    if (waypoints.size() < 2) return {};

    SchematicWire wire;
    wire.wire_id = "sch_wire_" + std::to_string(next_wire_idx_++);
    wire.points = waypoints;

    // Resolve or adopt net name from intersecting wire or pin
    std::string resolved_net = std::move(preferred_net_name);
    if (resolved_net.empty()) {
        for (const auto& pt : waypoints) {
            std::string existing_net = net_at_point(pt, 0.5);
            if (!existing_net.empty()) {
                resolved_net = std::move(existing_net);
                break;
            }
        }
    }
    if (resolved_net.empty()) {
        resolved_net = "Net_" + std::to_string(next_net_idx_++);
    }
    wire.net_name = resolved_net;

    // Create segments in document
    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_WIRES);
    for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
        if (!is_near(waypoints[i], waypoints[i + 1])) {
            auto seg = std::make_shared<LineSegment2D>(waypoints[i], waypoints[i + 1]);
            std::string eid = doc.add_entity(seg, true, layer);
            wire.entity_ids.push_back(std::move(eid));
        }
    }

    if (wire.entity_ids.empty()) return {};

    wires_.push_back(std::move(wire));
    update_junctions(doc);
    return wires_.back().wire_id;
}

bool SchematicWireRouter::remove_wire(SketchDocument2D& doc, std::string_view wire_id) {
    auto it = std::find_if(wires_.begin(), wires_.end(), [&](const SchematicWire& w) {
        return w.wire_id == wire_id;
    });
    if (it == wires_.end()) return false;

    for (const auto& eid : it->entity_ids) {
        doc.remove_entity(eid);
    }
    wires_.erase(it);
    update_junctions(doc);
    return true;
}

void SchematicWireRouter::update_junctions(SketchDocument2D& doc) {
    // 1. Remove existing junction dot graphics from document
    for (const auto& j : junctions_) {
        if (!j.dot_entity_id.empty()) {
            doc.remove_entity(j.dot_entity_id);
        }
    }
    junctions_.clear();

    if (wires_.size() < 2) return;

    // Gather all wire line segments: {p0, p1, wire_id}
    struct SegInfo {
        Point2D p0;
        Point2D p1;
        std::string wire_id;
    };
    std::vector<SegInfo> all_segs;
    for (const auto& w : wires_) {
        for (size_t i = 0; i + 1 < w.points.size(); ++i) {
            all_segs.push_back({w.points[i], w.points[i + 1], w.wire_id});
        }
    }

    // Collect candidate junction positions:
    // Case A: End of one wire segment lands interior to another wire segment (T-junction)
    // Case B: 3 or more wire segments meet at the exact same vertex point
    std::vector<Point2D> candidate_pts;

    // Check Case A: T-junctions
    for (const auto& w : wires_) {
        const Point2D ends[2] = {w.points.front(), w.points.back()};
        for (const auto& end_pt : ends) {
            for (const auto& seg : all_segs) {
                if (seg.wire_id != w.wire_id && point_interior_to_segment(end_pt, seg.p0, seg.p1, 0.1)) {
                    bool already = false;
                    for (const auto& cp : candidate_pts) {
                        if (is_near(cp, end_pt, 0.1)) { already = true; break; }
                    }
                    if (!already) candidate_pts.push_back(end_pt);
                }
            }
        }
    }

    // Check Case B: 3+ segment ends meeting at the same vertex
    std::vector<Point2D> vertices;
    for (const auto& s : all_segs) {
        vertices.push_back(s.p0);
        vertices.push_back(s.p1);
    }
    for (const auto& v : vertices) {
        int count = 0;
        for (const auto& s : all_segs) {
            if (is_near(v, s.p0, 0.1) || is_near(v, s.p1, 0.1)) {
                ++count;
            }
        }
        if (count >= 3) {
            bool already = false;
            for (const auto& cp : candidate_pts) {
                if (is_near(cp, v, 0.1)) { already = true; break; }
            }
            if (!already) candidate_pts.push_back(v);
        }
    }

    // Create JunctionDot for each valid junction point
    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_WIRES);
    for (const auto& pt : candidate_pts) {
        JunctionDot j;
        j.junction_id = "sch_junc_" + std::to_string(next_junction_idx_++);
        j.position = pt;
        j.radius_mm = 0.8;
        j.net_name = net_at_point(pt, 0.5);

        for (const auto& s : all_segs) {
            if (point_on_segment(pt, s.p0, s.p1, 0.1)) {
                if (std::find(j.connected_wire_ids.begin(), j.connected_wire_ids.end(), s.wire_id) == j.connected_wire_ids.end()) {
                    j.connected_wire_ids.push_back(s.wire_id);
                }
            }
        }

        // Add visual circle indicator to document
        auto dot_circ = std::make_shared<Circle2D>(pt, j.radius_mm);
        j.dot_entity_id = doc.add_entity(dot_circ, true, layer);

        junctions_.push_back(std::move(j));
    }
}

const SchematicWire* SchematicWireRouter::find_wire(std::string_view wire_id) const noexcept {
    for (const auto& w : wires_) {
        if (w.wire_id == wire_id) return &w;
    }
    return nullptr;
}

std::string SchematicWireRouter::net_at_point(Point2D pt, double tol) const {
    // Check existing wires
    for (const auto& w : wires_) {
        for (size_t i = 0; i + 1 < w.points.size(); ++i) {
            if (point_on_segment(pt, w.points[i], w.points[i + 1], tol)) {
                return w.net_name;
            }
        }
    }
    // Check pins from symbol engine if available
    if (symbol_engine_) {
        for (const auto& pin : symbol_engine_->all_pins()) {
            if (is_near(pin.world_pos, pt, tol)) {
                return pin.symbol_instance_id + "_" + pin.pin_name;
            }
        }
    }
    return {};
}

void SchematicWireRouter::clear(SketchDocument2D& doc) {
    for (const auto& w : wires_) {
        for (const auto& eid : w.entity_ids) {
            doc.remove_entity(eid);
        }
    }
    wires_.clear();
    for (const auto& j : junctions_) {
        if (!j.dot_entity_id.empty()) {
            doc.remove_entity(j.dot_entity_id);
        }
    }
    junctions_.clear();
}

} // namespace shunya::cad2d
