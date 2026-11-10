#include "shunya/cad2d/polygon_boolean.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

namespace shunya::cad2d {

namespace {

#if defined(__GNUC__) || defined(__clang__)
__extension__ using i128 = __int128;
#else
using i128 = long double;
#endif

struct IPt {
    int64_t x{0};
    int64_t y{0};

    constexpr bool operator==(const IPt& o) const noexcept { return x == o.x && y == o.y; }
    constexpr bool operator!=(const IPt& o) const noexcept { return !(*this == o); }
    constexpr bool operator<(const IPt& o) const noexcept { return x < o.x || (x == o.x && y < o.y); }
};

struct IPtHash {
    size_t operator()(const IPt& p) const noexcept {
        uint64_t h1 = static_cast<uint64_t>(p.x) * 0x9e3779b97f4a7c15ULL;
        uint64_t h2 = static_cast<uint64_t>(p.y) * 0xbf58476d1ce4e5b9ULL;
        return static_cast<size_t>(h1 ^ (h2 >> 17));
    }
};

struct RawSeg {
    IPt a;
    IPt b;
    int group{0}; // 0 = subject, 1 = clip
    int dir_w{1}; // +1 forward
};

i128 cross128(const IPt& o, const IPt& a, const IPt& b) noexcept {
    return static_cast<i128>(a.x - o.x) * static_cast<i128>(b.y - o.y) -
           static_cast<i128>(a.y - o.y) * static_cast<i128>(b.x - o.x);
}

bool point_on_bbox(const IPt& p, const IPt& a, const IPt& b) noexcept {
    int64_t minx = std::min(a.x, b.x), maxx = std::max(a.x, b.x);
    int64_t miny = std::min(a.y, b.y), maxy = std::max(a.y, b.y);
    return p.x >= minx && p.x <= maxx && p.y >= miny && p.y <= maxy;
}

int64_t div_round128(i128 num, i128 den) noexcept {
    if (den == 0) return 0;
    if (den < 0) { num = -num; den = -den; }
    if (num >= 0) {
        return static_cast<int64_t>((num + den / 2) / den);
    } else {
        return -static_cast<int64_t>(((-num) + den / 2) / den);
    }
}

bool eval_fill(int w, FillRule rule) noexcept {
    if (rule == FillRule::EvenOdd) return (w & 1) != 0;
    return w != 0;
}

bool eval_op(BooleanOp op, bool in_s, bool in_c) noexcept {
    switch (op) {
        case BooleanOp::Union:        return in_s || in_c;
        case BooleanOp::Intersection: return in_s && in_c;
        case BooleanOp::Difference:   return in_s && !in_c;
        case BooleanOp::Xor:          return in_s != in_c;
    }
    return false;
}

struct DirectedEdge {
    IPt u;
    IPt v;
    bool used{false};
};

} // namespace

Paths2D PolygonBooleanEngine::execute(
    BooleanOp op,
    const Paths2D& subject,
    const Paths2D& clip,
    FillRule fill_rule,
    double scale
) {
    if (scale <= 0.0) scale = DEFAULT_SCALE;

    auto to_ipt = [scale](const Point2D& p) -> IPt {
        return {
            static_cast<int64_t>(std::llround(p.x * scale)),
            static_cast<int64_t>(std::llround(p.y * scale))
        };
    };

    std::vector<RawSeg> orig_segs;
    auto ingest_paths = [&](const Paths2D& paths, int group) {
        for (const auto& path : paths) {
            if (path.size() < 3) continue;
            std::vector<IPt> pts;
            pts.reserve(path.size());
            for (const auto& p : path) {
                IPt q = to_ipt(p);
                if (pts.empty() || pts.back() != q) {
                    pts.push_back(q);
                }
            }
            if (pts.size() >= 2 && pts.front() == pts.back()) pts.pop_back();
            if (pts.size() < 3) continue;

            for (size_t i = 0; i < pts.size(); ++i) {
                IPt a = pts[i];
                IPt b = pts[(i + 1) % pts.size()];
                if (a != b) {
                    orig_segs.push_back({a, b, group, 1});
                }
            }
        }
    };

    ingest_paths(subject, 0);
    ingest_paths(clip, 1);

    if (orig_segs.empty()) return {};

    // Subdivide segments at intersections (2 passes handle any snap-rounding T-junctions)
    std::vector<RawSeg> segs = orig_segs;
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::vector<IPt>> splits(segs.size());
        for (size_t i = 0; i < segs.size(); ++i) {
            splits[i].push_back(segs[i].a);
            splits[i].push_back(segs[i].b);
        }

        for (size_t i = 0; i < segs.size(); ++i) {
            const auto& si = segs[i];
            int64_t minx_i = std::min(si.a.x, si.b.x), maxx_i = std::max(si.a.x, si.b.x);
            int64_t miny_i = std::min(si.a.y, si.b.y), maxy_i = std::max(si.a.y, si.b.y);

            for (size_t j = i + 1; j < segs.size(); ++j) {
                const auto& sj = segs[j];
                if (std::max(sj.a.x, sj.b.x) < minx_i || std::min(sj.a.x, sj.b.x) > maxx_i ||
                    std::max(sj.a.y, sj.b.y) < miny_i || std::min(sj.a.y, sj.b.y) > maxy_i) {
                    continue;
                }

                i128 c1 = cross128(si.a, si.b, sj.a);
                i128 c2 = cross128(si.a, si.b, sj.b);
                i128 c3 = cross128(sj.a, sj.b, si.a);
                i128 c4 = cross128(sj.a, sj.b, si.b);

                if (c1 == 0 && c2 == 0) {
                    // Collinear overlap
                    if (point_on_bbox(sj.a, si.a, si.b)) splits[i].push_back(sj.a);
                    if (point_on_bbox(sj.b, si.a, si.b)) splits[i].push_back(sj.b);
                    if (point_on_bbox(si.a, sj.a, sj.b)) splits[j].push_back(si.a);
                    if (point_on_bbox(si.b, sj.a, sj.b)) splits[j].push_back(si.b);
                    continue;
                }

                bool s1 = (c1 >= 0 && c2 <= 0) || (c1 <= 0 && c2 >= 0);
                bool s2 = (c3 >= 0 && c4 <= 0) || (c3 <= 0 && c4 >= 0);
                if (!s1 || !s2) continue;

                // Endpoint equality
                if (si.a == sj.a || si.a == sj.b || si.b == sj.a || si.b == sj.b) continue;

                // Compute intersection point
                i128 denom = static_cast<i128>(si.b.x - si.a.x) * static_cast<i128>(sj.b.y - sj.a.y) -
                             static_cast<i128>(si.b.y - si.a.y) * static_cast<i128>(sj.b.x - sj.a.x);
                if (denom == 0) continue;

                i128 num_t = static_cast<i128>(sj.a.x - si.a.x) * static_cast<i128>(sj.b.y - sj.a.y) -
                             static_cast<i128>(sj.a.y - si.a.y) * static_cast<i128>(sj.b.x - sj.a.x);

                IPt q{
                    si.a.x + div_round128(static_cast<i128>(si.b.x - si.a.x) * num_t, denom),
                    si.a.y + div_round128(static_cast<i128>(si.b.y - si.a.y) * num_t, denom)
                };

                if (point_on_bbox(q, si.a, si.b) && point_on_bbox(q, sj.a, sj.b)) {
                    splits[i].push_back(q);
                    splits[j].push_back(q);
                }
            }
        }

        std::vector<RawSeg> next_segs;
        next_segs.reserve(segs.size());
        bool any_split = false;

        for (size_t i = 0; i < segs.size(); ++i) {
            auto& pts = splits[i];
            const IPt a = segs[i].a;
            const IPt b = segs[i].b;
            const int64_t dx = b.x - a.x;
            const int64_t dy = b.y - a.y;

            std::sort(pts.begin(), pts.end(), [&](const IPt& p1, const IPt& p2) {
                i128 d1 = static_cast<i128>(p1.x - a.x) * dx + static_cast<i128>(p1.y - a.y) * dy;
                i128 d2 = static_cast<i128>(p2.x - a.x) * dx + static_cast<i128>(p2.y - a.y) * dy;
                return d1 < d2;
            });
            pts.erase(std::unique(pts.begin(), pts.end()), pts.end());

            if (pts.size() > 2) any_split = true;
            for (size_t k = 0; k + 1 < pts.size(); ++k) {
                if (pts[k] != pts[k + 1]) {
                    next_segs.push_back({pts[k], pts[k + 1], segs[i].group, segs[i].dir_w});
                }
            }
        }
        segs = std::move(next_segs);
        if (!any_split) break;
    }

    // Group elementary edges by canonical undirected key (min(u,v), max(u,v))
    struct EdgeKey {
        IPt p; // p < q
        IPt q;
        bool operator<(const EdgeKey& o) const noexcept {
            return p < o.p || (p == o.p && q < o.q);
        }
        bool operator==(const EdgeKey& o) const noexcept { return p == o.p && q == o.q; }
    };

    struct CoincidentWinding {
        int subj_delta{0};
        int clip_delta{0};
    };

    std::map<EdgeKey, CoincidentWinding> unique_edges;
    for (const auto& s : segs) {
        bool forward = (s.a < s.b);
        EdgeKey key = forward ? EdgeKey{s.a, s.b} : EdgeKey{s.b, s.a};
        auto& cw = unique_edges[key];

        // Non-vertical canonical (p.x < q.x): below = above + coincident(-dx)
        // Vertical canonical (p.x == q.x, p.y < q.y): left = right + coincident(+dy)
        int sign = forward ? 1 : -1;
        int contrib = (key.p.x < key.q.x) ? (-sign) : (+sign);
        if (s.group == 0) cw.subj_delta += contrib;
        else cw.clip_delta += contrib;
    }

    // Determine side winding via exact doubled-coordinate scanline ray test against split segments
    std::vector<DirectedEdge> boundary_edges;
    boundary_edges.reserve(unique_edges.size());

    for (const auto& [key, cw] : unique_edges) {
        const IPt P = key.p;
        const IPt Q = key.q;
        i128 mx2 = static_cast<i128>(P.x) + static_cast<i128>(Q.x);
        i128 my2 = static_cast<i128>(P.y) + static_cast<i128>(Q.y);

        int ws_sideA = 0, wc_sideA = 0; // Above (if non-vertical) OR Left (if vertical)

        if (P.x < Q.x) {
            // Shoot upward vertical ray at x = mx2 / 2
            for (const auto& s : segs) {
                i128 ax2 = static_cast<i128>(s.a.x) * 2;
                i128 bx2 = static_cast<i128>(s.b.x) * 2;
                if (ax2 == bx2) continue;

                bool fwd = (ax2 < bx2);
                i128 minx2 = fwd ? ax2 : bx2;
                i128 maxx2 = fwd ? bx2 : ax2;
                if (!(minx2 <= mx2 && mx2 < maxx2)) continue;

                // Skip edges sharing the exact same undirected key
                if ((s.a == P && s.b == Q) || (s.a == Q && s.b == P)) continue;

                // Evaluate y(mx2/2) > my2/2 without division
                i128 ay2 = static_cast<i128>(s.a.y) * 2;
                i128 dx = static_cast<i128>(s.b.x - s.a.x);
                i128 dy = static_cast<i128>(s.b.y - s.a.y);
                i128 lhs = dy * (mx2 - ax2);
                i128 rhs = (my2 - ay2) * dx;
                bool strictly_above = (dx > 0) ? (lhs > rhs) : (lhs < rhs);
                if (strictly_above) {
                    int sign = fwd ? -1 : 1; // -dir_x
                    if (s.group == 0) ws_sideA += sign;
                    else wc_sideA += sign;
                }
            }

            int ws_sideB = ws_sideA + cw.subj_delta;
            int wc_sideB = wc_sideA + cw.clip_delta;

            bool in_A = eval_op(op, eval_fill(ws_sideA, fill_rule), eval_fill(wc_sideA, fill_rule));
            bool in_B = eval_op(op, eval_fill(ws_sideB, fill_rule), eval_fill(wc_sideB, fill_rule));

            if (in_A != in_B) {
                // Interior on LEFT of directed edge: for P.x < Q.x, LEFT is ABOVE (sideA)
                if (in_A) boundary_edges.push_back({P, Q, false});
                else      boundary_edges.push_back({Q, P, false});
            }
        } else {
            // Vertical edge P.x == Q.x, P.y < Q.y: shoot rightward horizontal ray at y = my2 / 2
            for (const auto& s : segs) {
                i128 ay2 = static_cast<i128>(s.a.y) * 2;
                i128 by2 = static_cast<i128>(s.b.y) * 2;
                if (ay2 == by2) continue;

                bool fwd = (ay2 < by2);
                i128 miny2 = fwd ? ay2 : by2;
                i128 maxy2 = fwd ? by2 : ay2;
                if (!(miny2 <= my2 && my2 < maxy2)) continue;

                if ((s.a == P && s.b == Q) || (s.a == Q && s.b == P)) continue;

                // Evaluate x(my2/2) > mx2/2
                i128 ax2 = static_cast<i128>(s.a.x) * 2;
                i128 dx = static_cast<i128>(s.b.x - s.a.x);
                i128 dy = static_cast<i128>(s.b.y - s.a.y);
                i128 lhs = dx * (my2 - ay2);
                i128 rhs = (mx2 - ax2) * dy;
                bool strictly_right = (dy > 0) ? (lhs > rhs) : (lhs < rhs);
                if (strictly_right) {
                    int sign = fwd ? 1 : -1; // +dir_y
                    if (s.group == 0) ws_sideA += sign;
                    else wc_sideA += sign;
                }
            }

            // sideA = right_winding, sideB = left_winding
            int ws_sideB = ws_sideA + cw.subj_delta;
            int wc_sideB = wc_sideA + cw.clip_delta;

            bool in_right = eval_op(op, eval_fill(ws_sideA, fill_rule), eval_fill(wc_sideA, fill_rule));
            bool in_left  = eval_op(op, eval_fill(ws_sideB, fill_rule), eval_fill(wc_sideB, fill_rule));

            if (in_left != in_right) {
                // Interior on LEFT of directed edge: for P.y < Q.y (upward), LEFT is left
                if (in_left) boundary_edges.push_back({P, Q, false});
                else         boundary_edges.push_back({Q, P, false});
            }
        }
    }

    if (boundary_edges.empty()) return {};

    // Trace closed loops by sharpest right turn (minimum interior left turn without pinching)
    std::unordered_map<IPt, std::vector<size_t>, IPtHash> adj;
    for (size_t i = 0; i < boundary_edges.size(); ++i) {
        adj[boundary_edges[i].u].push_back(i);
    }

    Paths2D result_paths;
    double inv_scale = 1.0 / scale;

    for (size_t start_idx = 0; start_idx < boundary_edges.size(); ++start_idx) {
        if (boundary_edges[start_idx].used) continue;

        std::vector<IPt> loop;
        size_t curr_idx = start_idx;
        bool closed_ok = false;

        while (true) {
            auto& e = boundary_edges[curr_idx];
            if (e.used) break;
            e.used = true;
            loop.push_back(e.u);

            if (e.v == loop.front()) {
                closed_ok = true;
                break;
            }

            auto it = adj.find(e.v);
            if (it == adj.end()) break;

            // Choose unused outgoing edge from e.v making sharpest right turn (keeps interior loops uncrossed)
            double in_ang = std::atan2(static_cast<double>(e.u.y - e.v.y), static_cast<double>(e.u.x - e.v.x));
            size_t best_next = static_cast<size_t>(-1);
            double best_cw_delta = 1e9;

            for (size_t cand : it->second) {
                if (boundary_edges[cand].used) continue;
                const auto& ne = boundary_edges[cand];
                double out_ang = std::atan2(static_cast<double>(ne.v.y - ne.u.y), static_cast<double>(ne.v.x - ne.u.x));
                double cw_delta = in_ang - out_ang;
                while (cw_delta <= 0.0) cw_delta += 2.0 * core::base::math::PI;
                if (cw_delta < best_cw_delta) {
                    best_cw_delta = cw_delta;
                    best_next = cand;
                }
            }

            if (best_next == static_cast<size_t>(-1)) break;
            curr_idx = best_next;
        }

        if (closed_ok && loop.size() >= 3) {
            // Clean redundant collinear intermediate vertices
            std::vector<IPt> cleaned;
            for (size_t i = 0; i < loop.size(); ++i) {
                IPt prev = loop[(i + loop.size() - 1) % loop.size()];
                IPt curr = loop[i];
                IPt next = loop[(i + 1) % loop.size()];
                if (cross128(prev, curr, next) == 0) {
                    i128 dot = static_cast<i128>(curr.x - prev.x) * static_cast<i128>(next.x - curr.x) +
                               static_cast<i128>(curr.y - prev.y) * static_cast<i128>(next.y - curr.y);
                    if (dot > 0) continue; // Redundant collinear vertex
                }
                cleaned.push_back(curr);
            }

            if (cleaned.size() >= 3) {
                Path2D out_path;
                out_path.reserve(cleaned.size());
                for (const auto& ip : cleaned) {
                    out_path.push_back({static_cast<double>(ip.x) * inv_scale, static_cast<double>(ip.y) * inv_scale});
                }
                if (std::abs(signed_area(out_path)) > 1e-12) {
                    result_paths.push_back(std::move(out_path));
                }
            }
        }
    }

    return result_paths;
}

Paths2D PolygonBooleanEngine::simplify(
    const Paths2D& polygons,
    FillRule fill_rule,
    double scale
) {
    return execute(BooleanOp::Union, polygons, {}, fill_rule, scale);
}

double PolygonBooleanEngine::signed_area(const Path2D& path) noexcept {
    if (path.size() < 3) return 0.0;
    double area2 = 0.0;
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& a = path[i];
        const auto& b = path[(i + 1) % path.size()];
        area2 += (a.x * b.y - b.x * a.y);
    }
    return 0.5 * area2;
}

double PolygonBooleanEngine::net_area(const Paths2D& paths) noexcept {
    double sum = 0.0;
    for (const auto& p : paths) {
        sum += signed_area(p);
    }
    return sum;
}

bool PolygonBooleanEngine::point_in_path(const Point2D& pt, const Path2D& path) noexcept {
    if (path.size() < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = path.size() - 1; i < path.size(); j = i++) {
        const auto& a = path[i];
        const auto& b = path[j];
        if (((a.y > pt.y) != (b.y > pt.y)) &&
            (pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y) + a.x)) {
            inside = !inside;
        }
    }
    return inside;
}

std::vector<PolygonWithHoles2D> PolygonBooleanEngine::extract_hierarchy(const Paths2D& paths) {
    std::vector<PolygonWithHoles2D> hierarchy;
    Paths2D holes;

    for (const auto& p : paths) {
        if (signed_area(p) > 0.0) {
            hierarchy.push_back(PolygonWithHoles2D{p, {}});
        } else if (signed_area(p) < 0.0) {
            holes.push_back(p);
        }
    }

    for (const auto& h : holes) {
        if (h.empty()) continue;
        // Find smallest-area enclosing outer polygon
        size_t best_outer = static_cast<size_t>(-1);
        double best_area = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < hierarchy.size(); ++i) {
            double outer_a = area(hierarchy[i].outer);
            if (outer_a < best_area && point_in_path(h.front(), hierarchy[i].outer)) {
                best_area = outer_a;
                best_outer = i;
            }
        }
        if (best_outer != static_cast<size_t>(-1)) {
            hierarchy[best_outer].holes.push_back(h);
        }
    }

    return hierarchy;
}

} // namespace shunya::cad2d
