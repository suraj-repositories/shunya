#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/data_exchange.hpp"
#include "shunya/core/domain/mechanics.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace shunya::phy {

struct MeshInertiaProperties {
    double volume_m3{0.0};
    double mass_kg{0.0};
    core::domain::mechanics::Vector3d center_of_mass{0.0, 0.0, 0.0};
    core::domain::mechanics::Matrix3x3 inertia_body_com{core::domain::mechanics::Matrix3x3::identity()};
};

/**
 * @brief Computes exact mass, center of mass, and 3x3 rotational inertia tensor of closed triangular boundary meshes.
 */
class InertiaTensorCalculator {
public:
    /**
     * @brief Integrates exact signed tetrahedron moments over closed oriented TriangleMesh (CCW outward normals).
     */
    [[nodiscard]] static core::base::Result<MeshInertiaProperties> compute_from_closed_mesh(
        const core::domain::TriangleMesh& mesh,
        double density_kg_m3 = 1000.0) {
        using namespace core::domain::mechanics;

        if (mesh.faces.empty() || mesh.vertices.empty() || density_kg_m3 <= 0.0) {
            return core::base::Result<MeshInertiaProperties>::err(
                core::base::Error{40, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "Closed mesh inertia requires non-empty mesh and positive density", ""});
        }

        double vol6_sum = 0.0;
        Vector3d com_weighted{0.0, 0.0, 0.0};
        // Second moments P_{jk} = int_{V} x_j * x_k dV summed in array order xx, yy, zz, xy, xz, yz
        std::array<double, 6> p_moments{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

        for (const auto& f : mesh.faces) {
            if (f.v0 >= mesh.vertices.size() || f.v1 >= mesh.vertices.size() || f.v2 >= mesh.vertices.size()) {
                return core::base::Result<MeshInertiaProperties>::err(
                    core::base::Error{41, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                      "Mesh face index out of range", ""});
            }
            const auto& a = mesh.vertices[f.v0];
            const auto& b = mesh.vertices[f.v1];
            const auto& c = mesh.vertices[f.v2];

            const double vol6 = a.dot(b.cross(c));
            vol6_sum += vol6;

            // int_{tet} x dV = (vol6 / 6) * ((a + b + c) / 4) = vol6 * (a + b + c) / 24
            com_weighted += (a + b + c) * (vol6 / 24.0);

            auto second_moment = [&](double aj, double bj, double cj,
                                     double ak, double bk, double ck) -> double {
                const double sym = aj * ak + bj * bk + cj * ck;
                const double sum_jk = (aj + bj + cj) * (ak + bk + ck);
                return (vol6 / 120.0) * (sym + sum_jk);
            };

            p_moments[0] += second_moment(a.x, b.x, c.x, a.x, b.x, c.x); // xx
            p_moments[1] += second_moment(a.y, b.y, c.y, a.y, b.y, c.y); // yy
            p_moments[2] += second_moment(a.z, b.z, c.z, a.z, b.z, c.z); // zz
            p_moments[3] += second_moment(a.x, b.x, c.x, a.y, b.y, c.y); // xy
            p_moments[4] += second_moment(a.x, b.x, c.x, a.z, b.z, c.z); // xz
            p_moments[5] += second_moment(a.y, b.y, c.y, a.z, b.z, c.z); // yz
        }

        const double volume = vol6_sum / 6.0;
        if (std::abs(volume) < 1e-18) {
            return core::base::Result<MeshInertiaProperties>::err(
                core::base::Error{42, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "Degenerate closed mesh has zero signed volume", ""});
        }

        const double sign = (volume < 0.0) ? -1.0 : 1.0;
        const double abs_vol = std::abs(volume);
        const double mass = abs_vol * density_kg_m3;
        const Vector3d com = com_weighted / volume; // sign cancels cleanly

        const double p_xx = p_moments[0] * sign * density_kg_m3;
        const double p_yy = p_moments[1] * sign * density_kg_m3;
        const double p_zz = p_moments[2] * sign * density_kg_m3;
        const double p_xy = p_moments[3] * sign * density_kg_m3;
        const double p_xz = p_moments[4] * sign * density_kg_m3;
        const double p_yz = p_moments[5] * sign * density_kg_m3;

        // Inertia about world origin
        const double ixx_0 = p_yy + p_zz;
        const double iyy_0 = p_xx + p_zz;
        const double izz_0 = p_xx + p_yy;
        const double ixy_0 = -p_xy;
        const double ixz_0 = -p_xz;
        const double iyz_0 = -p_yz;

        // Parallel axis shift to CoM
        const double c2 = com.dot(com);
        const double ixx = ixx_0 - mass * (c2 - com.x * com.x);
        const double iyy = iyy_0 - mass * (c2 - com.y * com.y);
        const double izz = izz_0 - mass * (c2 - com.z * com.z);
        const double ixy = ixy_0 + mass * (com.x * com.y);
        const double ixz = ixz_0 + mass * (com.x * com.z);
        const double iyz = iyz_0 + mass * (com.y * com.z);

        MeshInertiaProperties props;
        props.volume_m3 = abs_vol;
        props.mass_kg = mass;
        props.center_of_mass = com;
        props.inertia_body_com = Matrix3x3(
            ixx, ixy, ixz,
            ixy, iyy, iyz,
            ixz, iyz, izz);
        return core::base::Result<MeshInertiaProperties>::ok(props);
    }
};

} // namespace shunya::phy
