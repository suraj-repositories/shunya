#include "shunya/pcb/wire_router_2d.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace shunya::pcb {

namespace {

core::base::Error make_wr_error(std::string msg) {
    return core::base::Error{
        .code = 5005,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

struct GridKey {
    int x{0};
    int y{0};
    int dir{4}; // 0:R, 1:L, 2:U, 3:D, 4:Start

    bool operator==(const GridKey& o) const noexcept {
        return x == o.x && y == o.y && dir == o.dir;
    }
};

struct GridKeyHash {
    size_t operator()(const GridKey& k) const noexcept {
        size_t h1 = std::hash<int>{}(k.x);
        size_t h2 = std::hash<int>{}(k.y);
        size_t h3 = std::hash<int>{}(k.dir);
        return h1 ^ (h2 * 2654435761u) ^ (h3 << 7);
    }
};

bool inside_obstacle(double px, double py, const std::vector<SchematicComponent>& obstacles,
                     double margin_mm, double s_x, double s_y, double g_x, double g_y) noexcept {
    // Do not block the start or goal pin locations themselves
    if ((std::abs(px - s_x) < 1e-3 && std::abs(py - s_y) < 1e-3) ||
        (std::abs(px - g_x) < 1e-3 && std::abs(py - g_y) < 1e-3)) {
        return false;
    }
    for (const auto& c : obstacles) {
        if (px >= c.center_mm.x - c.half_width_mm - margin_mm &&
            px <= c.center_mm.x + c.half_width_mm + margin_mm &&
            py >= c.center_mm.y - c.half_height_mm - margin_mm &&
            py <= c.center_mm.y + c.half_height_mm + margin_mm) {
            return true;
        }
    }
    return false;
}

bool point_strictly_interior_to_seg(const core::base::math::Vector2<double>& p,
                                    const WireSegment2D& seg,
                                    double tol = 1e-3) noexcept {
    const double min_x = std::min(seg.start_mm.x, seg.end_mm.x) - tol;
    const double max_x = std::max(seg.start_mm.x, seg.end_mm.x) + tol;
    const double min_y = std::min(seg.start_mm.y, seg.end_mm.y) - tol;
    const double max_y = std::max(seg.start_mm.y, seg.end_mm.y) + tol;
    if (p.x < min_x || p.x > max_x || p.y < min_y || p.y > max_y) return false;

    const bool at_start = std::abs(p.x - seg.start_mm.x) <= tol && std::abs(p.y - seg.start_mm.y) <= tol;
    const bool at_end = std::abs(p.x - seg.end_mm.x) <= tol && std::abs(p.y - seg.end_mm.y) <= tol;
    if (at_start || at_end) return false;

    const auto ab = seg.end_mm - seg.start_mm;
    const double len = ab.length();
    if (len <= tol) return false;
    const double area2 = std::abs(ab.x * (p.y - seg.start_mm.y) - ab.y * (p.x - seg.start_mm.x));
    return (area2 / len) <= tol;
}

} // namespace

core::base::Result<OrthogonalRouteResult> OrthogonalWireRouter::route(
    const core::base::math::Vector2<double>& start_mm,
    const core::base::math::Vector2<double>& goal_mm,
    const std::vector<SchematicComponent>& obstacles,
    double grid_pitch_mm,
    double margin_mm) {
    if (grid_pitch_mm <= 1e-6) {
        return make_wr_error("Grid pitch must be positive");
    }

    auto to_gx = [&](double x) { return static_cast<int>(std::llround(x / grid_pitch_mm)); };
    auto to_gy = [&](double y) { return static_cast<int>(std::llround(y / grid_pitch_mm)); };
    const int sx = to_gx(start_mm.x);
    const int sy = to_gy(start_mm.y);
    const int gx = to_gx(goal_mm.x);
    const int gy = to_gy(goal_mm.y);

    if (sx == gx && sy == gy) {
        OrthogonalRouteResult trivial;
        trivial.waypoints_mm = {start_mm};
        return trivial;
    }

    // Determine search window bounding box
    int min_x = std::min(sx, gx) - 12;
    int max_x = std::max(sx, gx) + 12;
    int min_y = std::min(sy, gy) - 12;
    int max_y = std::max(sy, gy) + 12;
    for (const auto& c : obstacles) {
        min_x = std::min(min_x, to_gx(c.center_mm.x - c.half_width_mm) - 4);
        max_x = std::max(max_x, to_gx(c.center_mm.x + c.half_width_mm) + 4);
        min_y = std::min(min_y, to_gy(c.center_mm.y - c.half_height_mm) - 4);
        max_y = std::max(max_y, to_gy(c.center_mm.y + c.half_height_mm) + 4);
    }

    struct QItem {
        double f;
        double g;
        GridKey key;
        bool operator>(const QItem& o) const noexcept { return f > o.f; }
    };

    std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;
    std::unordered_map<GridKey, double, GridKeyHash> best_g;
    std::unordered_map<GridKey, GridKey, GridKeyHash> parent;

    const GridKey start_k{sx, sy, 4};
    best_g[start_k] = 0.0;
    pq.push({static_cast<double>(std::abs(gx - sx) + std::abs(gy - sy)), 0.0, start_k});

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    GridKey found_goal{gx, gy, -1};

    size_t expansions = 0;
    constexpr size_t kMaxExpansions = 150000;

    while (!pq.empty() && expansions < kMaxExpansions) {
        const auto cur = pq.top();
        pq.pop();
        ++expansions;

        if (cur.key.x == gx && cur.key.y == gy) {
            found_goal = cur.key;
            break;
        }
        auto it_bg = best_g.find(cur.key);
        if (it_bg != best_g.end() && cur.g > it_bg->second + 1e-9) continue;

        for (int d = 0; d < 4; ++d) {
            const int nx = cur.key.x + dx[d];
            const int ny = cur.key.y + dy[d];
            if (nx < min_x || nx > max_x || ny < min_y || ny > max_y) continue;

            const double wx = static_cast<double>(nx) * grid_pitch_mm;
            const double wy = static_cast<double>(ny) * grid_pitch_mm;
            if (inside_obstacle(wx, wy, obstacles, margin_mm,
                                static_cast<double>(sx) * grid_pitch_mm,
                                static_cast<double>(sy) * grid_pitch_mm,
                                static_cast<double>(gx) * grid_pitch_mm,
                                static_cast<double>(gy) * grid_pitch_mm)) {
                continue;
            }

            const double bend_cost = (cur.key.dir != 4 && cur.key.dir != d) ? 0.25 : 0.0;
            const double ng = cur.g + 1.0 + bend_cost;
            const GridKey nk{nx, ny, d};
            auto it_n = best_g.find(nk);
            if (it_n == best_g.end() || ng < it_n->second - 1e-9) {
                best_g[nk] = ng;
                parent[nk] = cur.key;
                const double h = static_cast<double>(std::abs(gx - nx) + std::abs(gy - ny));
                pq.push({ng + h, ng, nk});
            }
        }
    }

    if (found_goal.dir == -1) {
        return make_wr_error("Orthogonal wire router could not find collision-free path");
    }

    std::vector<core::base::math::Vector2<double>> raw_pts;
    GridKey trace = found_goal;
    while (!(trace == start_k)) {
        raw_pts.push_back({static_cast<double>(trace.x) * grid_pitch_mm,
                           static_cast<double>(trace.y) * grid_pitch_mm});
        trace = parent[trace];
    }
    raw_pts.push_back({static_cast<double>(sx) * grid_pitch_mm,
                       static_cast<double>(sy) * grid_pitch_mm});
    std::reverse(raw_pts.begin(), raw_pts.end());
    raw_pts.front() = start_mm;
    raw_pts.back() = goal_mm;

    // Simplify collinear intermediate waypoints while guaranteeing strict orthogonal segments
    OrthogonalRouteResult res;
    res.waypoints_mm.push_back(raw_pts.front());
    for (size_t i = 1; i + 1 < raw_pts.size(); ++i) {
        const auto& prev = res.waypoints_mm.back();
        const auto& cur = raw_pts[i];
        const auto& next = raw_pts[i + 1];
        const bool same_x = std::abs(prev.x - cur.x) < 1e-6 && std::abs(cur.x - next.x) < 1e-6;
        const bool same_y = std::abs(prev.y - cur.y) < 1e-6 && std::abs(cur.y - next.y) < 1e-6;
        if (!same_x && !same_y) {
            res.waypoints_mm.push_back(cur);
        }
    }
    res.waypoints_mm.push_back(raw_pts.back());

    for (size_t i = 0; i + 1 < res.waypoints_mm.size(); ++i) {
        res.segments.push_back({res.waypoints_mm[i], res.waypoints_mm[i + 1], ""});
    }
    res.inserted_junctions = detect_junctions(res.segments);
    return res;
}

std::vector<core::base::math::Vector2<double>> OrthogonalWireRouter::detect_junctions(
    const std::vector<WireSegment2D>& segments) {
    std::vector<core::base::math::Vector2<double>> out;
    auto push_unique = [&](const core::base::math::Vector2<double>& p) {
        for (const auto& existing : out) {
            if (std::abs(existing.x - p.x) < 1e-3 && std::abs(existing.y - p.y) < 1e-3) return;
        }
        out.push_back(p);
    };

    for (size_t i = 0; i < segments.size(); ++i) {
        for (size_t j = 0; j < segments.size(); ++j) {
            if (i == j) continue;
            if (point_strictly_interior_to_seg(segments[i].start_mm, segments[j])) {
                push_unique(segments[i].start_mm);
            }
            if (point_strictly_interior_to_seg(segments[i].end_mm, segments[j])) {
                push_unique(segments[i].end_mm);
            }
        }
    }
    return out;
}

} // namespace shunya::pcb
