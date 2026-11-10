#include "shunya/cad3d/polygon_tessellator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace shunya::cad3d {

namespace {

double cross_p(const cad2d::Point2D& a, const cad2d::Point2D& b, const cad2d::Point2D& c) noexcept {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool point_in_triangle_2d(
    const cad2d::Point2D& p,
    const cad2d::Point2D& a,
    const cad2d::Point2D& b,
    const cad2d::Point2D& c
) noexcept {
    const double c1 = cross_p(a, b, p);
    const double c2 = cross_p(b, c, p);
    const double c3 = cross_p(c, a, p);
    constexpr double eps = -1e-12;
    return (c1 >= eps && c2 >= eps && c3 >= eps);
}

cad2d::Path2D ensure_clean_orientation(const cad2d::Path2D& in, bool want_ccw) {
    cad2d::Path2D clean;
    clean.reserve(in.size());
    for (const auto& pt : in) {
        if (clean.empty() || !clean.back().approx_equals(pt, 1e-10)) {
            clean.push_back(pt);
        }
    }
    if (clean.size() >= 2 && clean.front().approx_equals(clean.back(), 1e-10)) {
        clean.pop_back();
    }
    if (clean.size() < 3) return {};
    const double area = cad2d::PolygonBooleanEngine::signed_area(clean);
    if ((want_ccw && area < 0.0) || (!want_ccw && area > 0.0)) {
        std::reverse(clean.begin(), clean.end());
    }
    return clean;
}

struct IndexedPt {
    cad2d::Point2D pt;
    uint32_t orig_idx;
};

/**
 * @brief Eberly bridge algorithm splicing a CW hole loop into a CCW outer loop at mutually visible vertices.
 */
void bridge_hole_into_outer(std::vector<IndexedPt>& outer, const std::vector<IndexedPt>& hole) {
    if (outer.empty() || hole.empty()) return;

    // 1. Find hole vertex M with maximum x coordinate
    size_t hole_max_i = 0;
    for (size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].pt.x > hole[hole_max_i].pt.x ||
            (hole[i].pt.x == hole[hole_max_i].pt.x && hole[i].pt.y < hole[hole_max_i].pt.y)) {
            hole_max_i = i;
        }
    }
    const cad2d::Point2D M = hole[hole_max_i].pt;

    // 2. Cast horizontal ray +X from M against solid-bounding edges (A.y <= M.y < B.y)
    double best_t = std::numeric_limits<double>::infinity();
    size_t best_edge_i = 0;
    double best_s = 0.0;
    cad2d::Point2D best_I = M;
    const size_t n = outer.size();

    for (size_t i = 0; i < n; ++i) {
        const cad2d::Point2D& A = outer[i].pt;
        const cad2d::Point2D& B = outer[(i + 1) % n].pt;
        if (!(A.y <= M.y && B.y > M.y)) continue;
        const double s = (M.y - A.y) / (B.y - A.y);
        const double ix = A.x + s * (B.x - A.x);
        if (ix >= M.x - 1e-12 && (ix - M.x) < best_t) {
            best_t = ix - M.x;
            best_edge_i = i;
            best_s = s;
            best_I = {ix, M.y};
        }
    }

    // If ray hits endpoint A/B directly, skip reflex triangle search because M->I is already unobstructed
    const size_t i0 = best_edge_i;
    const size_t i1 = (best_edge_i + 1) % n;
    size_t visible_outer_idx = i0;
    bool hit_exact_vertex = false;
    if (best_s <= 1e-9) {
        visible_outer_idx = i0;
        hit_exact_vertex = true;
    } else if (best_s >= 1.0 - 1e-9) {
        visible_outer_idx = i1;
        hit_exact_vertex = true;
    } else {
        visible_outer_idx = (outer[i0].pt.x >= outer[i1].pt.x) ? i0 : i1;
    }
    const cad2d::Point2D P = outer[visible_outer_idx].pt;

    // 3. Check if any reflex vertex of outer lies inside non-degenerate triangle (M, best_I, P)
    if (!hit_exact_vertex && (P - best_I).length_squared() > 1e-18) {
        const cad2d::Point2D tri_a = M;
        const cad2d::Point2D tri_b = best_I;
        const cad2d::Point2D tri_c = P;
        const bool ccw_tri = cross_p(tri_a, tri_b, tri_c) >= 0.0;

        double min_cos = -2.0; // smallest angle -> largest dot product with +X
        size_t candidate_idx = visible_outer_idx;
        bool found_reflex_inside = false;

        for (size_t k = 0; k < n; ++k) {
            if (k == visible_outer_idx) continue;
            const cad2d::Point2D& R = outer[k].pt;
            if (R.x < M.x - 1e-12) continue;

            const bool inside = ccw_tri
                ? point_in_triangle_2d(R, tri_a, tri_b, tri_c)
                : point_in_triangle_2d(R, tri_a, tri_c, tri_b);
            if (!inside) continue;

            const double dx = R.x - M.x;
            const double dy = R.y - M.y;
            const double len = std::hypot(dx, dy);
            if (len <= 1e-12) continue;
            const double cos_theta = dx / len;
            if (!found_reflex_inside || cos_theta > min_cos) {
                found_reflex_inside = true;
                min_cos = cos_theta;
                candidate_idx = k;
            }
        }
        if (found_reflex_inside) {
            visible_outer_idx = candidate_idx;
        }
    }

    // 4. Splice hole into outer at visible_outer_idx -> hole_max_i
    std::vector<IndexedPt> merged;
    merged.reserve(outer.size() + hole.size() + 2);
    for (size_t i = 0; i <= visible_outer_idx; ++i) {
        merged.push_back(outer[i]);
    }
    for (size_t j = 0; j < hole.size(); ++j) {
        merged.push_back(hole[(hole_max_i + j) % hole.size()]);
    }
    merged.push_back(hole[hole_max_i]);
    merged.push_back(outer[visible_outer_idx]);
    for (size_t i = visible_outer_idx + 1; i < outer.size(); ++i) {
        merged.push_back(outer[i]);
    }
    outer = std::move(merged);
}

} // namespace

TessellatedPolygon2D PolygonTessellator::triangulate(
    const cad2d::Path2D& outer,
    const cad2d::Paths2D& holes
) {
    TessellatedPolygon2D result;
    const cad2d::Path2D outer_ccw = ensure_clean_orientation(outer, true);
    if (outer_ccw.size() < 3) return result;

    std::vector<IndexedPt> poly;
    poly.reserve(outer_ccw.size());
    for (const auto& pt : outer_ccw) {
        const uint32_t idx = static_cast<uint32_t>(result.vertices.size());
        result.vertices.push_back(pt);
        poly.push_back({pt, idx});
    }
    result.loop_sizes.push_back(static_cast<uint32_t>(outer_ccw.size()));

    // Sort holes by descending max X for Eberly bridging while preserving exact input order in result.vertices
    struct HoleItem {
        std::vector<IndexedPt> pts;
        double max_x{0.0};
    };
    std::vector<HoleItem> hole_list;
    for (const auto& h : holes) {
        const cad2d::Path2D hole_cw = ensure_clean_orientation(h, false);
        if (hole_cw.size() < 3) continue;
        result.loop_sizes.push_back(static_cast<uint32_t>(hole_cw.size()));
        HoleItem item;
        item.max_x = -std::numeric_limits<double>::infinity();
        for (const auto& pt : hole_cw) {
            const uint32_t idx = static_cast<uint32_t>(result.vertices.size());
            result.vertices.push_back(pt);
            item.pts.push_back({pt, idx});
            item.max_x = std::max(item.max_x, pt.x);
        }
        hole_list.push_back(std::move(item));
    }
    std::sort(hole_list.begin(), hole_list.end(), [](const HoleItem& a, const HoleItem& b) {
        return a.max_x > b.max_x;
    });

    for (const auto& item : hole_list) {
        bridge_hole_into_outer(poly, item.pts);
    }

    // Ear clipping over bridged simple CCW loop
    int guard = static_cast<int>(poly.size()) * 4 + 16;
    while (poly.size() >= 3 && guard-- > 0) {
        if (poly.size() == 3) {
            const double area2 = cross_p(poly[0].pt, poly[1].pt, poly[2].pt);
            if (std::abs(area2) > 1e-14) {
                result.triangles.push_back({poly[0].orig_idx, poly[1].orig_idx, poly[2].orig_idx});
            }
            break;
        }

        bool clipped = false;
        const size_t n = poly.size();
        for (size_t i = 0; i < n; ++i) {
            const size_t prev = (i + n - 1) % n;
            const size_t next = (i + 1) % n;
            const auto& A = poly[prev];
            const auto& B = poly[i];
            const auto& C = poly[next];

            const double cross_val = cross_p(A.pt, B.pt, C.pt);
            if (cross_val <= 1e-12) {
                continue; // Reflex or collinear vertex; preserve straight/bridge vertices for topological matching
            }

            bool contains_other = false;
            for (size_t j = 0; j < n; ++j) {
                if (j == prev || j == i || j == next) continue;
                if (poly[j].orig_idx == A.orig_idx ||
                    poly[j].orig_idx == B.orig_idx ||
                    poly[j].orig_idx == C.orig_idx) {
                    continue;
                }
                if (point_in_triangle_2d(poly[j].pt, A.pt, B.pt, C.pt)) {
                    contains_other = true;
                    break;
                }
            }
            if (contains_other) continue;

            result.triangles.push_back({A.orig_idx, B.orig_idx, C.orig_idx});
            poly.erase(poly.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }

        if (!clipped) {
            // Pass 2: Allow erasing duplicate adjacent vertex (M_1 -> M_2) or true collinear hairpin before reflex fallback
            bool cleaned_collinear = false;
            for (size_t i = 0; i < n; ++i) {
                const size_t prev = (i + n - 1) % n;
                const size_t next = (i + 1) % n;
                const auto& A = poly[prev];
                const auto& B = poly[i];
                const auto& C = poly[next];
                if (A.orig_idx == B.orig_idx || B.orig_idx == C.orig_idx || A.orig_idx == C.orig_idx) {
                    poly.erase(poly.begin() + static_cast<std::ptrdiff_t>(i));
                    cleaned_collinear = true;
                    break;
                }
            }
            if (cleaned_collinear) continue;

            // Degenerate fallback
            size_t fallback_i = 0;
            double best_cross = -std::numeric_limits<double>::infinity();
            for (size_t i = 0; i < n; ++i) {
                const size_t prev = (i + n - 1) % n;
                const size_t next = (i + 1) % n;
                const double cv = cross_p(poly[prev].pt, poly[i].pt, poly[next].pt);
                if (cv >= best_cross) {
                    best_cross = cv;
                    fallback_i = i;
                }
            }
            const size_t prev = (fallback_i + n - 1) % n;
            const size_t next = (fallback_i + 1) % n;
            if (best_cross > 1e-14) {
                result.triangles.push_back({poly[prev].orig_idx, poly[fallback_i].orig_idx, poly[next].orig_idx});
            }
            poly.erase(poly.begin() + static_cast<std::ptrdiff_t>(fallback_i));
        }
    }

    return result;
}

} // namespace shunya::cad3d
