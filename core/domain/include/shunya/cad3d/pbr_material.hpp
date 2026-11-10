#pragma once

#include "shunya/cad3d/material_library.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <string_view>

namespace shunya::cad3d {

/**
 * @brief Cook-Torrance microfacet PBR BRDF (GGX/Trowbridge-Reitz + Smith + Schlick Fresnel).
 */
class PbrBrdf {
public:
    [[nodiscard]] static Vec3d fresnel_schlick(double cos_theta, const Vec3d& f0) noexcept;
    [[nodiscard]] static double distribution_ggx(double n_dot_h, double roughness) noexcept;
    [[nodiscard]] static double geometry_schlick_ggx(double n_dot_v, double roughness) noexcept;
    [[nodiscard]] static double geometry_smith(double n_dot_v, double n_dot_l, double roughness) noexcept;

    [[nodiscard]] static Vec3d compute_f0(const PbrMaterialParams& mat) noexcept;

    /**
     * @brief Evaluates full Cook-Torrance (diffuse + specular) BRDF f(V, L) * (N . L).
     */
    [[nodiscard]] static Vec3d evaluate_radiance(
        const PbrMaterialParams& mat,
        const Vec3d& N,
        const Vec3d& V,
        const Vec3d& L,
        const Vec3d& light_radiance
    ) noexcept;

    /**
     * @brief Numerically integrates upper hemisphere reflectance to verify physical energy conservation (<= 1.0).
     */
    [[nodiscard]] static double integrate_hemisphere_reflectance(
        const PbrMaterialParams& mat,
        double n_dot_v,
        int theta_steps = 32,
        int phi_steps = 64
    ) noexcept;

    [[nodiscard]] static std::string_view vert_shader_source() noexcept;
    [[nodiscard]] static std::string_view frag_shader_source() noexcept;
};

} // namespace shunya::cad3d
