#include "shunya/cad3d/volume_renderer.hpp"

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace shunya::cad3d {

namespace {

bool intersect_ray_box_interval(
    const Ray3D& ray,
    const BBox3D& box,
    double& out_t0,
    double& out_t1
) noexcept {
    double tmin = 0.0;
    double tmax = 1e30;
    const double* o = &ray.origin.x;
    const double* d = &ray.direction.x;
    const double* bmin = &box.min.x;
    const double* bmax = &box.max.x;

    for (int ax = 0; ax < 3; ++ax) {
        if (std::abs(d[ax]) < 1e-18) {
            if (o[ax] < bmin[ax] || o[ax] > bmax[ax]) return false;
        } else {
            double inv = 1.0 / d[ax];
            double t0 = (bmin[ax] - o[ax]) * inv;
            double t1 = (bmax[ax] - o[ax]) * inv;
            if (t0 > t1) std::swap(t0, t1);
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
            if (tmax <= tmin) return false;
        }
    }
    out_t0 = tmin;
    out_t1 = tmax;
    return true;
}

} // namespace

TransferSample VolumeRaymarcher::blackbody_transfer(
    double normalized_scalar,
    double density_scale
) noexcept {
    const double s = std::clamp(normalized_scalar, 0.0, 1.0);
    const double sigma = s * density_scale;
    // Blackbody ramp: dark blue -> orange -> bright white-yellow
    Vec3d em;
    if (s < 0.5) {
        const double u = s * 2.0;
        em = {u * 0.9, u * 0.35, 0.1 + (1.0 - u) * 0.3};
    } else {
        const double u = (s - 0.5) * 2.0;
        em = {0.9 + 0.1 * u, 0.35 + 0.65 * u, 0.1 + 0.8 * u};
    }
    return {em * (s * 1.5), sigma};
}

std::array<double, 4> VolumeRaymarcher::integrate_ray(
    const Ray3D& ray,
    const VolumeGrid3D& volume,
    const VolumeRenderConfig& config,
    const std::function<TransferSample(double scalar)>& transfer_fn
) {
    double t0 = 0.0, t1 = 0.0;
    if (!intersect_ray_box_interval(ray, volume.world_bounds, t0, t1)) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    const double span = std::max(1e-9, config.max_value - config.min_value);
    const double base_ds = std::max(1e-4, config.step_size);
    double transmittance = 1.0;
    Vec3d accum_radiance{0.0, 0.0, 0.0};

    double t = t0;
    while (t < t1 && transmittance > 1e-4) {
        const double ds = std::min(base_ds, t1 - t);
        const Point3D sample_pos = ray.origin + ray.direction * (t + ds * 0.5);
        const double raw = volume.sample_world(sample_pos);
        const double norm = std::clamp((raw - config.min_value) / span, 0.0, 1.0);

        const TransferSample tf = transfer_fn
            ? transfer_fn(norm)
            : blackbody_transfer(norm, config.density_scale);

        const double sigma = std::max(0.0, tf.absorption);
        if (sigma > 1e-12) {
            const double seg_atten = std::exp(-sigma * ds);
            const double seg_alpha = 1.0 - seg_atten;
            accum_radiance += tf.emission * (transmittance * seg_alpha);
            transmittance *= seg_atten;
        }
        t += ds;
    }

    const double opacity = std::clamp(1.0 - transmittance, 0.0, 1.0);
    return {
        std::clamp(accum_radiance.x, 0.0, 1.0),
        std::clamp(accum_radiance.y, 0.0, 1.0),
        std::clamp(accum_radiance.z, 0.0, 1.0),
        opacity
    };
}

VolumeRenderResult VolumeRaymarcher::render(
    const core::domain::ViewportCamera& camera,
    const VolumeGrid3D& volume,
    const VolumeRenderConfig& config,
    const std::function<TransferSample(double scalar)>& transfer_fn
) {
    const auto t_start = std::chrono::steady_clock::now();
    VolumeRenderResult res;
    res.target.resize(config.width, config.height);
    res.opacity_buffer.assign(static_cast<size_t>(config.width) * config.height, 0.0);

    const Vec3d fwd = camera.forward();
    const Vec3d rgt = camera.right();
    const Vec3d up = camera.up_vector();
    const double tan_half_fov = std::tan((camera.fov_degrees * 0.5) * core::base::math::PI / 180.0);

    for (uint32_t py = 0; py < config.height; ++py) {
        const double ndc_y = 1.0 - (static_cast<double>(py) + 0.5) / config.height * 2.0;
        for (uint32_t px = 0; px < config.width; ++px) {
            const double ndc_x = (static_cast<double>(px) + 0.5) / config.width * 2.0 - 1.0;
            const Vec3d dir = (fwd + rgt * (ndc_x * tan_half_fov * camera.aspect_ratio) +
                               up * (ndc_y * tan_half_fov)).normalized();
            const Ray3D ray{camera.position, dir};
            const auto rgba = integrate_ray(ray, volume, config, transfer_fn);

            const size_t idx = static_cast<size_t>(py) * config.width + px;
            res.target.color_rgba[idx] = rgba;
            res.opacity_buffer[idx] = rgba[3];
        }
    }

    const auto t_end = std::chrono::steady_clock::now();
    res.render_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    return res;
}

std::string_view VolumeRaymarcher::shader_source() noexcept {
    return R"(#version 450
layout(binding = 0) uniform sampler3D u_volume_field;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = vec4(0.0);
}
)";
}

} // namespace shunya::cad3d
