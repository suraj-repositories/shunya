#include "shunya/cad2d/snap_manager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace shunya::cad2d {

namespace {

double norm_ang(double a) noexcept {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    a = std::fmod(a, kTwoPi);
    if (a < 0.0) a += kTwoPi;
    return a;
}

bool point_on_arc_sweep(const Point2D& pt, const Arc2D& arc) noexcept {
    constexpr double kTwoPi = 2.0 * core::base::math::PI;
    if (std::abs(arc.sweep_angle) >= kTwoPi - 1e-10) return true;
    double ang = std::atan2(pt.y - arc.center.y, pt.x - arc.center.x);
    if (arc.sweep_angle >= 0.0) {
        return norm_ang(ang - arc.start_angle) <= arc.sweep_angle + 1e-9;
    } else {
        return norm_ang(arc.start_angle - ang) <= (-arc.sweep_angle) + 1e-9;
    }
}

std::vector<Point2D> intersect_line_line(const LineSegment2D& l1, const LineSegment2D& l2) {
    Vec2d d1 = l1.p1 - l1.p0;
    Vec2d d2 = l2.p1 - l2.p0;
    double denom = cross2d(d1, d2);
    if (std::abs(denom) < 1e-12) return {};

    Vec2d delta = l2.p0 - l1.p0;
    double t = cross2d(delta, d2) / denom;
    double u = cross2d(delta, d1) / denom;
    if (t >= -1e-9 && t <= 1.0 + 1e-9 && u >= -1e-9 && u <= 1.0 + 1e-9) {
        return {l1.p0 + d1 * std::clamp(t, 0.0, 1.0)};
    }
    return {};
}

std::vector<Point2D> intersect_line_circle_gen(
    const LineSegment2D& seg,
    const Point2D& center,
    double radius,
    const Arc2D* optional_arc
) {
    Vec2d d = seg.p1 - seg.p0;
    Vec2d f = seg.p0 - center;
    double a = d.dot(d);
    if (a < 1e-24) return {};

    double b = 2.0 * f.dot(d);
    double c = f.dot(f) - radius * radius;
    double disc = b * b - 4.0 * a * c;
    if (disc < -1e-11) return {};
    disc = std::max(0.0, disc);

    double sqrt_d = std::sqrt(disc);
    double t1 = (-b - sqrt_d) / (2.0 * a);
    double t2 = (-b + sqrt_d) / (2.0 * a);

    std::vector<Point2D> out;
    auto check_t = [&](double t) {
        if (t >= -1e-9 && t <= 1.0 + 1e-9) {
            Point2D p = seg.p0 + d * std::clamp(t, 0.0, 1.0);
            if (!optional_arc || point_on_arc_sweep(p, *optional_arc)) {
                out.push_back(p);
            }
        }
    };
    check_t(t1);
    if (std::abs(t2 - t1) > 1e-9) check_t(t2);
    return out;
}

std::vector<Point2D> intersect_circle_circle_gen(
    const Point2D& c1, double r1, const Arc2D* arc1,
    const Point2D& c2, double r2, const Arc2D* arc2
) {
    Vec2d diff = c2 - c1;
    double d = diff.length();
    if (d < 1e-12 || d > r1 + r2 + 1e-9 || d < std::abs(r1 - r2) - 1e-9) {
        return {};
    }

    double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    double h_sq = std::max(0.0, r1 * r1 - a * a);
    double h = std::sqrt(h_sq);

    Vec2d ex = diff / d;
    Vec2d ey = perp_ccw(ex);
    Point2D mid = c1 + ex * a;

    std::vector<Point2D> out;
    auto emit_if_on_arcs = [&](const Point2D& p) {
        if (arc1 && !point_on_arc_sweep(p, *arc1)) return;
        if (arc2 && !point_on_arc_sweep(p, *arc2)) return;
        out.push_back(p);
    };

    if (h < 1e-10) {
        emit_if_on_arcs(mid);
    } else {
        emit_if_on_arcs(mid + ey * h);
        emit_if_on_arcs(mid - ey * h);
    }
    return out;
}

int snap_priority(SnapType t) noexcept {
    switch (t) {
        case SnapType::Endpoint:     return 0;
        case SnapType::Intersection: return 1;
        case SnapType::Center:       return 2;
        case SnapType::Midpoint:     return 3;
        case SnapType::Quadrant:     return 4;
        case SnapType::Tangent:      return 5;
        default: return 10;
    }
}

void collect_simple_segments(const Curve2D& curve, std::vector<LineSegment2D>& out_segs) {
    if (curve.type() == CurveType2D::LineSegment) {
        out_segs.push_back(static_cast<const LineSegment2D&>(curve));
    } else if (curve.type() == CurveType2D::Polyline) {
        const auto& pl = static_cast<const Polyline2D&>(curve);
        for (size_t i = 0; i + 1 < pl.points.size(); ++i) {
            out_segs.emplace_back(pl.points[i], pl.points[i + 1]);
        }
        if (pl.closed && pl.points.size() >= 3) {
            out_segs.emplace_back(pl.points.back(), pl.points.front());
        }
    }
}

} // namespace

SnapTargetManager::SnapTargetManager(double hash_cell_size)
    : cell_size_(std::max(1e-4, hash_cell_size))
{
}

void SnapTargetManager::rebuild_index(const SketchDocument2D& doc) {
    grid_.clear();
    large_entities_.clear();
    const auto& ents = doc.entities();

    for (size_t idx = 0; idx < ents.size(); ++idx) {
        const auto& e = ents[idx];
        if (!e.visible || !e.curve) continue;
        BBox2D box = e.curve->bounding_box();
        if (!box.is_valid()) continue;

        int64_t ix0 = static_cast<int64_t>(std::floor(box.min.x / cell_size_));
        int64_t iy0 = static_cast<int64_t>(std::floor(box.min.y / cell_size_));
        int64_t ix1 = static_cast<int64_t>(std::floor(box.max.x / cell_size_));
        int64_t iy1 = static_cast<int64_t>(std::floor(box.max.y / cell_size_));

        if ((ix1 - ix0 + 1) * (iy1 - iy0 + 1) > 64) {
            large_entities_.push_back(idx);
        } else {
            for (int64_t ix = ix0; ix <= ix1; ++ix) {
                for (int64_t iy = iy0; iy <= iy1; ++iy) {
                    grid_[{ix, iy}].push_back(idx);
                }
            }
        }
    }
}

std::vector<Point2D> SnapTargetManager::intersect_curves(const Curve2D& a, const Curve2D& b) {
    if (!a.bounding_box().intersects(b.bounding_box(), 1e-6)) {
        return {};
    }

    auto ta = a.type();
    auto tb = b.type();

    // Expand polylines into segments cleanly
    if (ta == CurveType2D::Polyline || tb == CurveType2D::Polyline) {
        if (ta == CurveType2D::Polyline) {
            std::vector<LineSegment2D> segs_a;
            collect_simple_segments(a, segs_a);
            std::vector<Point2D> pts;
            for (const auto& s : segs_a) {
                auto sub = intersect_curves(s, b);
                pts.insert(pts.end(), sub.begin(), sub.end());
            }
            return pts;
        } else {
            return intersect_curves(b, a);
        }
    }

    if (ta == CurveType2D::LineSegment && tb == CurveType2D::LineSegment) {
        return intersect_line_line(static_cast<const LineSegment2D&>(a), static_cast<const LineSegment2D&>(b));
    }
    if (ta == CurveType2D::LineSegment && tb == CurveType2D::Circle) {
        const auto& c = static_cast<const Circle2D&>(b);
        return intersect_line_circle_gen(static_cast<const LineSegment2D&>(a), c.center, c.radius, nullptr);
    }
    if (ta == CurveType2D::Circle && tb == CurveType2D::LineSegment) {
        return intersect_curves(b, a);
    }
    if (ta == CurveType2D::LineSegment && tb == CurveType2D::Arc) {
        const auto& arc = static_cast<const Arc2D&>(b);
        return intersect_line_circle_gen(static_cast<const LineSegment2D&>(a), arc.center, arc.radius, &arc);
    }
    if (ta == CurveType2D::Arc && tb == CurveType2D::LineSegment) {
        return intersect_curves(b, a);
    }
    if (ta == CurveType2D::Circle && tb == CurveType2D::Circle) {
        const auto& c1 = static_cast<const Circle2D&>(a);
        const auto& c2 = static_cast<const Circle2D&>(b);
        return intersect_circle_circle_gen(c1.center, c1.radius, nullptr, c2.center, c2.radius, nullptr);
    }
    if (ta == CurveType2D::Circle && tb == CurveType2D::Arc) {
        const auto& c1 = static_cast<const Circle2D&>(a);
        const auto& a2 = static_cast<const Arc2D&>(b);
        return intersect_circle_circle_gen(c1.center, c1.radius, nullptr, a2.center, a2.radius, &a2);
    }
    if (ta == CurveType2D::Arc && tb == CurveType2D::Circle) {
        return intersect_curves(b, a);
    }
    if (ta == CurveType2D::Arc && tb == CurveType2D::Arc) {
        const auto& a1 = static_cast<const Arc2D&>(a);
        const auto& a2 = static_cast<const Arc2D&>(b);
        return intersect_circle_circle_gen(a1.center, a1.radius, &a1, a2.center, a2.radius, &a2);
    }

    return {};
}

std::vector<Point2D> SnapTargetManager::find_tangent_points(const Point2D& anchor, const Curve2D& curve) {
    Point2D center;
    double radius = 0.0;
    const Arc2D* arc_ptr = nullptr;

    if (curve.type() == CurveType2D::Circle) {
        const auto& c = static_cast<const Circle2D&>(curve);
        center = c.center;
        radius = c.radius;
    } else if (curve.type() == CurveType2D::Arc) {
        const auto& a = static_cast<const Arc2D&>(curve);
        center = a.center;
        radius = a.radius;
        arc_ptr = &a;
    } else {
        return {};
    }

    Vec2d delta = anchor - center;
    double d = delta.length();
    if (d <= radius + 1e-12 || radius <= 1e-12) return {};

    double base_ang = std::atan2(delta.y, delta.x);
    double alpha = std::acos(std::clamp(radius / d, -1.0, 1.0));

    std::vector<Point2D> out;
    for (double sgn : {1.0, -1.0}) {
        double theta = base_ang + sgn * alpha;
        Point2D pt = center + Vec2d{radius * std::cos(theta), radius * std::sin(theta)};
        if (!arc_ptr || point_on_arc_sweep(pt, *arc_ptr)) {
            out.push_back(pt);
        }
    }
    return out;
}

std::optional<SnapCandidate> SnapTargetManager::query_snap(
    const SketchDocument2D& doc,
    const SketchViewport2D& viewport,
    const Point2D& cursor_world,
    double tolerance_px,
    uint32_t enabled_mask,
    std::optional<Point2D> active_anchor
) const {
    const auto& ents = doc.entities();
    if (ents.empty() || enabled_mask == 0) return std::nullopt;

    double tol_world = tolerance_px / std::max(1e-12, viewport.pixels_per_unit);
    Point2D cursor_px = viewport.world_to_screen(cursor_world);

    std::vector<size_t> candidate_indices;
    if (grid_.empty() && large_entities_.empty()) {
        candidate_indices.reserve(ents.size());
        for (size_t i = 0; i < ents.size(); ++i) candidate_indices.push_back(i);
    } else {
        std::unordered_set<size_t> seen;
        int64_t ix0 = static_cast<int64_t>(std::floor((cursor_world.x - tol_world) / cell_size_));
        int64_t iy0 = static_cast<int64_t>(std::floor((cursor_world.y - tol_world) / cell_size_));
        int64_t ix1 = static_cast<int64_t>(std::floor((cursor_world.x + tol_world) / cell_size_));
        int64_t iy1 = static_cast<int64_t>(std::floor((cursor_world.y + tol_world) / cell_size_));

        for (int64_t ix = ix0; ix <= ix1; ++ix) {
            for (int64_t iy = iy0; iy <= iy1; ++iy) {
                auto it = grid_.find({ix, iy});
                if (it != grid_.end()) {
                    for (size_t idx : it->second) seen.insert(idx);
                }
            }
        }
        for (size_t idx : large_entities_) seen.insert(idx);
        candidate_indices.assign(seen.begin(), seen.end());
    }

    std::optional<SnapCandidate> best;
    auto consider_point = [&](const Point2D& world_pt, SnapType type, const std::string& ent_id) {
        if ((enabled_mask & static_cast<uint32_t>(type)) == 0) return;
        Point2D pt_px = viewport.world_to_screen(world_pt);
        double dist_px = (pt_px - cursor_px).length();
        if (dist_px > tolerance_px) return;

        if (!best) {
            best = SnapCandidate{world_pt, type, ent_id, dist_px};
            return;
        }
        int p_new = snap_priority(type);
        int p_old = snap_priority(best->type);
        if (dist_px + 2.0 < best->distance_px || (std::abs(dist_px - best->distance_px) <= 2.0 && p_new < p_old)) {
            best = SnapCandidate{world_pt, type, ent_id, dist_px};
        }
    };

    for (size_t idx : candidate_indices) {
        if (idx >= ents.size()) continue;
        const auto& e = ents[idx];
        if (!e.visible || !e.curve) continue;

        BBox2D box = e.curve->bounding_box();
        if (!box.contains(cursor_world, tol_world)) continue;

        auto t = e.curve->type();
        if (t == CurveType2D::LineSegment) {
            const auto& seg = static_cast<const LineSegment2D&>(*e.curve);
            consider_point(seg.p0, SnapType::Endpoint, e.id);
            consider_point(seg.p1, SnapType::Endpoint, e.id);
            consider_point((seg.p0 + seg.p1) * 0.5, SnapType::Midpoint, e.id);
        } else if (t == CurveType2D::Polyline) {
            const auto& pl = static_cast<const Polyline2D&>(*e.curve);
            for (size_t k = 0; k < pl.points.size(); ++k) {
                consider_point(pl.points[k], SnapType::Endpoint, e.id);
                if (k + 1 < pl.points.size()) {
                    consider_point((pl.points[k] + pl.points[k + 1]) * 0.5, SnapType::Midpoint, e.id);
                }
            }
            if (pl.closed && pl.points.size() >= 3) {
                consider_point((pl.points.back() + pl.points.front()) * 0.5, SnapType::Midpoint, e.id);
            }
        } else if (t == CurveType2D::Circle) {
            const auto& c = static_cast<const Circle2D&>(*e.curve);
            consider_point(c.center, SnapType::Center, e.id);
            consider_point({c.center.x + c.radius, c.center.y}, SnapType::Quadrant, e.id);
            consider_point({c.center.x - c.radius, c.center.y}, SnapType::Quadrant, e.id);
            consider_point({c.center.x, c.center.y + c.radius}, SnapType::Quadrant, e.id);
            consider_point({c.center.x, c.center.y - c.radius}, SnapType::Quadrant, e.id);
            if (active_anchor) {
                for (const auto& tp : find_tangent_points(*active_anchor, c)) {
                    consider_point(tp, SnapType::Tangent, e.id);
                }
            }
        } else if (t == CurveType2D::Arc) {
            const auto& arc = static_cast<const Arc2D&>(*e.curve);
            consider_point(arc.center, SnapType::Center, e.id);
            consider_point(arc.evaluate(0.0), SnapType::Endpoint, e.id);
            consider_point(arc.evaluate(1.0), SnapType::Endpoint, e.id);
            consider_point(arc.evaluate(0.5), SnapType::Midpoint, e.id);
            const std::array<Point2D, 4> quads = {
                Point2D{arc.center.x + arc.radius, arc.center.y},
                Point2D{arc.center.x - arc.radius, arc.center.y},
                Point2D{arc.center.x, arc.center.y + arc.radius},
                Point2D{arc.center.x, arc.center.y - arc.radius}
            };
            for (const auto& q : quads) {
                if (point_on_arc_sweep(q, arc)) consider_point(q, SnapType::Quadrant, e.id);
            }
            if (active_anchor) {
                for (const auto& tp : find_tangent_points(*active_anchor, arc)) {
                    consider_point(tp, SnapType::Tangent, e.id);
                }
            }
        } else if (t == CurveType2D::Ellipse) {
            const auto& ell = static_cast<const Ellipse2D&>(*e.curve);
            consider_point(ell.center, SnapType::Center, e.id);
            consider_point(ell.evaluate(0.0), SnapType::Quadrant, e.id);
            consider_point(ell.evaluate(0.25), SnapType::Quadrant, e.id);
            consider_point(ell.evaluate(0.5), SnapType::Quadrant, e.id);
            consider_point(ell.evaluate(0.75), SnapType::Quadrant, e.id);
        } else {
            consider_point(e.curve->evaluate(0.0), SnapType::Endpoint, e.id);
            consider_point(e.curve->evaluate(1.0), SnapType::Endpoint, e.id);
            consider_point(e.curve->evaluate(0.5), SnapType::Midpoint, e.id);
        }
    }

    // Check pairwise intersections among nearby candidates
    if ((enabled_mask & static_cast<uint32_t>(SnapType::Intersection)) != 0) {
        for (size_t i = 0; i < candidate_indices.size(); ++i) {
            size_t idx_a = candidate_indices[i];
            if (idx_a >= ents.size() || !ents[idx_a].visible || !ents[idx_a].curve) continue;
            if (!ents[idx_a].curve->bounding_box().contains(cursor_world, tol_world)) continue;

            for (size_t j = i + 1; j < candidate_indices.size(); ++j) {
                size_t idx_b = candidate_indices[j];
                if (idx_b >= ents.size() || !ents[idx_b].visible || !ents[idx_b].curve) continue;
                if (!ents[idx_b].curve->bounding_box().contains(cursor_world, tol_world)) continue;

                for (const auto& ipt : intersect_curves(*ents[idx_a].curve, *ents[idx_b].curve)) {
                    consider_point(ipt, SnapType::Intersection, ents[idx_a].id + "+" + ents[idx_b].id);
                }
            }
        }
    }

    return best;
}

} // namespace shunya::cad2d
