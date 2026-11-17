#include "shunya/cad2d/associative_fillet_chamfer.hpp"

#include <cmath>
#include <algorithm>

namespace shunya::cad2d {

core::base::Result<Polyline2D> AssociativeFilletChamferTool::blend_polyline_vertex(
    const Polyline2D& poly,
    size_t vertex_idx,
    const BlendParameters& params,
    size_t arc_segments
) {
    size_t n = poly.points.size();
    if (n < 3) {
        return core::base::Result<Polyline2D>::err({
            .code = 4201,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Polyline must have at least 3 vertices to blend a corner",
            .details = ""
        });
    }

    bool is_closed = poly.is_closed();
    if (!is_closed && (vertex_idx == 0 || vertex_idx >= n - 1)) {
        return core::base::Result<Polyline2D>::err({
            .code = 4202,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Cannot blend open polyline endpoint vertices",
            .details = ""
        });
    }
    if (vertex_idx >= n) {
        return core::base::Result<Polyline2D>::err({
            .code = 4203,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Vertex index out of range",
            .details = ""
        });
    }

    size_t prev_idx = (vertex_idx + n - 1) % n;
    size_t next_idx = (vertex_idx + 1) % n;

    Point2D p_prev = poly.points[prev_idx];
    Point2D p_v = poly.points[vertex_idx];
    Point2D p_next = poly.points[next_idx];

    std::vector<Point2D> new_pts;
    new_pts.reserve(n + arc_segments + 2);

    for (size_t i = 0; i < vertex_idx; ++i) {
        new_pts.push_back(poly.points[i]);
    }

    if (params.op == BlendOperation::Fillet) {
        auto res = FilletChamferEngine::fillet_corner(p_prev, p_v, p_next, params.radius);
        if (res.is_err()) {
            return core::base::Result<Polyline2D>::err(res.error());
        }
        const auto& f = res.value();
        size_t segs = std::max(arc_segments, size_t{4});
        for (size_t k = 0; k <= segs; ++k) {
            double t = static_cast<double>(k) / static_cast<double>(segs);
            new_pts.push_back(f.arc.evaluate(t));
        }
    } else {
        auto res = FilletChamferEngine::chamfer_corner(p_prev, p_v, p_next, params.chamfer_dist1, params.chamfer_dist2);
        if (res.is_err()) {
            return core::base::Result<Polyline2D>::err(res.error());
        }
        const auto& c = res.value();
        new_pts.push_back(c.bevel.p0);
        new_pts.push_back(c.bevel.p1);
    }

    for (size_t i = vertex_idx + 1; i < n; ++i) {
        new_pts.push_back(poly.points[i]);
    }

    return core::base::Result<Polyline2D>::ok(Polyline2D(std::move(new_pts), is_closed));
}

namespace {

[[nodiscard]] double point_dist(const Point2D& a, const Point2D& b) noexcept {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool find_line_intersection_vertex(const LineSegment2D& la, const LineSegment2D& lb,
                                   Point2D& out_vertex, Point2D& out_a_opp, Point2D& out_b_opp) {
    const double eps = 1e-6;
    if (la.p0.approx_equals(lb.p0, eps)) {
        out_vertex = la.p0; out_a_opp = la.p1; out_b_opp = lb.p1; return true;
    }
    if (la.p0.approx_equals(lb.p1, eps)) {
        out_vertex = la.p0; out_a_opp = la.p1; out_b_opp = lb.p0; return true;
    }
    if (la.p1.approx_equals(lb.p0, eps)) {
        out_vertex = la.p1; out_a_opp = la.p0; out_b_opp = lb.p1; return true;
    }
    if (la.p1.approx_equals(lb.p1, eps)) {
        out_vertex = la.p1; out_a_opp = la.p0; out_b_opp = lb.p0; return true;
    }

    // Check analytical intersection
    double x1 = la.p0.x, y1 = la.p0.y, x2 = la.p1.x, y2 = la.p1.y;
    double x3 = lb.p0.x, y3 = lb.p0.y, x4 = lb.p1.x, y4 = lb.p1.y;

    double d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::abs(d) < 1e-12) return false;

    double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / d;

    Point2D inter{x1 + t * (x2 - x1), y1 + t * (y2 - y1)};
    out_vertex = inter;

    // Opposite ends
    out_a_opp = (point_dist(la.p0, inter) > point_dist(la.p1, inter)) ? la.p0 : la.p1;
    out_b_opp = (point_dist(lb.p0, inter) > point_dist(lb.p1, inter)) ? lb.p0 : lb.p1;
    return true;
}

} // namespace

bool AssociativeBlendFeature::update_associative(SketchDocument2D& doc) {
    auto* ent_a = const_cast<SketchEntity*>(doc.find_entity(entity_a_id));
    auto* ent_b = const_cast<SketchEntity*>(doc.find_entity(entity_b_id));
    auto* ent_blend = const_cast<SketchEntity*>(doc.find_entity(blend_entity_id));

    if (!ent_a || !ent_b || !ent_blend) return false;
    auto* la = dynamic_cast<LineSegment2D*>(ent_a->curve.get());
    auto* lb = dynamic_cast<LineSegment2D*>(ent_b->curve.get());
    if (!la || !lb) return false;

    Point2D vertex{}, a_opp{}, b_opp{};
    if (!find_line_intersection_vertex(*la, *lb, vertex, a_opp, b_opp)) {
        return false;
    }

    if (parameters.op == BlendOperation::Fillet) {
        auto res = FilletChamferEngine::fillet_corner(a_opp, vertex, b_opp, parameters.radius);
        if (res.is_err()) return false;
        const auto& f = res.value();

        la->p0 = a_opp;
        la->p1 = f.trimmed_leg1.p1;

        lb->p0 = f.trimmed_leg2.p0;
        lb->p1 = b_opp;

        ent_blend->curve = std::make_shared<Arc2D>(f.arc);
        return true;
    } else {
        auto res = FilletChamferEngine::chamfer_corner(a_opp, vertex, b_opp, parameters.chamfer_dist1, parameters.chamfer_dist2);
        if (res.is_err()) return false;
        const auto& c = res.value();

        la->p0 = a_opp;
        la->p1 = c.trimmed_leg1.p1;

        lb->p0 = c.trimmed_leg2.p0;
        lb->p1 = b_opp;

        ent_blend->curve = std::make_shared<LineSegment2D>(c.bevel);
        return true;
    }
}

core::base::Result<AssociativeBlendFeature> AssociativeFilletChamferTool::apply_between_lines(
    SketchDocument2D& doc,
    const std::string& line_a_id,
    const std::string& line_b_id,
    const BlendParameters& params
) {
    auto* ent_a = const_cast<SketchEntity*>(doc.find_entity(line_a_id));
    auto* ent_b = const_cast<SketchEntity*>(doc.find_entity(line_b_id));

    if (!ent_a || !ent_b) {
        return core::base::Result<AssociativeBlendFeature>::err({
            .code = 4210,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Could not find line entities in document",
            .details = ""
        });
    }

    auto* la = dynamic_cast<LineSegment2D*>(ent_a->curve.get());
    auto* lb = dynamic_cast<LineSegment2D*>(ent_b->curve.get());
    if (!la || !lb) {
        return core::base::Result<AssociativeBlendFeature>::err({
            .code = 4211,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Blend operation requires LineSegment2D curves",
            .details = ""
        });
    }

    Point2D vertex{}, a_opp{}, b_opp{};
    if (!find_line_intersection_vertex(*la, *lb, vertex, a_opp, b_opp)) {
        return core::base::Result<AssociativeBlendFeature>::err({
            .code = 4212,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Lines do not intersect or share a common vertex",
            .details = ""
        });
    }

    AssociativeBlendFeature feat;
    feat.id = "BLEND_" + std::to_string(next_id_++);
    feat.parameters = params;
    feat.entity_a_id = line_a_id;
    feat.entity_b_id = line_b_id;

    if (params.op == BlendOperation::Fillet) {
        auto res = FilletChamferEngine::fillet_corner(a_opp, vertex, b_opp, params.radius);
        if (res.is_err()) return core::base::Result<AssociativeBlendFeature>::err(res.error());
        const auto& f = res.value();

        la->p0 = a_opp;
        la->p1 = f.trimmed_leg1.p1;

        lb->p0 = f.trimmed_leg2.p0;
        lb->p1 = b_opp;

        feat.blend_entity_id = doc.add_entity(std::make_shared<Arc2D>(f.arc), ent_a->is_construction, ent_a->layer);
    } else {
        auto res = FilletChamferEngine::chamfer_corner(a_opp, vertex, b_opp, params.chamfer_dist1, params.chamfer_dist2);
        if (res.is_err()) return core::base::Result<AssociativeBlendFeature>::err(res.error());
        const auto& c = res.value();

        la->p0 = a_opp;
        la->p1 = c.trimmed_leg1.p1;

        lb->p0 = c.trimmed_leg2.p0;
        lb->p1 = b_opp;

        feat.blend_entity_id = doc.add_entity(std::make_shared<LineSegment2D>(c.bevel), ent_a->is_construction, ent_a->layer);
    }

    features_.push_back(feat);
    return core::base::Result<AssociativeBlendFeature>::ok(feat);
}

size_t AssociativeFilletChamferTool::update_all_features(SketchDocument2D& doc) {
    size_t count = 0;
    for (auto& feat : features_) {
        if (feat.update_associative(doc)) {
            ++count;
        }
    }
    return count;
}

} // namespace shunya::cad2d
