#include "shunya/cad3d/sweep_feature.hpp"

#include "shunya/cad3d/polygon_tessellator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace shunya::cad3d {

namespace {

cad2d::Path2D clean_loop_for_sweep(const cad2d::Path2D& in, bool want_ccw) {
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

} // namespace

SolidMesh3D SweepEngine::sweep(
    const cad2d::Path2D& profile_outer,
    const cad2d::Paths2D& profile_holes,
    const std::vector<Point3D>& trajectory_points,
    const SweepParams& params
) {
    SolidMesh3D mesh;
    const cad2d::Path2D clean_outer = clean_loop_for_sweep(profile_outer, true);
    if (clean_outer.size() < 3) return mesh;

    cad2d::Paths2D clean_holes;
    for (const auto& h : profile_holes) {
        auto ch = clean_loop_for_sweep(h, false);
        if (ch.size() >= 3) clean_holes.push_back(std::move(ch));
    }

    const auto frames = RotationMinimizingFrame::compute_double_reflection(
        trajectory_points,
        params.closed_trajectory,
        params.initial_up_hint
    );
    if (frames.size() < 2) return mesh;

    const TessellatedPolygon2D tess = PolygonTessellator::triangulate(clean_outer, clean_holes);
    if (tess.vertices.empty() || tess.triangles.empty()) return mesh;

    const uint32_t n_pts = static_cast<uint32_t>(tess.vertices.size());
    const size_t n_frames = frames.size();
    mesh.vertices.reserve(n_frames * n_pts);

    for (size_t f = 0; f < n_frames; ++f) {
        const auto& fr = frames[f];
        const double t = fr.arc_fraction;
        const double s = params.scale_start + (params.scale_end - params.scale_start) * t;
        const double tw = params.twist_total_rad * t;
        const double cw = std::cos(tw);
        const double sw = std::sin(tw);

        for (uint32_t i = 0; i < n_pts; ++i) {
            const double u = (tess.vertices[i].x * cw - tess.vertices[i].y * sw) * s;
            const double v = (tess.vertices[i].x * sw + tess.vertices[i].y * cw) * s;
            mesh.add_vertex(fr.position + fr.normal * u + fr.binormal * v);
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> loop_spans;
    uint32_t cursor = 0;
    for (const uint32_t count : tess.loop_sizes) {
        loop_spans.push_back({cursor, count});
        cursor += count;
    }

    const size_t seg_steps = params.closed_trajectory ? n_frames : (n_frames - 1);
    for (size_t f = 0; f < seg_steps; ++f) {
        const uint32_t base0 = static_cast<uint32_t>(f * n_pts);
        const uint32_t base1 = static_cast<uint32_t>(((f + 1) % n_frames) * n_pts);
        for (const auto& [offset, count] : loop_spans) {
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t a0 = base0 + offset + i;
                const uint32_t b0 = base0 + offset + ((i + 1) % count);
                const uint32_t a1 = base1 + offset + i;
                const uint32_t b1 = base1 + offset + ((i + 1) % count);
                mesh.add_triangle(a0, b0, b1);
                mesh.add_triangle(a0, b1, a1);
            }
        }
    }

    if (!params.closed_trajectory) {
        const uint32_t end_base = static_cast<uint32_t>((n_frames - 1) * n_pts);
        for (const auto& tri : tess.triangles) {
            mesh.add_triangle(tri[0], tri[2], tri[1]);
            mesh.add_triangle(end_base + tri[0], end_base + tri[1], end_base + tri[2]);
        }
    }

    mesh.ensure_outward_winding();
    return mesh;
}

} // namespace shunya::cad3d
