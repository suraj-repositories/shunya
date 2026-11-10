#include "shunya/cad3d/ibl_processor.hpp"

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad3d {

namespace {

double radical_inverse_vdc(uint32_t bits) noexcept {
    bits = (bits << 16U) | (bits >> 16U);
    bits = ((bits & 0x55555555U) << 1U) | ((bits & 0xAAAAAAAAU) >> 1U);
    bits = ((bits & 0x33333333U) << 2U) | ((bits & 0xCCCCCCCCU) >> 2U);
    bits = ((bits & 0x0F0F0F0FU) << 4U) | ((bits & 0xF0F0F0F0U) >> 4U);
    bits = ((bits & 0x00FF00FFU) << 8U) | ((bits & 0xFF00FF00U) >> 8U);
    return static_cast<double>(bits) * 2.3283064365386963e-10; // / 2^32
}

std::array<double, 2> hammersley(uint32_t i, uint32_t n) noexcept {
    return {static_cast<double>(i) / static_cast<double>(n), radical_inverse_vdc(i)};
}

Vec3d importance_sample_ggx(const std::array<double, 2>& xi, const Vec3d& N, double roughness) noexcept {
    const double r = std::clamp(roughness, 0.04, 1.0);
    const double a = r * r;
    const double phi = 2.0 * core::base::math::PI * xi[0];
    const double cos_theta = std::sqrt(std::max(0.0, (1.0 - xi[1]) / (1.0 + (a * a - 1.0) * xi[1])));
    const double sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));

    const Vec3d H_tangent{std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta};
    const Vec3d up = (std::abs(N.z) < 0.999) ? Vec3d{0.0, 0.0, 1.0} : Vec3d{1.0, 0.0, 0.0};
    const Vec3d tangent = up.cross(N).normalized();
    const Vec3d bitangent = N.cross(tangent);
    return (tangent * H_tangent.x + bitangent * H_tangent.y + N * H_tangent.z).normalized();
}

double geometry_schlick_ibl(double n_dot_v, double roughness) noexcept {
    const double r = std::clamp(roughness, 0.04, 1.0);
    const double k = (r * r) * 0.5;
    return n_dot_v / (n_dot_v * (1.0 - k) + k + 1e-12);
}

double geometry_smith_ibl(double n_dot_v, double n_dot_l, double roughness) noexcept {
    return geometry_schlick_ibl(std::max(n_dot_v, 0.0), roughness) *
           geometry_schlick_ibl(std::max(n_dot_l, 0.0), roughness);
}

} // namespace

std::array<double, 2> BrdfLut2D::sample(double n_dot_v, double roughness) const noexcept {
    if (rg_scale_bias.empty() || width == 0 || height == 0) return {1.0, 0.0};
    const double fx = std::clamp(n_dot_v, 0.0, 1.0) * static_cast<double>(width - 1);
    const double fy = std::clamp(roughness, 0.0, 1.0) * static_cast<double>(height - 1);
    const uint32_t x0 = static_cast<uint32_t>(std::floor(fx));
    const uint32_t y0 = static_cast<uint32_t>(std::floor(fy));
    const uint32_t x1 = std::min(width - 1, x0 + 1);
    const uint32_t y1 = std::min(height - 1, y0 + 1);
    const double tx = fx - x0;
    const double ty = fy - y0;

    const auto& p00 = rg_scale_bias[static_cast<size_t>(y0) * width + x0];
    const auto& p10 = rg_scale_bias[static_cast<size_t>(y0) * width + x1];
    const auto& p01 = rg_scale_bias[static_cast<size_t>(y1) * width + x0];
    const auto& p11 = rg_scale_bias[static_cast<size_t>(y1) * width + x1];

    return {
        (1 - ty) * ((1 - tx) * p00[0] + tx * p10[0]) + ty * ((1 - tx) * p01[0] + tx * p11[0]),
        (1 - ty) * ((1 - tx) * p00[1] + tx * p10[1]) + ty * ((1 - tx) * p01[1] + tx * p11[1])
    };
}

EnvironmentMap EnvironmentMap::studio_hdr() {
    EnvironmentMap env;
    env.radiance_fn = [](const Vec3d& d) -> Vec3d {
        const Vec3d dir = d.normalized();
        const double sky_t = std::clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
        const Vec3d ground{0.18, 0.16, 0.15};
        const Vec3d sky{0.55, 0.72, 0.95};
        Vec3d col = ground * (1.0 - sky_t) + sky * sky_t;
        const Vec3d key_dir = Vec3d{0.4, 0.85, 0.35}.normalized();
        const double spot = std::pow(std::max(0.0, dir.dot(key_dir)), 16.0);
        col += Vec3d{2.5, 2.3, 2.0} * spot;
        return col;
    };
    return env;
}

EnvironmentMap EnvironmentMap::uniform_white(double intensity) {
    EnvironmentMap env;
    env.radiance_fn = [intensity](const Vec3d&) -> Vec3d {
        return {intensity, intensity, intensity};
    };
    return env;
}

BrdfLut2D IblProcessor::integrate_brdf_lut(uint32_t width, uint32_t height, uint32_t sample_count) {
    BrdfLut2D lut;
    lut.width = std::max<uint32_t>(2, width);
    lut.height = std::max<uint32_t>(2, height);
    lut.rg_scale_bias.resize(static_cast<size_t>(lut.width) * lut.height);

    const Vec3d N{0.0, 0.0, 1.0};
    for (uint32_t y = 0; y < lut.height; ++y) {
        const double roughness = std::max(0.04, static_cast<double>(y) / static_cast<double>(lut.height - 1));
        for (uint32_t x = 0; x < lut.width; ++x) {
            const double n_dot_v = std::max(0.02, static_cast<double>(x) / static_cast<double>(lut.width - 1));
            const Vec3d V{std::sqrt(std::max(0.0, 1.0 - n_dot_v * n_dot_v)), 0.0, n_dot_v};

            double A = 0.0;
            double B = 0.0;
            for (uint32_t i = 0; i < sample_count; ++i) {
                const auto xi = hammersley(i, sample_count);
                const Vec3d H = importance_sample_ggx(xi, N, roughness);
                const Vec3d L = (H * (2.0 * V.dot(H)) - V).normalized();

                const double n_dot_l = std::max(L.z, 0.0);
                const double n_dot_h = std::max(H.z, 0.0);
                const double v_dot_h = std::max(V.dot(H), 0.0);
                if (n_dot_l > 0.0) {
                    const double G = geometry_smith_ibl(n_dot_v, n_dot_l, roughness);
                    const double G_vis = (G * v_dot_h) / (n_dot_h * n_dot_v + 1e-12);
                    const double fc = std::pow(1.0 - v_dot_h, 5.0);
                    A += (1.0 - fc) * G_vis;
                    B += fc * G_vis;
                }
            }
            const double inv_n = 1.0 / static_cast<double>(sample_count);
            lut.rg_scale_bias[static_cast<size_t>(y) * lut.width + x] = {A * inv_n, B * inv_n};
        }
    }
    return lut;
}

Vec3d IblProcessor::convolve_diffuse_irradiance(
    const EnvironmentMap& env,
    const Vec3d& normal,
    uint32_t sample_count
) {
    const Vec3d N = normal.normalized();
    const Vec3d up = (std::abs(N.z) < 0.999) ? Vec3d{0.0, 0.0, 1.0} : Vec3d{1.0, 0.0, 0.0};
    const Vec3d tangent = up.cross(N).normalized();
    const Vec3d bitangent = N.cross(tangent);

    Vec3d irr{0.0, 0.0, 0.0};
    const uint32_t n_phi = std::max<uint32_t>(8, static_cast<uint32_t>(std::sqrt(sample_count) * 1.5));
    const uint32_t n_theta = std::max<uint32_t>(4, sample_count / n_phi);
    const double d_phi = (2.0 * core::base::math::PI) / static_cast<double>(n_phi);
    const double d_theta = (0.5 * core::base::math::PI) / static_cast<double>(n_theta);

    for (uint32_t it = 0; it < n_theta; ++it) {
        const double theta = (static_cast<double>(it) + 0.5) * d_theta;
        const double st = std::sin(theta);
        const double ct = std::cos(theta);
        for (uint32_t ip = 0; ip < n_phi; ++ip) {
            const double phi = (static_cast<double>(ip) + 0.5) * d_phi;
            const Vec3d t_dir{st * std::cos(phi), st * std::sin(phi), ct};
            const Vec3d world_dir = (tangent * t_dir.x + bitangent * t_dir.y + N * t_dir.z).normalized();
            irr += env.sample(world_dir) * (ct * st * d_theta * d_phi);
        }
    }
    return irr * (1.0 / core::base::math::PI);
}

Vec3d IblProcessor::prefilter_specular_env(
    const EnvironmentMap& env,
    const Vec3d& reflection_dir,
    double roughness,
    uint32_t sample_count
) {
    const Vec3d N = reflection_dir.normalized();
    const Vec3d V = N;
    Vec3d prefiltered{0.0, 0.0, 0.0};
    double total_weight = 0.0;

    for (uint32_t i = 0; i < sample_count; ++i) {
        const auto xi = hammersley(i, sample_count);
        const Vec3d H = importance_sample_ggx(xi, N, roughness);
        const Vec3d L = (H * (2.0 * V.dot(H)) - V).normalized();
        const double n_dot_l = std::max(N.dot(L), 0.0);
        if (n_dot_l > 0.0) {
            prefiltered += env.sample(L) * n_dot_l;
            total_weight += n_dot_l;
        }
    }
    return (total_weight > 1e-12) ? (prefiltered * (1.0 / total_weight)) : env.sample(N);
}

Vec3d IblProcessor::evaluate_split_sum(
    const PbrMaterialParams& mat,
    const Vec3d& N,
    const Vec3d& V,
    const EnvironmentMap& env,
    const BrdfLut2D& lut,
    uint32_t env_samples
) {
    const Vec3d n_unit = N.normalized();
    const Vec3d v_unit = V.normalized();
    const double n_dot_v = std::clamp(n_unit.dot(v_unit), 0.02, 1.0);
    const Vec3d R = (n_unit * (2.0 * n_dot_v) - v_unit).normalized();

    const Vec3d f0 = PbrBrdf::compute_f0(mat);
    const Vec3d F = PbrBrdf::fresnel_schlick(n_dot_v, f0);

    const double m = std::clamp(mat.metallic, 0.0, 1.0);
    const Vec3d kd{(1.0 - F.x) * (1.0 - m), (1.0 - F.y) * (1.0 - m), (1.0 - F.z) * (1.0 - m)};

    const Vec3d irr = convolve_diffuse_irradiance(env, n_unit, env_samples);
    const Vec3d diffuse{kd.x * irr.x * mat.albedo.x, kd.y * irr.y * mat.albedo.y, kd.z * irr.z * mat.albedo.z};

    const Vec3d prefiltered = prefilter_specular_env(env, R, mat.roughness, env_samples);
    const auto brdf_sb = lut.sample(n_dot_v, mat.roughness);
    const Vec3d spec{
        prefiltered.x * (F.x * brdf_sb[0] + brdf_sb[1]),
        prefiltered.y * (F.y * brdf_sb[0] + brdf_sb[1]),
        prefiltered.z * (F.z * brdf_sb[0] + brdf_sb[1])
    };
    return (diffuse + spec) * mat.ao + mat.emissive;
}

Vec3d IblProcessor::integrate_monte_carlo_ground_truth(
    const PbrMaterialParams& mat,
    const Vec3d& N,
    const Vec3d& V,
    const EnvironmentMap& env,
    uint32_t sample_count
) {
    const Vec3d n_unit = N.normalized();
    const Vec3d v_unit = V.normalized();
    const double n_dot_v = std::clamp(n_unit.dot(v_unit), 0.02, 1.0);
    const Vec3d f0 = PbrBrdf::compute_f0(mat);

    // Specular importance sampling ground truth + diffuse cosine-weighted hemisphere ground truth
    Vec3d spec_accum{0.0, 0.0, 0.0};
    for (uint32_t i = 0; i < sample_count; ++i) {
        const auto xi = hammersley(i, sample_count);
        const Vec3d H = importance_sample_ggx(xi, n_unit, mat.roughness);
        const Vec3d L = (H * (2.0 * v_unit.dot(H)) - v_unit).normalized();
        const double n_dot_l = std::max(n_unit.dot(L), 0.0);
        const double n_dot_h = std::max(n_unit.dot(H), 0.0);
        const double v_dot_h = std::max(v_unit.dot(H), 0.0);
        if (n_dot_l > 0.0) {
            const Vec3d Li = env.sample(L);
            const Vec3d F = PbrBrdf::fresnel_schlick(v_dot_h, f0);
            const double G = geometry_smith_ibl(n_dot_v, n_dot_l, mat.roughness);
            const double weight = (G * v_dot_h) / (n_dot_h * n_dot_v + 1e-12);
            spec_accum += Vec3d{Li.x * F.x * weight, Li.y * F.y * weight, Li.z * F.z * weight};
        }
    }
    spec_accum = spec_accum * (1.0 / static_cast<double>(sample_count));

    const Vec3d F_view = PbrBrdf::fresnel_schlick(n_dot_v, f0);
    const double m = std::clamp(mat.metallic, 0.0, 1.0);
    const Vec3d kd{(1.0 - F_view.x) * (1.0 - m), (1.0 - F_view.y) * (1.0 - m), (1.0 - F_view.z) * (1.0 - m)};
    const Vec3d irr = convolve_diffuse_irradiance(env, n_unit, sample_count);
    const Vec3d diff{kd.x * irr.x * mat.albedo.x, kd.y * irr.y * mat.albedo.y, kd.z * irr.z * mat.albedo.z};
    return (diff + spec_accum) * mat.ao + mat.emissive;
}

} // namespace shunya::cad3d
