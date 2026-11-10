#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace shunya::cad3d {

/**
 * @brief Symmetric 4x4 error quadric Q for Garland-Heckbert surface simplification.
 *
 * Represents plane equation squared distance: (n.x, n.y, n.z, d)^T * [x, y, z, 1].
 */
struct ErrorQuadric {
    // Packed symmetric matrix entries:
    // [ a00 a01 a02 a03 ]
    // [ a01 a11 a12 a13 ]
    // [ a02 a12 a22 a23 ]
    // [ a03 a13 a23 a33 ]
    double a00{0}, a01{0}, a02{0}, a03{0};
    double a11{0}, a12{0}, a13{0};
    double a22{0}, a23{0};
    double a33{0};

    [[nodiscard]] static ErrorQuadric from_plane(const Vec3d& n, double d, double weight = 1.0) noexcept {
        return {
            n.x * n.x * weight, n.x * n.y * weight, n.x * n.z * weight, n.x * d * weight,
            n.y * n.y * weight, n.y * n.z * weight, n.y * d * weight,
            n.z * n.z * weight, n.z * d * weight,
            d * d * weight
        };
    }

    [[nodiscard]] static ErrorQuadric from_triangle(const Point3D& p0, const Point3D& p1, const Point3D& p2) noexcept {
        const Vec3d cross_v = (p1 - p0).cross(p2 - p0);
        const double area2 = cross_v.length();
        if (area2 <= 1e-18) return {};
        const Vec3d n = cross_v * (1.0 / area2);
        const double d = -n.dot(p0);
        return from_plane(n, d, area2 * 0.5);
    }

    ErrorQuadric& operator+=(const ErrorQuadric& r) noexcept {
        a00 += r.a00; a01 += r.a01; a02 += r.a02; a03 += r.a03;
        a11 += r.a11; a12 += r.a12; a13 += r.a13;
        a22 += r.a22; a23 += r.a23;
        a33 += r.a33;
        return *this;
    }

    [[nodiscard]] ErrorQuadric operator+(const ErrorQuadric& r) const noexcept {
        ErrorQuadric res = *this;
        res += r;
        return res;
    }

    [[nodiscard]] double evaluate(const Point3D& p) const noexcept {
        const double val =
            a00 * p.x * p.x + 2.0 * a01 * p.x * p.y + 2.0 * a02 * p.x * p.z + 2.0 * a03 * p.x +
            a11 * p.y * p.y + 2.0 * a12 * p.y * p.z + 2.0 * a13 * p.y +
            a22 * p.z * p.z + 2.0 * a23 * p.z +
            a33;
        return std::max(0.0, val);
    }

    /**
     * @brief Solves for optimal collapse target point minimizing v^T Q v, or picks min among v0, v1, midpoint.
     */
    [[nodiscard]] Point3D optimal_target(const Point3D& v0, const Point3D& v1) const noexcept {
        // Upper 3x3 cofactor determinant of A:
        const double c00 = a11 * a22 - a12 * a12;
        const double c01 = a02 * a12 - a01 * a22;
        const double c02 = a01 * a12 - a02 * a11;
        const double det = a00 * c00 + a01 * c01 + a02 * c02;

        if (std::abs(det) > 1e-10) {
            const double c11 = a00 * a22 - a02 * a02;
            const double c12 = a01 * a02 - a00 * a12;
            const double c22 = a00 * a11 - a01 * a01;
            const double inv = -1.0 / det;
            const Point3D candidate{
                (c00 * a03 + c01 * a13 + c02 * a23) * inv,
                (c01 * a03 + c11 * a13 + c12 * a23) * inv,
                (c02 * a03 + c12 * a13 + c22 * a23) * inv
            };
            // Sanity radius guard so nearly-singular Q doesn't shoot target far away
            const double edge_len = (v1 - v0).length();
            const Point3D mid = (v0 + v1) * 0.5;
            if ((candidate - mid).length() <= 2.0 * edge_len + 1e-6) {
                return candidate;
            }
        }

        const Point3D mid = (v0 + v1) * 0.5;
        const double e0 = evaluate(v0);
        const double e1 = evaluate(v1);
        const double em = evaluate(mid);
        if (em <= e0 && em <= e1) return mid;
        return (e0 <= e1) ? v0 : v1;
    }
};

} // namespace shunya::cad3d
