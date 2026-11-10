#include "shunya/cad3d/trajectory_frame.hpp"

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad3d {

namespace {

std::vector<Point3D> deduplicate_path(const std::vector<Point3D>& pts) {
    std::vector<Point3D> clean;
    clean.reserve(pts.size());
    for (const auto& p : pts) {
        if (clean.empty() || (p - clean.back()).length_squared() > 1e-18) {
            clean.push_back(p);
        }
    }
    return clean;
}

Vec3d pick_orthogonal_unit(const Vec3d& tangent, const Vec3d& hint) {
    Vec3d u = hint - tangent * tangent.dot(hint);
    if (u.length_squared() > 1e-12) {
        return u.normalized();
    }
    const Vec3d alt = (std::abs(tangent.x) < 0.8) ? Vec3d{1.0, 0.0, 0.0} : Vec3d{0.0, 1.0, 0.0};
    return (alt - tangent * tangent.dot(alt)).normalized();
}

} // namespace

std::vector<Point3D> TrajectoryCurve3D::sample_cubic_bezier(
    const Point3D& p0,
    const Point3D& p1,
    const Point3D& p2,
    const Point3D& p3,
    size_t samples
) {
    const size_t n = std::max<size_t>(2, samples);
    std::vector<Point3D> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n - 1);
        const double u = 1.0 - t;
        const double b0 = u * u * u;
        const double b1 = 3.0 * u * u * t;
        const double b2 = 3.0 * u * t * t;
        const double b3 = t * t * t;
        out.push_back(p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3);
    }
    return out;
}

std::vector<Point3D> TrajectoryCurve3D::sample_helix(
    double radius,
    double pitch,
    double turns,
    size_t samples
) {
    const size_t n = std::max<size_t>(8, samples);
    std::vector<Point3D> out;
    out.reserve(n);
    const double total_angle = 2.0 * core::base::math::PI * turns;
    for (size_t i = 0; i < n; ++i) {
        const double frac = static_cast<double>(i) / static_cast<double>(n - 1);
        const double theta = total_angle * frac;
        out.push_back({
            radius * std::cos(theta),
            radius * std::sin(theta),
            pitch * turns * frac
        });
    }
    return out;
}

std::vector<Point3D> TrajectoryCurve3D::resample_polyline(
    const std::vector<Point3D>& points,
    size_t target_count
) {
    const auto clean = deduplicate_path(points);
    if (clean.size() < 2 || target_count < 2) return clean;

    std::vector<double> arc_len(clean.size(), 0.0);
    for (size_t i = 1; i < clean.size(); ++i) {
        arc_len[i] = arc_len[i - 1] + (clean[i] - clean[i - 1]).length();
    }
    const double total = arc_len.back();
    if (total <= 1e-12) return clean;

    std::vector<Point3D> out;
    out.reserve(target_count);
    size_t seg = 0;
    for (size_t k = 0; k < target_count; ++k) {
        const double target_s = total * (static_cast<double>(k) / static_cast<double>(target_count - 1));
        while (seg + 1 < clean.size() && arc_len[seg + 1] < target_s) {
            ++seg;
        }
        const size_t next = std::min(seg + 1, clean.size() - 1);
        const double seg_len = arc_len[next] - arc_len[seg];
        const double alpha = (seg_len > 1e-12) ? ((target_s - arc_len[seg]) / seg_len) : 0.0;
        out.push_back(clean[seg] + (clean[next] - clean[seg]) * alpha);
    }
    return out;
}

std::vector<TrajectoryFrame> RotationMinimizingFrame::compute_double_reflection(
    const std::vector<Point3D>& path_points,
    bool closed_loop,
    const Vec3d& initial_up_hint
) {
    const auto pts = deduplicate_path(path_points);
    const size_t n = pts.size();
    std::vector<TrajectoryFrame> frames;
    if (n < 2) return frames;

    // Cumulative normalized arc fraction
    std::vector<double> cum(n, 0.0);
    for (size_t i = 1; i < n; ++i) {
        cum[i] = cum[i - 1] + (pts[i] - pts[i - 1]).length();
    }
    const double total_len = std::max(1e-12, cum.back());

    frames.resize(n);
    for (size_t i = 0; i < n; ++i) {
        frames[i].position = pts[i];
        frames[i].arc_fraction = cum[i] / total_len;
        Vec3d t_vec;
        if (closed_loop) {
            const size_t prev = (i + n - 1) % n;
            const size_t next = (i + 1) % n;
            t_vec = pts[next] - pts[prev];
        } else if (i == 0) {
            t_vec = pts[1] - pts[0];
        } else if (i + 1 == n) {
            t_vec = pts[n - 1] - pts[n - 2];
        } else {
            t_vec = pts[i + 1] - pts[i - 1];
        }
        frames[i].tangent = t_vec.normalized();
    }

    frames[0].normal = pick_orthogonal_unit(frames[0].tangent, initial_up_hint);
    frames[0].binormal = frames[0].tangent.cross(frames[0].normal).normalized();

    // Wang et al. (2008) double-reflection step for twist-free propagation
    for (size_t i = 0; i + 1 < n; ++i) {
        const Vec3d v1 = pts[i + 1] - pts[i];
        const double c1 = v1.dot(v1);
        Vec3d rL = frames[i].normal;
        Vec3d tL = frames[i].tangent;
        if (c1 > 1e-18) {
            rL = frames[i].normal - v1 * ((2.0 / c1) * v1.dot(frames[i].normal));
            tL = frames[i].tangent - v1 * ((2.0 / c1) * v1.dot(frames[i].tangent));
        }
        const Vec3d v2 = frames[i + 1].tangent - tL;
        const double c2 = v2.dot(v2);
        Vec3d next_u = rL;
        if (c2 > 1e-18) {
            next_u = rL - v2 * ((2.0 / c2) * v2.dot(rL));
        }
        // Re-orthogonalize cleanly against T_{i+1}
        next_u = next_u - frames[i + 1].tangent * frames[i + 1].tangent.dot(next_u);
        if (next_u.length_squared() <= 1e-16) {
            next_u = pick_orthogonal_unit(frames[i + 1].tangent, initial_up_hint);
        } else {
            next_u = next_u.normalized();
        }
        frames[i + 1].normal = next_u;
        frames[i + 1].binormal = frames[i + 1].tangent.cross(next_u).normalized();
    }

    return frames;
}

} // namespace shunya::cad3d
