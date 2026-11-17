#include "shunya/cad2d/trim_extend_tool.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <algorithm>
#include <limits>

namespace shunya::cad2d {

namespace {

[[nodiscard]] double dist(const Point2D& a, const Point2D& b) noexcept {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool line_line_intersection(const Point2D& p1, const Point2D& p2,
                            const Point2D& p3, const Point2D& p4,
                            Point2D& out_pt, double& out_t, double& out_u) {
    double x1 = p1.x, y1 = p1.y, x2 = p2.x, y2 = p2.y;
    double x3 = p3.x, y3 = p3.y, x4 = p4.x, y4 = p4.y;

    double d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::abs(d) < 1e-12) return false;

    out_t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / d;
    out_u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / d;

    out_pt = Point2D{x1 + out_t * (x2 - x1), y1 + out_t * (y2 - y1)};
    return true;
}

std::vector<Point2D> line_circle_intersection(const Point2D& p0, const Point2D& p1,
                                              const Point2D& center, double radius) {
    std::vector<Point2D> pts;
    Vec2d d = p1 - p0;
    Vec2d f = p0 - center;

    double a = d.dot(d);
    double b = 2.0 * f.dot(d);
    double c = f.dot(f) - radius * radius;

    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0 || a < 1e-12) return pts;

    double sqrt_d = std::sqrt(disc);
    double t1 = (-b - sqrt_d) / (2.0 * a);
    double t2 = (-b + sqrt_d) / (2.0 * a);

    if (t1 >= -1e-6 && t1 <= 1.0 + 1e-6) {
        pts.push_back(p0 + d * std::clamp(t1, 0.0, 1.0));
    }
    if (t2 >= -1e-6 && t2 <= 1.0 + 1e-6 && std::abs(t2 - t1) > 1e-6) {
        pts.push_back(p0 + d * std::clamp(t2, 0.0, 1.0));
    }
    return pts;
}

} // namespace

std::vector<Point2D> TrimExtendEngine::find_intersections(const Curve2D& a, const Curve2D& b) {
    std::vector<Point2D> results;

    const auto* la = dynamic_cast<const LineSegment2D*>(&a);
    const auto* lb = dynamic_cast<const LineSegment2D*>(&b);

    if (la && lb) {
        Point2D inter{};
        double t = 0.0, u = 0.0;
        if (line_line_intersection(la->p0, la->p1, lb->p0, lb->p1, inter, t, u)) {
            if (t >= -1e-6 && t <= 1.0 + 1e-6 && u >= -1e-6 && u <= 1.0 + 1e-6) {
                results.push_back(inter);
            }
        }
        return results;
    }

    const auto* ca = dynamic_cast<const Circle2D*>(&a);
    const auto* cb = dynamic_cast<const Circle2D*>(&b);

    if (la && cb) {
        return line_circle_intersection(la->p0, la->p1, cb->center, cb->radius);
    }
    if (ca && lb) {
        return line_circle_intersection(lb->p0, lb->p1, ca->center, ca->radius);
    }

    // General sampling intersection
    auto pts_a = a.sample_polyline(48);
    auto pts_b = b.sample_polyline(48);

    for (size_t i = 0; i + 1 < pts_a.size(); ++i) {
        for (size_t j = 0; j + 1 < pts_b.size(); ++j) {
            Point2D inter{};
            double t = 0.0, u = 0.0;
            if (line_line_intersection(pts_a[i], pts_a[i + 1], pts_b[j], pts_b[j + 1], inter, t, u)) {
                if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
                    bool duplicate = false;
                    for (const auto& r : results) {
                        if (dist(r, inter) < 1e-4) { duplicate = true; break; }
                    }
                    if (!duplicate) results.push_back(inter);
                }
            }
        }
    }

    return results;
}

core::base::Result<TrimResult> TrimExtendEngine::trim_curve(
    SketchDocument2D& doc,
    const std::string& target_entity_id,
    const Point2D& pick_pt,
    const std::vector<std::string>& boundary_entity_ids
) {
    const auto* ent = doc.find_entity(target_entity_id);
    if (!ent || !ent->curve) {
        return core::base::Result<TrimResult>::err({
            .code = 4301,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Target entity not found in document",
            .details = ""
        });
    }

    const auto& target_curve = *ent->curve;
    std::string layer = ent->layer;
    bool is_const = ent->is_construction;

    // Collect cutting intersections
    std::vector<double> split_params;
    split_params.push_back(0.0);
    split_params.push_back(1.0);

    for (const auto& e : doc.entities()) {
        if (e.id == target_entity_id || !e.curve) continue;
        if (!boundary_entity_ids.empty()) {
            if (std::find(boundary_entity_ids.begin(), boundary_entity_ids.end(), e.id) == boundary_entity_ids.end()) {
                continue;
            }
        }

        auto inters = find_intersections(target_curve, *e.curve);
        for (const auto& pt : inters) {
            double t = 0.0;
            target_curve.closest_point(pt, &t);
            if (t > 1e-4 && t < 1.0 - 1e-4) {
                split_params.push_back(t);
            }
        }
    }

    std::sort(split_params.begin(), split_params.end());
    // Remove duplicate parameters
    auto last = std::unique(split_params.begin(), split_params.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-4;
    });
    split_params.erase(last, split_params.end());

    if (split_params.size() <= 2) {
        // No intersections found to trim
        return core::base::Result<TrimResult>::err({
            .code = 4302,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "No cutting intersections found to trim curve",
            .details = ""
        });
    }

    double pick_t = 0.0;
    target_curve.closest_point(pick_pt, &pick_t);

    // Find interval containing pick_t
    size_t trim_interval_idx = split_params.size();
    for (size_t i = 0; i + 1 < split_params.size(); ++i) {
        if (pick_t >= split_params[i] && pick_t <= split_params[i + 1]) {
            trim_interval_idx = i;
            break;
        }
    }

    TrimResult result;
    result.original_entity_id = target_entity_id;

    for (size_t i = 0; i + 1 < split_params.size(); ++i) {
        if (i == trim_interval_idx) continue; // Trim this segment out

        double t0 = split_params[i];
        double t1 = split_params[i + 1];

        if (const auto* line = dynamic_cast<const LineSegment2D*>(&target_curve)) {
            Point2D pa = line->evaluate(t0);
            Point2D pb = line->evaluate(t1);
            if (dist(pa, pb) > 1e-6) {
                result.remaining_curves.push_back(std::make_shared<LineSegment2D>(pa, pb));
            }
        } else if (const auto* arc = dynamic_cast<const Arc2D*>(&target_curve)) {
            double a0 = arc->start_angle + t0 * arc->sweep_angle;
            double sweep = (t1 - t0) * arc->sweep_angle;
            result.remaining_curves.push_back(std::make_shared<Arc2D>(arc->center, arc->radius, a0, sweep));
        } else {
            // General curve fallback: sample polyline between t0 and t1
            std::vector<Point2D> pts;
            const size_t segs = 20;
            for (size_t k = 0; k <= segs; ++k) {
                double sub_t = t0 + (static_cast<double>(k) / static_cast<double>(segs)) * (t1 - t0);
                pts.push_back(target_curve.evaluate(sub_t));
            }
            result.remaining_curves.push_back(std::make_shared<Polyline2D>(std::move(pts), false));
        }
    }

    // Replace in document
    doc.remove_entity(target_entity_id);
    for (auto& c : result.remaining_curves) {
        doc.add_entity(c, is_const, layer);
    }

    return core::base::Result<TrimResult>::ok(result);
}

core::base::Result<Point2D> TrimExtendEngine::extend_curve(
    SketchDocument2D& doc,
    const std::string& target_entity_id,
    const Point2D& pick_near_end,
    const std::vector<std::string>& boundary_entity_ids
) {
    auto* ent = const_cast<SketchEntity*>(doc.find_entity(target_entity_id));
    if (!ent || !ent->curve) {
        return core::base::Result<Point2D>::err({
            .code = 4310,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Target entity not found in document",
            .details = ""
        });
    }

    auto* line = dynamic_cast<LineSegment2D*>(ent->curve.get());
    if (!line) {
        return core::base::Result<Point2D>::err({
            .code = 4311,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Only LineSegment2D extension is currently supported",
            .details = ""
        });
    }

    double d0 = dist(pick_near_end, line->p0);
    double d1 = dist(pick_near_end, line->p1);

    bool extend_p1 = (d1 <= d0);
    Point2D ray_origin = extend_p1 ? line->p1 : line->p0;
    Vec2d ray_dir = extend_p1 ? (line->p1 - line->p0) : (line->p0 - line->p1);
    double ray_len = ray_dir.length();
    if (ray_len < 1e-12) {
        return core::base::Result<Point2D>::err({
            .code = 4312,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Zero-length line cannot be extended",
            .details = ""
        });
    }
    ray_dir = ray_dir / ray_len;

    // Search for closest boundary intersection in forward ray direction
    double closest_dist = std::numeric_limits<double>::infinity();
    Point2D best_inter{};

    // Project forward line
    const double max_extend_dist = 10000.0;
    LineSegment2D probe_line(ray_origin, ray_origin + ray_dir * max_extend_dist);

    for (const auto& e : doc.entities()) {
        if (e.id == target_entity_id || !e.curve) continue;
        if (!boundary_entity_ids.empty()) {
            if (std::find(boundary_entity_ids.begin(), boundary_entity_ids.end(), e.id) == boundary_entity_ids.end()) {
                continue;
            }
        }

        auto inters = find_intersections(probe_line, *e.curve);
        for (const auto& pt : inters) {
            double d = dist(ray_origin, pt);
            if (d > 1e-4 && d < closest_dist) {
                closest_dist = d;
                best_inter = pt;
            }
        }
    }

    if (std::isinf(closest_dist)) {
        return core::base::Result<Point2D>::err({
            .code = 4313,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "No boundary intersection found to extend line to",
            .details = ""
        });
    }

    if (extend_p1) {
        line->p1 = best_inter;
    } else {
        line->p0 = best_inter;
    }

    return core::base::Result<Point2D>::ok(best_inter);
}

bool TrimExtendEngine::erase_entity(SketchDocument2D& doc, const std::string& entity_id) {
    return doc.remove_entity(entity_id);
}

size_t TrimExtendEngine::erase_along_stroke(SketchDocument2D& doc, const std::vector<Point2D>& stroke_pts, double radius) {
    if (stroke_pts.size() < 2) return 0;
    std::vector<std::string> to_remove;

    for (const auto& ent : doc.entities()) {
        if (!ent.curve) continue;
        bool hits = false;

        for (size_t k = 0; k + 1 < stroke_pts.size(); ++k) {
            LineSegment2D stroke_seg(stroke_pts[k], stroke_pts[k + 1]);
            // Check direct intersection
            auto inters = find_intersections(*ent.curve, stroke_seg);
            if (!inters.empty()) {
                hits = true;
                break;
            }
            // Check proximity by sampling segment
            const size_t sub_samples = 16;
            for (size_t s = 0; s <= sub_samples; ++s) {
                double sub_t = static_cast<double>(s) / static_cast<double>(sub_samples);
                Point2D p = stroke_seg.evaluate(sub_t);
                double t = 0.0;
                Point2D cp = ent.curve->closest_point(p, &t);
                if (dist(p, cp) <= radius) {
                    hits = true;
                    break;
                }
            }
            if (hits) break;
        }

        if (hits) {
            to_remove.push_back(ent.id);
        }
    }

    for (const auto& id : to_remove) {
        doc.remove_entity(id);
    }
    return to_remove.size();
}

bool TrimExtendTool::on_click(SketchDocument2D& doc, const Point2D& world_pt) {
    // Find closest entity to click
    double min_d = 0.5; // Pick tolerance
    std::string picked_id;

    for (const auto& ent : doc.entities()) {
        if (!ent.curve) continue;
        double t = 0.0;
        Point2D cp = ent.curve->closest_point(world_pt, &t);
        double d = dist(world_pt, cp);
        if (d < min_d) {
            min_d = d;
            picked_id = ent.id;
        }
    }

    if (picked_id.empty()) return false;

    if (mode == TrimExtendMode::Eraser) {
        return TrimExtendEngine::erase_entity(doc, picked_id);
    } else if (mode == TrimExtendMode::Trim) {
        auto res = TrimExtendEngine::trim_curve(doc, picked_id, world_pt);
        return res.is_ok();
    } else if (mode == TrimExtendMode::Extend) {
        auto res = TrimExtendEngine::extend_curve(doc, picked_id, world_pt);
        return res.is_ok();
    }
    return false;
}

} // namespace shunya::cad2d
