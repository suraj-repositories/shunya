#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace shunya::cad3d {

struct TetraMesh3D {
    std::vector<Point3D> vertices;
    std::vector<std::array<uint32_t, 4>> tetrahedra; // Tet4 elements oriented with positive Jacobian

    [[nodiscard]] double total_volume() const noexcept {
        double vol6 = 0.0;
        for (const auto& t : tetrahedra) {
            const Vec3d e1 = vertices[t[1]] - vertices[t[0]];
            const Vec3d e2 = vertices[t[2]] - vertices[t[0]];
            const Vec3d e3 = vertices[t[3]] - vertices[t[0]];
            vol6 += e1.dot(e2.cross(e3));
        }
        return vol6 / 6.0;
    }
};

struct TetQualityReport {
    size_t element_count{0};
    size_t inverted_count{0};
    double min_jacobian{0.0};
    double min_dihedral_deg{180.0};
    double max_aspect_ratio{0.0};
    double total_volume{0.0};
};

class TetQualityAnalyzer {
public:
    [[nodiscard]] static double signed_jacobian(
        const Point3D& a,
        const Point3D& b,
        const Point3D& c,
        const Point3D& d
    ) noexcept {
        return (b - a).dot((c - a).cross(d - a));
    }

    [[nodiscard]] static double aspect_ratio(
        const Point3D& a,
        const Point3D& b,
        const Point3D& c,
        const Point3D& d
    ) noexcept {
        const Point3D pts[4] = {a, b, c, d};
        double max_edge = 0.0;
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                max_edge = std::max(max_edge, (pts[j] - pts[i]).length());
            }
        }
        const double jac = std::abs(signed_jacobian(a, b, c, d));
        if (jac <= 1e-18) return 1e9;
        const double vol = jac / 6.0;
        const double face_areas =
            0.5 * ((b - a).cross(c - a)).length() +
            0.5 * ((b - a).cross(d - a)).length() +
            0.5 * ((c - a).cross(d - a)).length() +
            0.5 * ((c - b).cross(d - b)).length();
        if (face_areas <= 1e-18) return 1e9;
        const double inradius = 3.0 * vol / face_areas;
        // Normalized relative to regular tetrahedron: h / (2*sqrt(6)*r) == 1.0
        return max_edge / (2.0 * std::sqrt(6.0) * inradius);
    }

    [[nodiscard]] static double min_dihedral_degrees(
        const Point3D& a,
        const Point3D& b,
        const Point3D& c,
        const Point3D& d
    ) noexcept {
        const Vec3d n0 = ((c - a).cross(b - a)).normalized(); // opposite d
        const Vec3d n1 = ((b - a).cross(d - a)).normalized(); // opposite c
        const Vec3d n2 = ((d - a).cross(c - a)).normalized(); // opposite b
        const Vec3d n3 = ((c - b).cross(d - b)).normalized(); // opposite a
        const Vec3d normals[4] = {n0, n1, n2, n3};
        const double jsign = (signed_jacobian(a, b, c, d) >= 0.0) ? 1.0 : -1.0;

        double min_deg = 180.0;
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                const double cos_adj = std::clamp(-(normals[i] * jsign).dot(normals[j] * jsign), -1.0, 1.0);
                const double angle_rad = std::acos(cos_adj);
                min_deg = std::min(min_deg, angle_rad * 180.0 / core::base::math::PI);
            }
        }
        return min_deg;
    }

    [[nodiscard]] static TetQualityReport evaluate(const TetraMesh3D& mesh) noexcept {
        TetQualityReport rep;
        rep.element_count = mesh.tetrahedra.size();
        if (mesh.tetrahedra.empty()) return rep;
        rep.min_jacobian = 1e30;

        for (const auto& t : mesh.tetrahedra) {
            const auto& a = mesh.vertices[t[0]];
            const auto& b = mesh.vertices[t[1]];
            const auto& c = mesh.vertices[t[2]];
            const auto& d = mesh.vertices[t[3]];
            const double jac = signed_jacobian(a, b, c, d);
            if (jac <= 0.0) ++rep.inverted_count;
            rep.min_jacobian = std::min(rep.min_jacobian, jac);
            rep.total_volume += jac / 6.0;
            rep.max_aspect_ratio = std::max(rep.max_aspect_ratio, aspect_ratio(a, b, c, d));
            rep.min_dihedral_deg = std::min(rep.min_dihedral_deg, min_dihedral_degrees(a, b, c, d));
        }
        return rep;
    }
};

} // namespace shunya::cad3d
