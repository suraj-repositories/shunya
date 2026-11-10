#include "shunya/cad3d/pbr_material.hpp"

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad3d {

Vec3d PbrBrdf::fresnel_schlick(double cos_theta, const Vec3d& f0) noexcept {
    const double one_minus = std::clamp(1.0 - cos_theta, 0.0, 1.0);
    const double p5 = std::pow(one_minus, 5.0);
    return {
        f0.x + (1.0 - f0.x) * p5,
        f0.y + (1.0 - f0.y) * p5,
        f0.z + (1.0 - f0.z) * p5
    };
}

double PbrBrdf::distribution_ggx(double n_dot_h, double roughness) noexcept {
    const double r = std::clamp(roughness, 0.04, 1.0);
    const double a = r * r;
    const double a2 = a * a;
    const double nh2 = n_dot_h * n_dot_h;
    const double denom = nh2 * (a2 - 1.0) + 1.0;
    return a2 / (core::base::math::PI * denom * denom + 1e-12);
}

double PbrBrdf::geometry_schlick_ggx(double n_dot_v, double roughness) noexcept {
    const double r = std::clamp(roughness, 0.04, 1.0) + 1.0;
    const double k = (r * r) / 8.0;
    return n_dot_v / (n_dot_v * (1.0 - k) + k + 1e-12);
}

double PbrBrdf::geometry_smith(double n_dot_v, double n_dot_l, double roughness) noexcept {
    return geometry_schlick_ggx(std::max(n_dot_v, 0.0), roughness) *
           geometry_schlick_ggx(std::max(n_dot_l, 0.0), roughness);
}

Vec3d PbrBrdf::compute_f0(const PbrMaterialParams& mat) noexcept {
    const double m = std::clamp(mat.metallic, 0.0, 1.0);
    const double d0 = 0.04;
    return {
        d0 * (1.0 - m) + mat.albedo.x * m,
        d0 * (1.0 - m) + mat.albedo.y * m,
        d0 * (1.0 - m) + mat.albedo.z * m
    };
}

Vec3d PbrBrdf::evaluate_radiance(
    const PbrMaterialParams& mat,
    const Vec3d& N,
    const Vec3d& V,
    const Vec3d& L,
    const Vec3d& light_radiance
) noexcept {
    const double n_dot_l = std::max(N.dot(L), 0.0);
    const double n_dot_v = std::max(N.dot(V), 0.0);
    if (n_dot_l <= 0.0 || n_dot_v <= 0.0) return {0.0, 0.0, 0.0};

    const Vec3d H = (V + L).normalized();
    const double n_dot_h = std::max(N.dot(H), 0.0);
    const double h_dot_v = std::max(H.dot(V), 0.0);

    const Vec3d f0 = compute_f0(mat);
    const Vec3d F = fresnel_schlick(h_dot_v, f0);
    const double D = distribution_ggx(n_dot_h, mat.roughness);
    const double G = geometry_smith(n_dot_v, n_dot_l, mat.roughness);

    const double spec_denom = 4.0 * n_dot_v * n_dot_l + 1e-7;
    const Vec3d spec = F * ((D * G) / spec_denom);

    const double m = std::clamp(mat.metallic, 0.0, 1.0);
    const Vec3d kd{
        (1.0 - F.x) * (1.0 - m),
        (1.0 - F.y) * (1.0 - m),
        (1.0 - F.z) * (1.0 - m)
    };
    const double inv_pi = 1.0 / core::base::math::PI;
    const Vec3d diff{
        kd.x * mat.albedo.x * inv_pi,
        kd.y * mat.albedo.y * inv_pi,
        kd.z * mat.albedo.z * inv_pi
    };

    const Vec3d brdf{diff.x + spec.x, diff.y + spec.y, diff.z + spec.z};
    return {
        brdf.x * light_radiance.x * n_dot_l,
        brdf.y * light_radiance.y * n_dot_l,
        brdf.z * light_radiance.z * n_dot_l
    };
}

double PbrBrdf::integrate_hemisphere_reflectance(
    const PbrMaterialParams& mat,
    double n_dot_v,
    int theta_steps,
    int phi_steps
) noexcept {
    const double ndv = std::clamp(n_dot_v, 0.02, 1.0);
    const Vec3d N{0.0, 0.0, 1.0};
    const Vec3d V{std::sqrt(std::max(0.0, 1.0 - ndv * ndv)), 0.0, ndv};

    const double d_theta = (core::base::math::PI * 0.5) / static_cast<double>(theta_steps);
    const double d_phi = (2.0 * core::base::math::PI) / static_cast<double>(phi_steps);

    Vec3d total{0.0, 0.0, 0.0};
    for (int it = 0; it < theta_steps; ++it) {
        const double theta = (static_cast<double>(it) + 0.5) * d_theta;
        const double st = std::sin(theta);
        const double ct = std::cos(theta);
        for (int ip = 0; ip < phi_steps; ++ip) {
            const double phi = (static_cast<double>(ip) + 0.5) * d_phi;
            const Vec3d L{st * std::cos(phi), st * std::sin(phi), ct};
            const Vec3d rad = evaluate_radiance(mat, N, V, L, {1.0, 1.0, 1.0});
            total += rad * (st * d_theta * d_phi);
        }
    }
    return std::max({total.x, total.y, total.z});
}

std::string_view PbrBrdf::vert_shader_source() noexcept {
    return R"(#version 450
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 0) out vec3 out_world_pos;
layout(location = 1) out vec3 out_normal;
void main() {
    out_world_pos = in_pos;
    out_normal = in_normal;
    gl_Position = vec4(in_pos, 1.0);
}
)";
}

std::string_view PbrBrdf::frag_shader_source() noexcept {
    return R"(#version 450
layout(location = 0) in vec3 in_world_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = vec4(normalize(in_normal) * 0.5 + 0.5, 1.0);
}
)";
}

} // namespace shunya::cad3d
