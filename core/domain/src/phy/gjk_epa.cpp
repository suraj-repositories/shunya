#include "shunya/phy/gjk_epa.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace shunya::phy {

using namespace core::domain::mechanics;

namespace {

struct SupportVertex {
    Vector3d p{0.0, 0.0, 0.0}; // p_a - p_b in Minkowski space
    Vector3d a{0.0, 0.0, 0.0};
    Vector3d b{0.0, 0.0, 0.0};
};

SupportVertex eval_minkowski_support(
    const ConvexCollider& ca,
    const ConvexCollider& cb,
    const Vector3d& dir) noexcept {
    Vector3d d = dir;
    if (d.length_squared() < 1e-18) d = {1.0, 0.0, 0.0};
    const Vector3d pa = ca.support_world(d);
    const Vector3d pb = cb.support_world(-d);
    return {pa - pb, pa, pb};
}

// Computes closest point to origin on triangle ABC with barycentric weights
GjkEpaResult closest_on_triangle(const SupportVertex& a, const SupportVertex& b, const SupportVertex& c) noexcept {
    const Vector3d ab = b.p - a.p;
    const Vector3d ac = c.p - a.p;
    const Vector3d ap = -a.p;

    const double d1 = ab.dot(ap);
    const double d2 = ac.dot(ap);
    if (d1 <= 0.0 && d2 <= 0.0) {
        const double dist = a.p.length();
        return {dist < 1e-12, dist, 0.0, (dist > 1e-12) ? (a.p / dist) : Vector3d{0.0, 1.0, 0.0}, a.a, a.b};
    }

    const Vector3d bp = -b.p;
    const double d3 = ab.dot(bp);
    const double d4 = ac.dot(bp);
    if (d3 >= 0.0 && d4 <= d3) {
        const double dist = b.p.length();
        return {dist < 1e-12, dist, 0.0, (dist > 1e-12) ? (b.p / dist) : Vector3d{0.0, 1.0, 0.0}, b.a, b.b};
    }

    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        const double v = d1 / std::max(1e-18, d1 - d3);
        const Vector3d cp = a.p + ab * v;
        const double dist = cp.length();
        return {dist < 1e-12, dist, 0.0, (dist > 1e-12) ? (cp / dist) : Vector3d{0.0, 1.0, 0.0},
                a.a * (1.0 - v) + b.a * v, a.b * (1.0 - v) + b.b * v};
    }

    const Vector3d cpv = -c.p;
    const double d5 = ab.dot(cpv);
    const double d6 = ac.dot(cpv);
    if (d6 >= 0.0 && d5 <= d6) {
        const double dist = c.p.length();
        return {dist < 1e-12, dist, 0.0, (dist > 1e-12) ? (c.p / dist) : Vector3d{0.0, 1.0, 0.0}, c.a, c.b};
    }

    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        const double w = d2 / std::max(1e-18, d2 - d6);
        const Vector3d cp = a.p + ac * w;
        const double dist = cp.length();
        return {dist < 1e-12, dist, 0.0, (dist > 1e-12) ? (cp / dist) : Vector3d{0.0, 1.0, 0.0},
                a.a * (1.0 - w) + c.a * w, a.b * (1.0 - w) + c.b * w};
    }

    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        const double w = (d4 - d3) / std::max(1e-18, (d4 - d3) + (d5 - d6));
        const Vector3d cp = b.p + (c.p - b.p) * w;
        const double dist = cp.length();
        return {dist < 1e-12, dist, 0.0, (dist > 1e-12) ? (cp / dist) : Vector3d{0.0, 1.0, 0.0},
                b.a * (1.0 - w) + c.a * w, b.b * (1.0 - w) + c.b * w};
    }

    const double denom = 1.0 / std::max(1e-18, va + vb + vc);
    const double v = vb * denom;
    const double w = vc * denom;
    const double u = 1.0 - v - w;
    const Vector3d cp = a.p * u + b.p * v + c.p * w;
    const double dist = cp.length();
    return {dist < 1e-12, dist, 0.0, (dist > 1e-12) ? (cp / dist) : ab.cross(ac).normalized(),
            a.a * u + b.a * v + c.a * w, a.b * u + b.b * v + c.b * w};
}

// Reduces simplex of size 1..3 to minimal active subset contributing to closest point to origin,
// or detects tetrahedron containment (size == 4). Returns true if origin is strictly enclosed.
bool reduce_simplex_closest(
    std::vector<SupportVertex>& simplex,
    SupportVertex& out_cp) noexcept {
    if (simplex.empty()) {
        out_cp = {};
        return false;
    }
    if (simplex.size() == 1) {
        out_cp = simplex[0];
        return false;
    }
    if (simplex.size() == 2) {
        const auto& a = simplex[0];
        const auto& b = simplex[1];
        const Vector3d ab = b.p - a.p;
        const double ab2 = ab.length_squared();
        const double t = (ab2 > 1e-18) ? std::clamp(-a.p.dot(ab) / ab2, 0.0, 1.0) : 0.0;
        out_cp = {a.p + ab * t, a.a * (1.0 - t) + b.a * t, a.b * (1.0 - t) + b.b * t};
        if (t <= 1e-12) simplex = {a};
        else if (t >= 1.0 - 1e-12) simplex = {b};
        return false;
    }
    if (simplex.size() == 3) {
        const auto a = simplex[0];
        const auto b = simplex[1];
        const auto c = simplex[2];
        const Vector3d ab = b.p - a.p;
        const Vector3d ac = c.p - a.p;
        const Vector3d ap = -a.p;

        const double d1 = ab.dot(ap), d2 = ac.dot(ap);
        if (d1 <= 0.0 && d2 <= 0.0) { simplex = {a}; out_cp = a; return false; }

        const Vector3d bp = -b.p;
        const double d3 = ab.dot(bp), d4 = ac.dot(bp);
        if (d3 >= 0.0 && d4 <= d3) { simplex = {b}; out_cp = b; return false; }

        const double vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
            const double v = d1 / std::max(1e-18, d1 - d3);
            simplex = {a, b};
            out_cp = {a.p * (1.0 - v) + b.p * v, a.a * (1.0 - v) + b.a * v, a.b * (1.0 - v) + b.b * v};
            return false;
        }

        const Vector3d cpv = -c.p;
        const double d5 = ab.dot(cpv), d6 = ac.dot(cpv);
        if (d6 >= 0.0 && d5 <= d6) { simplex = {c}; out_cp = c; return false; }

        const double vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
            const double w = d2 / std::max(1e-18, d2 - d6);
            simplex = {a, c};
            out_cp = {a.p * (1.0 - w) + c.p * w, a.a * (1.0 - w) + c.a * w, a.b * (1.0 - w) + c.b * w};
            return false;
        }

        const double va = d3 * d6 - d5 * d4;
        if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
            const double w = (d4 - d3) / std::max(1e-18, (d4 - d3) + (d5 - d6));
            simplex = {b, c};
            out_cp = {b.p * (1.0 - w) + c.p * w, b.a * (1.0 - w) + c.a * w, b.b * (1.0 - w) + c.b * w};
            return false;
        }

        const double denom = 1.0 / std::max(1e-18, va + vb + vc);
        const double v = vb * denom;
        const double w = vc * denom;
        const double u = 1.0 - v - w;
        out_cp = {a.p * u + b.p * v + c.p * w, a.a * u + b.a * v + c.a * w, a.b * u + b.b * v + c.b * w};
        return false;
    }

    // size == 4: check all 4 faces; if origin is on interior side of all 4 faces, intersecting!
    const auto a = simplex[0], b = simplex[1], c = simplex[2], d = simplex[3];
    const Vector3d interior = (a.p + b.p + c.p + d.p) * 0.25;
    std::array<std::array<SupportVertex, 3>, 4> faces{{
        {a, b, c}, {a, c, d}, {a, d, b}, {b, d, c}
    }};

    bool outside_any = false;
    double best_d2 = std::numeric_limits<double>::infinity();
    std::vector<SupportVertex> best_sub;
    SupportVertex best_cp;

    for (const auto& f : faces) {
        const Vector3d n = (f[1].p - f[0].p).cross(f[2].p - f[0].p);
        const Vector3d outward = (n.dot(f[0].p - interior) < 0.0) ? -n : n;
        if (outward.dot(-f[0].p) > 1e-12) {
            outside_any = true;
            std::vector<SupportVertex> sub{f[0], f[1], f[2]};
            SupportVertex cp;
            reduce_simplex_closest(sub, cp);
            const double d2 = cp.p.length_squared();
            if (d2 < best_d2) {
                best_d2 = d2;
                best_sub = std::move(sub);
                best_cp = cp;
            }
        }
    }

    if (!outside_any) {
        out_cp = {};
        return true; // Enclosed!
    }
    simplex = std::move(best_sub);
    out_cp = best_cp;
    return false;
}

GjkEpaResult run_epa(
    const ConvexCollider& ca,
    const ConvexCollider& cb,
    std::vector<SupportVertex> polytope) noexcept {
    // Expand simplex to non-degenerate 4-vertex tetrahedron if needed
    constexpr std::array<Vector3d, 6> kAxes{{
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    }};

    while (polytope.size() < 4) {
        if (polytope.size() == 1) {
            for (const auto& ax : kAxes) {
                auto v = eval_minkowski_support(ca, cb, ax);
                if ((v.p - polytope[0].p).length_squared() > 1e-12) {
                    polytope.push_back(v);
                    break;
                }
            }
            if (polytope.size() == 1) break;
        } else if (polytope.size() == 2) {
            const Vector3d seg = polytope[1].p - polytope[0].p;
            Vector3d best_dir = seg.cross(Vector3d{1, 0, 0});
            if (best_dir.length_squared() < 1e-12) best_dir = seg.cross(Vector3d{0, 1, 0});
            auto v = eval_minkowski_support(ca, cb, best_dir);
            if (seg.cross(v.p - polytope[0].p).length_squared() < 1e-12) {
                v = eval_minkowski_support(ca, cb, -best_dir);
            }
            polytope.push_back(v);
        } else if (polytope.size() == 3) {
            const Vector3d n = (polytope[1].p - polytope[0].p).cross(polytope[2].p - polytope[0].p);
            auto v = eval_minkowski_support(ca, cb, n);
            if (std::abs(n.dot(v.p - polytope[0].p)) < 1e-12) {
                v = eval_minkowski_support(ca, cb, -n);
            }
            polytope.push_back(v);
        }
    }

    if (polytope.size() < 4) {
        return {true, 0.0, 0.0, {0, 1, 0}, ca.position, cb.position};
    }

    struct EpaFace {
        size_t i0, i1, i2;
        Vector3d normal;
        double distance;
    };

    const Vector3d interior = (polytope[0].p + polytope[1].p + polytope[2].p + polytope[3].p) * 0.25;
    auto make_face = [&](size_t a, size_t b, size_t c) -> EpaFace {
        Vector3d n = (polytope[b].p - polytope[a].p).cross(polytope[c].p - polytope[a].p);
        if (n.dot(polytope[a].p - interior) < 0.0) {
            std::swap(b, c);
            n = -n;
        }
        const double len = n.length();
        n = (len > 1e-15) ? (n / len) : Vector3d{0, 1, 0};
        const double d = n.dot(polytope[a].p);
        return {a, b, c, n, d};
    };

    std::vector<EpaFace> faces{
        make_face(0, 1, 2),
        make_face(0, 2, 3),
        make_face(0, 3, 1),
        make_face(1, 3, 2)
    };

    for (int iter = 0; iter < GjkEpaSolver::kMaxEpaIterations; ++iter) {
        size_t best_idx = 0;
        double min_dist = faces[0].distance;
        for (size_t i = 1; i < faces.size(); ++i) {
            if (faces[i].distance < min_dist) {
                min_dist = faces[i].distance;
                best_idx = i;
            }
        }

        const EpaFace& bf = faces[best_idx];
        const SupportVertex sup = eval_minkowski_support(ca, cb, bf.normal);
        const double sup_dist = bf.normal.dot(sup.p);

        if (sup_dist - min_dist < 1e-5 || polytope.size() > 64) {
            // Interpolate witness points on bf using projected closest point
            const auto tri = closest_on_triangle(polytope[bf.i0], polytope[bf.i1], polytope[bf.i2]);
            // Note: EPA outward normal of (A - B) points A -> B, so B -> A separating normal is -bf.normal
            const double depth = std::max(0.0, min_dist);
            return {true, 0.0, depth, -bf.normal, tri.point_a, tri.point_b};
        }

        // Remove visible faces and rebuild horizon edges
        struct Edge {
            size_t a, b;
        };
        std::vector<Edge> horizon;
        std::vector<EpaFace> kept;
        kept.reserve(faces.size());

        auto add_horizon_edge = [&](size_t u, size_t v) {
            for (size_t k = 0; k < horizon.size(); ++k) {
                if (horizon[k].a == v && horizon[k].b == u) {
                    horizon.erase(horizon.begin() + static_cast<ptrdiff_t>(k));
                    return;
                }
            }
            horizon.push_back({u, v});
        };

        for (const auto& f : faces) {
            if (f.normal.dot(sup.p - polytope[f.i0].p) > 1e-8) {
                add_horizon_edge(f.i0, f.i1);
                add_horizon_edge(f.i1, f.i2);
                add_horizon_edge(f.i2, f.i0);
            } else {
                kept.push_back(f);
            }
        }

        if (horizon.empty()) {
            const auto tri = closest_on_triangle(polytope[bf.i0], polytope[bf.i1], polytope[bf.i2]);
            return {true, 0.0, std::max(0.0, min_dist), -bf.normal, tri.point_a, tri.point_b};
        }

        const size_t new_idx = polytope.size();
        polytope.push_back(sup);
        for (const auto& e : horizon) {
            kept.push_back(make_face(e.a, e.b, new_idx));
        }
        faces = std::move(kept);
    }

    size_t best_idx = 0;
    for (size_t i = 1; i < faces.size(); ++i) {
        if (faces[i].distance < faces[best_idx].distance) best_idx = i;
    }
    const auto& bf = faces[best_idx];
    const auto tri = closest_on_triangle(polytope[bf.i0], polytope[bf.i1], polytope[bf.i2]);
    return {true, 0.0, std::max(0.0, bf.distance), -bf.normal, tri.point_a, tri.point_b};
}

} // namespace

Vector3d ConvexCollider::support_world(const Vector3d& dir_world) const noexcept {
    const Vector3d d_local = orientation.conjugate().rotate(dir_world);
    Vector3d local_pt{0.0, 0.0, 0.0};

    switch (kind) {
        case ColliderKind::Sphere: {
            const double len = d_local.length();
            if (len > 1e-15) local_pt = d_local * (radius / len);
            break;
        }
        case ColliderKind::Box: {
            local_pt = {
                (d_local.x >= 0.0) ? half_extents.x : -half_extents.x,
                (d_local.y >= 0.0) ? half_extents.y : -half_extents.y,
                (d_local.z >= 0.0) ? half_extents.z : -half_extents.z
            };
            break;
        }
        case ColliderKind::Capsule: {
            const double len = d_local.length();
            const Vector3d sphere_part = (len > 1e-15) ? (d_local * (radius / len)) : Vector3d{0, 0, 0};
            const double cap_y = (d_local.y >= 0.0) ? half_height : -half_height;
            local_pt = {sphere_part.x, sphere_part.y + cap_y, sphere_part.z};
            break;
        }
        case ColliderKind::ConvexHull: {
            double best_dot = -std::numeric_limits<double>::infinity();
            for (const auto& v : hull_vertices) {
                const double dot = v.dot(d_local);
                if (dot > best_dot) {
                    best_dot = dot;
                    local_pt = v;
                }
            }
            break;
        }
    }

    return position + orientation.rotate(local_pt);
}

AABB ConvexCollider::world_aabb(double margin) const noexcept {
    Vector3d mn{
        support_world({-1, 0, 0}).x - margin,
        support_world({0, -1, 0}).y - margin,
        support_world({0, 0, -1}).z - margin
    };
    Vector3d mx{
        support_world({1, 0, 0}).x + margin,
        support_world({0, 1, 0}).y + margin,
        support_world({0, 0, 1}).z + margin
    };
    return {mn, mx};
}

double ConvexCollider::max_bounding_radius() const noexcept {
    switch (kind) {
        case ColliderKind::Sphere: return radius;
        case ColliderKind::Box: return half_extents.length();
        case ColliderKind::Capsule: return half_height + radius;
        case ColliderKind::ConvexHull: {
            double r2 = 0.0;
            for (const auto& v : hull_vertices) r2 = std::max(r2, v.length_squared());
            return std::sqrt(r2);
        }
    }
    return radius;
}

GjkEpaResult GjkEpaSolver::detect(const ConvexCollider& a, const ConvexCollider& b) noexcept {
    // Fast-path analytic sphere-sphere check
    if (a.kind == ColliderKind::Sphere && b.kind == ColliderKind::Sphere) {
        const Vector3d delta = a.position - b.position;
        const double dist_c = delta.length();
        const double r_sum = a.radius + b.radius;
        const Vector3d n = (dist_c > 1e-12) ? (delta / dist_c) : Vector3d{0.0, 1.0, 0.0};
        const Vector3d pa = a.position - n * a.radius;
        const Vector3d pb = b.position + n * b.radius;
        if (dist_c <= r_sum) {
            return {true, 0.0, r_sum - dist_c, n, pa, pb};
        }
        return {false, dist_c - r_sum, 0.0, n, pa, pb};
    }

    Vector3d dir = b.position - a.position;
    if (dir.length_squared() < 1e-18) dir = {1.0, 0.0, 0.0};

    std::vector<SupportVertex> simplex;
    simplex.reserve(4);
    simplex.push_back(eval_minkowski_support(a, b, dir));

    for (int iter = 0; iter < kMaxGjkIterations; ++iter) {
        SupportVertex cp;
        if (reduce_simplex_closest(simplex, cp)) {
            return run_epa(a, b, std::move(simplex));
        }

        const double dist = cp.p.length();
        if (dist < 1e-9) {
            return run_epa(a, b, std::move(simplex));
        }

        dir = -cp.p;
        const Vector3d dir_unit = dir / dist;
        const SupportVertex sup = eval_minkowski_support(a, b, dir_unit);

        // Signed distance progress toward origin along dir_unit
        const double gap = sup.p.dot(dir_unit) - cp.p.dot(dir_unit);
        if (gap <= 1e-7 * std::max(1.0, dist)) {
            return {false, dist, 0.0, cp.p / dist, cp.a, cp.b};
        }

        bool duplicate = false;
        for (const auto& sv : simplex) {
            if ((sv.p - sup.p).length_squared() < 1e-16) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            return {false, dist, 0.0, cp.p / dist, cp.a, cp.b};
        }

        simplex.push_back(sup);
    }

    SupportVertex cp;
    if (reduce_simplex_closest(simplex, cp)) {
        return run_epa(a, b, std::move(simplex));
    }
    const double dist = cp.p.length();
    if (dist < 1e-9) return run_epa(a, b, std::move(simplex));
    return {false, dist, 0.0, cp.p / dist, cp.a, cp.b};
}

} // namespace shunya::phy
