#include "shunya/cad3d/loft_feature.hpp"

#include "shunya/cad3d/polygon_tessellator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace shunya::cad3d {

namespace {

cad2d::Path2D resample_closed_2d(const cad2d::Path2D& in, size_t n_out) {
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
    if (clean.size() < 3 || n_out < 3) return {};

    // Ensure CCW in local plane before loft alignment
    if (cad2d::PolygonBooleanEngine::signed_area(clean) < 0.0) {
        std::reverse(clean.begin(), clean.end());
    }

    const size_t n_in = clean.size();
    std::vector<double> cum(n_in + 1, 0.0);
    for (size_t i = 0; i < n_in; ++i) {
        cum[i + 1] = cum[i] + (clean[(i + 1) % n_in] - clean[i]).length();
    }
    const double total = cum.back();
    if (total <= 1e-12) return {};

    cad2d::Path2D out;
    out.reserve(n_out);
    size_t seg = 0;
    for (size_t k = 0; k < n_out; ++k) {
        const double target_s = total * (static_cast<double>(k) / static_cast<double>(n_out));
        while (seg + 1 < n_in && cum[seg + 1] < target_s) {
            ++seg;
        }
        const size_t next = (seg + 1) % n_in;
        const double seg_len = cum[seg + 1] - cum[seg];
        const double alpha = (seg_len > 1e-12) ? ((target_s - cum[seg]) / seg_len) : 0.0;
        out.push_back(clean[seg] + (clean[next] - clean[seg]) * alpha);
    }
    return out;
}

std::vector<Point3D> align_ring_minimal_twist(
    const std::vector<Point3D>& prev_ring,
    std::vector<Point3D> cur_ring,
    const Vec3d& sweep_dir
) {
    const size_t n = cur_ring.size();
    if (n < 3 || prev_ring.size() != n) return cur_ring;

    // Ensure cur_ring loop orientation agrees with prev_ring around sweep_dir
    auto compute_loop_axis = [&](const std::vector<Point3D>& ring) {
        Point3D c{0.0, 0.0, 0.0};
        for (const auto& p : ring) c += p;
        c = c * (1.0 / static_cast<double>(ring.size()));
        Vec3d sum{0.0, 0.0, 0.0};
        for (size_t i = 0; i < ring.size(); ++i) {
            const Vec3d r0 = ring[i] - c;
            const Vec3d r1 = ring[(i + 1) % ring.size()] - c;
            sum += r0.cross(r1);
        }
        return sum;
    };

    if (compute_loop_axis(prev_ring).dot(compute_loop_axis(cur_ring)) < 0.0) {
        std::reverse(cur_ring.begin(), cur_ring.end());
    }
    (void)sweep_dir;

    size_t best_shift = 0;
    double best_cost = std::numeric_limits<double>::infinity();
    for (size_t shift = 0; shift < n; ++shift) {
        double cost = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const Vec3d d = prev_ring[i] - cur_ring[(i + shift) % n];
            cost += d.length_squared();
        }
        if (cost < best_cost) {
            best_cost = cost;
            best_shift = shift;
        }
    }

    std::rotate(cur_ring.begin(), cur_ring.begin() + static_cast<std::ptrdiff_t>(best_shift), cur_ring.end());
    return cur_ring;
}

} // namespace

SolidMesh3D LoftEngine::loft(
    const std::vector<LoftSection>& sections,
    const LoftParams& params
) {
    SolidMesh3D mesh;
    if (sections.size() < 2) return mesh;

    const size_t n_pts = std::max<size_t>(6, params.samples_per_section);
    std::vector<cad2d::Path2D> resampled_2d;
    std::vector<std::vector<Point3D>> rings_3d;
    resampled_2d.reserve(sections.size());
    rings_3d.reserve(sections.size());

    for (const auto& sec : sections) {
        cad2d::Path2D p2 = resample_closed_2d(sec.profile_2d, n_pts);
        if (p2.size() != n_pts) return mesh;
        std::vector<Point3D> r3;
        r3.reserve(n_pts);
        for (const auto& pt : p2) {
            r3.push_back(sec.plane.to_3d(pt, 0.0));
        }
        if (!rings_3d.empty()) {
            Point3D c0{0, 0, 0}, c1{0, 0, 0};
            for (const auto& v : rings_3d.back()) c0 += v;
            for (const auto& v : r3) c1 += v;
            r3 = align_ring_minimal_twist(rings_3d.back(), std::move(r3), c1 - c0);
        }
        resampled_2d.push_back(std::move(p2));
        rings_3d.push_back(std::move(r3));
    }

    // Align winding of ring 0 with progression direction so side quads match start/end caps cleanly
    Point3D c0{0, 0, 0}, c1{0, 0, 0};
    for (const auto& v : rings_3d[0]) c0 += v;
    for (const auto& v : rings_3d[1]) c1 += v;
    const Vec3d prog_dir = (c1 - c0).normalized();
    const bool dir_matches_plane0 = sections[0].plane.normal().dot(prog_dir) >= 0.0;

    mesh.vertices.reserve(rings_3d.size() * n_pts);
    for (const auto& ring : rings_3d) {
        for (const auto& pt : ring) {
            mesh.add_vertex(pt);
        }
    }

    const size_t n_sec = rings_3d.size();
    const size_t seg_steps = params.closed_loft ? n_sec : (n_sec - 1);
    for (size_t s = 0; s < seg_steps; ++s) {
        const uint32_t base0 = static_cast<uint32_t>(s * n_pts);
        const uint32_t base1 = static_cast<uint32_t>(((s + 1) % n_sec) * n_pts);
        for (size_t i = 0; i < n_pts; ++i) {
            const uint32_t a0 = base0 + static_cast<uint32_t>(i);
            const uint32_t b0 = base0 + static_cast<uint32_t>((i + 1) % n_pts);
            const uint32_t a1 = base1 + static_cast<uint32_t>(i);
            const uint32_t b1 = base1 + static_cast<uint32_t>((i + 1) % n_pts);
            if (dir_matches_plane0) {
                mesh.add_triangle(a0, b0, b1);
                mesh.add_triangle(a0, b1, a1);
            } else {
                mesh.add_triangle(a0, b1, b0);
                mesh.add_triangle(a0, a1, b1);
            }
        }
    }

    if (params.cap_ends && !params.closed_loft) {
        // Project first and last rings onto their respective local planes so rotated/shifted vertex order matches exact ring indices
        auto cap_section = [&](size_t sec_idx, bool is_start) {
            const auto& plane = sections[sec_idx].plane;
            const auto& ring = rings_3d[sec_idx];
            cad2d::Path2D local_2d;
            local_2d.reserve(ring.size());
            for (const auto& v : ring) {
                const Vec3d rel = v - plane.origin;
                local_2d.push_back({rel.dot(plane.axis_u), rel.dot(plane.axis_v)});
            }
            const bool local_is_ccw = cad2d::PolygonBooleanEngine::signed_area(local_2d) >= 0.0;
            const auto tess = PolygonTessellator::triangulate(local_2d);
            // Notice PolygonTessellator normalizes tess.vertices to CCW, which might reverse or shift local_2d indices!
            // Instead map tess vertices directly back to exact ring vertex indices by nearest 2D point (<1e-9).
            std::vector<uint32_t> tess_to_ring(tess.vertices.size(), 0);
            const uint32_t base = static_cast<uint32_t>(sec_idx * n_pts);
            for (size_t ti = 0; ti < tess.vertices.size(); ++ti) {
                size_t best_k = 0;
                double best_d2 = std::numeric_limits<double>::infinity();
                for (size_t k = 0; k < local_2d.size(); ++k) {
                    const double d2 = (tess.vertices[ti] - local_2d[k]).length_squared();
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        best_k = k;
                    }
                }
                tess_to_ring[ti] = base + static_cast<uint32_t>(best_k);
            }
            (void)local_is_ccw;
            const bool plane_matches_prog = plane.normal().dot(prog_dir) >= 0.0;
            const bool want_ccw_in_plane = is_start ? !plane_matches_prog : plane_matches_prog;
            for (const auto& tri : tess.triangles) {
                const uint32_t i0 = tess_to_ring[tri[0]];
                const uint32_t i1 = tess_to_ring[tri[1]];
                const uint32_t i2 = tess_to_ring[tri[2]];
                if (want_ccw_in_plane) {
                    mesh.add_triangle(i0, i1, i2);
                } else {
                    mesh.add_triangle(i0, i2, i1);
                }
            }
        };

        cap_section(0, true);
        cap_section(n_sec - 1, false);
    }

    mesh.ensure_outward_winding();
    return mesh;
}

} // namespace shunya::cad3d
