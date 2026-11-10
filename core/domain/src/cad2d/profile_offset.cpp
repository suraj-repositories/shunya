#include "shunya/cad2d/profile_offset.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

namespace {

void emit_corner_join(
    const Point2D& vertex,
    const Vec2d& n1,
    const Vec2d& n2,
    double delta,
    JoinType join_type,
    double miter_limit,
    size_t arc_segments,
    Path2D& out_pts
) {
    Point2D p_end1 = vertex + n1 * delta;
    Point2D p_start2 = vertex + n2 * delta;
    double abs_delta = std::abs(delta);

    // Check if angle between normals turns in outer convex direction of offset
    double c = cross2d(n1, n2);
    bool is_outer_turn = (delta >= 0.0) ? (c > 1e-12) : (c < -1e-12);

    if (!is_outer_turn) {
        // Compute exact shifted-line intersection for inward/reflex corner
        Vec2d t1 = perp_ccw(n1);
        Vec2d t2 = perp_ccw(n2);
        double det = cross2d(t1, t2);
        if (std::abs(det) > 1e-9) {
            double u = cross2d(p_start2 - p_end1, t2) / det;
            Point2D inter = p_end1 + t1 * u;
            if ((inter - vertex).length() <= abs_delta * 10.0) {
                out_pts.push_back(inter);
                return;
            }
        }
        out_pts.push_back(p_end1);
        out_pts.push_back(p_start2);
        return;
    }

    if (join_type == JoinType::Round) {
        double a1 = std::atan2(n1.y * (delta >= 0.0 ? 1.0 : -1.0), n1.x * (delta >= 0.0 ? 1.0 : -1.0));
        double a2 = std::atan2(n2.y * (delta >= 0.0 ? 1.0 : -1.0), n2.x * (delta >= 0.0 ? 1.0 : -1.0));
        double sweep = a2 - a1;
        constexpr double kTwoPi = 2.0 * core::base::math::PI;
        if (delta >= 0.0) {
            while (sweep < 0.0) sweep += kTwoPi;
        } else {
            while (sweep > 0.0) sweep -= kTwoPi;
        }
        size_t steps = std::max<size_t>(2, static_cast<size_t>(std::ceil(std::abs(sweep) / (kTwoPi / static_cast<double>(std::max<size_t>(8, arc_segments))))));
        for (size_t k = 0; k <= steps; ++k) {
            double ang = a1 + sweep * (static_cast<double>(k) / static_cast<double>(steps));
            out_pts.push_back(vertex + Vec2d{std::cos(ang), std::sin(ang)} * abs_delta);
        }
        return;
    }

    if (join_type == JoinType::Square) {
        Vec2d t1 = perp_ccw(n1);
        Vec2d t2 = perp_ccw(n2);
        Vec2d sq_dir = (n1 + n2).normalized();
        double sign = (delta >= 0.0) ? 1.0 : -1.0;
        Point2D p1 = p_end1 + t1 * (abs_delta * sign);
        Point2D p2 = p_start2 - t2 * (abs_delta * sign);
        (void)sq_dir;
        out_pts.push_back(p1);
        out_pts.push_back(p2);
        return;
    }

    // Miter join
    double dot_n = std::clamp(n1.dot(n2), -1.0, 1.0);
    double cos_half = std::sqrt(std::max(0.0, (1.0 + dot_n) * 0.5));
    if (cos_half > 1e-6 && (1.0 / cos_half) <= miter_limit) {
        Vec2d miter_vec = (n1 + n2).normalized();
        out_pts.push_back(vertex + miter_vec * (delta / cos_half));
    } else {
        // Bevel fallback when exceeding miter_limit
        out_pts.push_back(p_end1);
        out_pts.push_back(p_start2);
    }
}

int signed_winding_number(const Point2D& pt, const Path2D& poly) noexcept {
    int wn = 0;
    for (size_t i = 0; i < poly.size(); ++i) {
        const auto& a = poly[i];
        const auto& b = poly[(i + 1) % poly.size()];
        if (a.y <= pt.y) {
            if (b.y > pt.y && cross2d(b - a, pt - a) > 0.0) {
                ++wn;
            }
        } else {
            if (b.y <= pt.y && cross2d(b - a, pt - a) < 0.0) {
                --wn;
            }
        }
    }
    return wn;
}

bool find_strict_interior_point(const Path2D& simple_poly, Point2D& out_pt) {
    if (simple_poly.size() < 3) return false;
    for (size_t i = 0; i < simple_poly.size(); ++i) {
        const auto& p0 = simple_poly[i];
        const auto& p1 = simple_poly[(i + 1) % simple_poly.size()];
        if (std::abs(p1.x - p0.x) < 1e-12) continue;
        double mx = 0.5 * (p0.x + p1.x);
        std::vector<double> ys;
        for (size_t j = 0; j < simple_poly.size(); ++j) {
            const auto& a = simple_poly[j];
            const auto& b = simple_poly[(j + 1) % simple_poly.size()];
            double minx = std::min(a.x, b.x);
            double maxx = std::max(a.x, b.x);
            if (minx <= mx && mx < maxx && std::abs(b.x - a.x) > 1e-15) {
                double t = (mx - a.x) / (b.x - a.x);
                ys.push_back(a.y + t * (b.y - a.y));
            }
        }
        if (ys.size() >= 2) {
            std::sort(ys.begin(), ys.end());
            for (size_t k = 0; k + 1 < ys.size(); k += 2) {
                if (ys[k + 1] - ys[k] > 1e-11) {
                    out_pt = {mx, 0.5 * (ys[k] + ys[k + 1])};
                    return true;
                }
            }
        }
    }
    return false;
}

double min_distance_to_loop(const Point2D& pt, const Path2D& loop) noexcept {
    double best = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < loop.size(); ++i) {
        const auto& a = loop[i];
        const auto& b = loop[(i + 1) % loop.size()];
        Vec2d ab = b - a;
        double len_sq = ab.length_squared();
        double t = len_sq > 1e-24 ? std::clamp((pt - a).dot(ab) / len_sq, 0.0, 1.0) : 0.0;
        double d = (pt - (a + ab * t)).length();
        if (d < best) best = d;
    }
    return best;
}

} // namespace

Paths2D ProfileOffsetEngine::offset_polygons(
    const Paths2D& polygons,
    double delta,
    JoinType join_type,
    double miter_limit,
    size_t arc_segments
) {
    if (polygons.empty()) return {};
    if (std::abs(delta) < 1e-12) return polygons;

    auto hierarchy = PolygonBooleanEngine::extract_hierarchy(polygons);
    Paths2D outers_result;
    Paths2D holes_result;

    auto offset_single_loop = [&](const Path2D& loop, Paths2D& dest) {
        if (loop.size() < 3) return;
        std::vector<Vec2d> normals(loop.size());
        for (size_t i = 0; i < loop.size(); ++i) {
            Vec2d d = (loop[(i + 1) % loop.size()] - loop[i]).normalized();
            normals[i] = perp_cw(d); // Outward normal (to RIGHT of edge)
        }

        Path2D raw;
        raw.reserve(loop.size() * 2);
        for (size_t i = 0; i < loop.size(); ++i) {
            size_t prev = (i + loop.size() - 1) % loop.size();
            emit_corner_join(loop[i], normals[prev], normals[i], delta, join_type, miter_limit, arc_segments, raw);
        }

        double orig_sign = PolygonBooleanEngine::signed_area(loop);
        auto simplified = PolygonBooleanEngine::simplify({raw}, FillRule::NonZero);
        for (auto& s : simplified) {
            double new_area = PolygonBooleanEngine::signed_area(s);
            if (std::abs(new_area) <= 1e-10) continue;

            Point2D interior_pt;
            if (!find_strict_interior_point(s, interior_pt)) continue;
            int wn = signed_winding_number(interior_pt, raw);

            if (orig_sign > 0.0 && wn > 0) {
                if (delta < 0.0) {
                    if (!PolygonBooleanEngine::point_in_path(interior_pt, loop)) continue;
                    if (min_distance_to_loop(interior_pt, loop) < std::abs(delta) - 1e-4) continue;
                }
                dest.push_back(std::move(s));
            } else if (orig_sign < 0.0 && wn < 0) {
                if (delta > 0.0 && min_distance_to_loop(interior_pt, loop) < std::abs(delta) - 1e-4) continue;
                if (new_area < 0.0) std::reverse(s.begin(), s.end());
                dest.push_back(std::move(s));
            }
        }
    };

    if (hierarchy.empty()) {
        // Fallback if caller supplied only CW or unoriented paths
        for (const auto& p : polygons) {
            offset_single_loop(p, outers_result);
        }
        return PolygonBooleanEngine::simplify(outers_result, FillRule::NonZero);
    }

    for (const auto& poly : hierarchy) {
        offset_single_loop(poly.outer, outers_result);
        for (const auto& h : poly.holes) {
            // Shifting a CW hole by +delta shrinks the hole (which expands solid!).
            // Note: for a CW hole loop, perp_cw(d) points INWARD toward hole center, so +delta moves hole boundary inward cleanly!
            offset_single_loop(h, holes_result);
        }
    }

    if (outers_result.empty()) return {};
    if (holes_result.empty()) {
        return PolygonBooleanEngine::simplify(outers_result, FillRule::NonZero);
    }
    return PolygonBooleanEngine::execute(BooleanOp::Difference, outers_result, holes_result, FillRule::NonZero);
}

Path2D ProfileOffsetEngine::offset_open_polyline(
    const Path2D& polyline,
    double delta,
    JoinType join_type,
    double miter_limit,
    size_t arc_segments
) {
    if (polyline.size() < 2) return polyline;
    if (std::abs(delta) < 1e-12) return polyline;

    std::vector<Vec2d> normals;
    normals.reserve(polyline.size() - 1);
    for (size_t i = 0; i + 1 < polyline.size(); ++i) {
        Vec2d d = (polyline[i + 1] - polyline[i]).normalized();
        normals.push_back(perp_cw(d));
    }

    Path2D out;
    out.push_back(polyline.front() + normals.front() * delta);
    for (size_t i = 1; i + 1 < polyline.size(); ++i) {
        emit_corner_join(polyline[i], normals[i - 1], normals[i], delta, join_type, miter_limit, arc_segments, out);
    }
    out.push_back(polyline.back() + normals.back() * delta);
    return out;
}

} // namespace shunya::cad2d
