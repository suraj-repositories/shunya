#include "shunya/cad3d/extrusion_feature.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad3d {

namespace {

cad2d::Path2D clean_loop_for_extrusion(const cad2d::Path2D& in, bool want_ccw) {
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

/**
 * @brief Computes miter-offset vertices for a closed polygon loop (positive dist expands outer CCW outward).
 */
cad2d::Path2D miter_offset_loop(const cad2d::Path2D& loop, double dist) {
    if (std::abs(dist) <= 1e-12 || loop.size() < 3) return loop;
    const size_t n = loop.size();
    cad2d::Path2D out(n);
    for (size_t i = 0; i < n; ++i) {
        const cad2d::Point2D& prev = loop[(i + n - 1) % n];
        const cad2d::Point2D& curr = loop[i];
        const cad2d::Point2D& next = loop[(i + 1) % n];

        const cad2d::Vec2d e0 = (curr - prev).normalized();
        const cad2d::Vec2d e1 = (next - curr).normalized();
        // Outward right-hand normal for CCW loop is (dy, -dx)
        const cad2d::Vec2d n0{e0.y, -e0.x};
        const cad2d::Vec2d n1{e1.y, -e1.x};
        cad2d::Vec2d miter = n0 + n1;
        const double len = miter.length();
        if (len <= 1e-9) {
            out[i] = curr + n0 * dist;
        } else {
            miter = miter * (1.0 / len);
            const double denom = std::max(0.25, miter.dot(n0));
            out[i] = curr + miter * (dist / denom);
        }
    }
    return out;
}

bool point_in_poly(const cad2d::Point2D& pt, const cad2d::Path2D& poly) noexcept {
    bool inside = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto& a = poly[j];
        const auto& b = poly[i];
        if (((b.y > pt.y) != (a.y > pt.y)) &&
            (pt.x < (a.x - b.x) * (pt.y - b.y) / (a.y - b.y + 1e-30) + b.x)) {
            inside = !inside;
        }
    }
    return inside;
}

} // namespace

SolidMesh3D ExtrusionEngine::extrude(
    const cad2d::Path2D& outer,
    const cad2d::Paths2D& holes,
    const ExtrusionParams& params
) {
    SolidMesh3D mesh;
    const cad2d::Path2D clean_outer = clean_loop_for_extrusion(outer, true);
    if (clean_outer.size() < 3) return mesh;

    cad2d::Paths2D clean_holes;
    for (const auto& h : holes) {
        auto ch = clean_loop_for_extrusion(h, false);
        if (ch.size() >= 3) clean_holes.push_back(std::move(ch));
    }

    const TessellatedPolygon2D tess = PolygonTessellator::triangulate(clean_outer, clean_holes);
    if (tess.vertices.empty() || tess.triangles.empty()) return mesh;

    double z0 = 0.0;
    double z1 = params.depth;
    switch (params.end_condition) {
        case EndCondition::Blind:
            z0 = 0.0;
            z1 = params.depth;
            break;
        case EndCondition::Symmetric:
        case EndCondition::MidPlane:
            z0 = -params.depth * 0.5;
            z1 = params.depth * 0.5;
            break;
        case EndCondition::ThroughAll:
            z0 = -params.through_all_extent * 0.5;
            z1 = params.through_all_extent * 0.5;
            break;
    }
    if (z1 < z0) std::swap(z0, z1);
    const double span = z1 - z0;
    if (span <= 1e-12) return mesh;

    // Build top 2D vertex ring accounting for draft taper (matching tess.vertices & tess.loop_sizes order)
    const double draft_offset = span * std::tan(params.draft_angle_rad);
    std::vector<cad2d::Point2D> top_2d;
    top_2d.reserve(tess.vertices.size());
    {
        const auto off_outer = miter_offset_loop(clean_outer, draft_offset);
        for (const auto& pt : off_outer) top_2d.push_back(pt);
        for (const auto& h : clean_holes) {
            const auto off_h = miter_offset_loop(h, draft_offset);
            for (const auto& pt : off_h) top_2d.push_back(pt);
        }
    }

    const uint32_t n_verts = static_cast<uint32_t>(tess.vertices.size());
    if (top_2d.size() != n_verts) {
        top_2d = tess.vertices;
    }

    mesh.vertices.reserve(static_cast<size_t>(n_verts) * 2);
    for (uint32_t i = 0; i < n_verts; ++i) {
        mesh.add_vertex(params.plane.to_3d(tess.vertices[i], z0));
    }
    for (uint32_t i = 0; i < n_verts; ++i) {
        mesh.add_vertex(params.plane.to_3d(top_2d[i], z1));
    }

    // Bottom cap (reversed winding -> normal points along -plane.normal())
    for (const auto& tri : tess.triangles) {
        mesh.add_triangle(tri[0], tri[2], tri[1]);
    }
    // Top cap (CCW winding -> normal points along +plane.normal())
    for (const auto& tri : tess.triangles) {
        mesh.add_triangle(n_verts + tri[0], n_verts + tri[1], n_verts + tri[2]);
    }

    uint32_t cursor = 0;
    for (const uint32_t count : tess.loop_sizes) {
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t a = cursor + i;
            const uint32_t b = cursor + ((i + 1) % count);
            mesh.add_triangle(a, b, n_verts + b);
            mesh.add_triangle(a, n_verts + b, n_verts + a);
        }
        cursor += count;
    }

    mesh.ensure_outward_winding();
    return mesh;
}

std::vector<SolidMesh3D> ExtrusionEngine::extrude_paths(
    const cad2d::Paths2D& profile_paths,
    const ExtrusionParams& params
) {
    cad2d::Paths2D outers;
    cad2d::Paths2D holes;
    for (const auto& p : profile_paths) {
        if (p.size() < 3) continue;
        const double area = cad2d::PolygonBooleanEngine::signed_area(p);
        if (area >= 0.0) outers.push_back(p);
        else holes.push_back(p);
    }

    std::vector<SolidMesh3D> bodies;
    bodies.reserve(outers.size());
    for (const auto& outer : outers) {
        cad2d::Paths2D owned_holes;
        for (const auto& h : holes) {
            if (!h.empty() && point_in_poly(h[0], outer)) {
                owned_holes.push_back(h);
            }
        }
        auto mesh = extrude(outer, owned_holes, params);
        if (!mesh.triangles.empty()) {
            bodies.push_back(std::move(mesh));
        }
    }
    return bodies;
}

} // namespace shunya::cad3d
