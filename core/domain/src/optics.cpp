#include "shunya/core/domain/optics.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::core::domain::optics {

// =============================================================================
// OpticalMaterial Implementation
// =============================================================================

double OpticalMaterial::refractive_index(double wavelength_um) const noexcept {
    if (!is_dispersive_) {
        return constant_n_;
    }

    double w2 = wavelength_um * wavelength_um;
    double n2 = 1.0 +
        (sellmeier_.b1 * w2) / (w2 - sellmeier_.c1) +
        (sellmeier_.b2 * w2) / (w2 - sellmeier_.c2) +
        (sellmeier_.b3 * w2) / (w2 - sellmeier_.c3);

    return (n2 > 0.0) ? std::sqrt(n2) : constant_n_;
}

double OpticalMaterial::absorption_coefficient(double wavelength_um) const noexcept {
    if (wavelength_um <= 0.0) return 0.0;
    double lambda_m = wavelength_um * 1e-6;
    constexpr double PI = 3.14159265358979323846;
    return (4.0 * PI * extinction_k_) / lambda_m;
}

double OpticalMaterial::abbe_number() const noexcept {
    double n_d = refractive_index(0.58756); // Helium d-line (yellow)
    double n_F = refractive_index(0.48613); // Hydrogen F-line (blue)
    double n_C = refractive_index(0.65627); // Hydrogen C-line (red)

    double delta_n = n_F - n_C;
    return (delta_n > 0.0) ? (n_d - 1.0) / delta_n : 0.0;
}

// =============================================================================
// RayTracerEngine Implementation
// =============================================================================

std::optional<Vector2D> RayTracerEngine::refract(
    const Vector2D& inc_dir,
    const Vector2D& normal,
    double n1,
    double n2) {
    if (n2 <= 0.0 || n1 <= 0.0) {
        return std::nullopt;
    }

    Vector2D d = inc_dir.normalized();
    Vector2D n = normal.normalized();

    double cos_i = -d.dot(n);
    if (cos_i < 0.0) {
        // Ray is inside the medium pointing out; invert normal
        n = Vector2D{-n.x, -n.y};
        cos_i = -d.dot(n);
    }

    double eta = n1 / n2;
    double sin2_t = eta * eta * (1.0 - cos_i * cos_i);

    if (sin2_t > 1.0) {
        // Total Internal Reflection (TIR)
        return std::nullopt;
    }

    double cos_t = std::sqrt(1.0 - sin2_t);
    Vector2D t{
        eta * d.x + (eta * cos_i - cos_t) * n.x,
        eta * d.y + (eta * cos_i - cos_t) * n.y
    };
    return t.normalized();
}

Vector2D RayTracerEngine::reflect(const Vector2D& inc_dir, const Vector2D& normal) {
    Vector2D d = inc_dir.normalized();
    Vector2D n = normal.normalized();
    double dot_dn = d.dot(n);
    return Vector2D{
        d.x - 2.0 * dot_dn * n.x,
        d.y - 2.0 * dot_dn * n.y
    }.normalized();
}

std::optional<double> RayTracerEngine::critical_angle_radians(double n1, double n2) {
    if (n1 > n2 && n1 > 0.0) {
        return std::asin(n2 / n1);
    }
    return std::nullopt;
}

RayTraceResult RayTracerEngine::trace_rays(const std::vector<OpticalRay>& incoming_rays) {
    RayTraceResult result;
    result.total_rays_traced = incoming_rays.size();

    // Sort bench elements by X position
    auto sorted_elements = elements_;
    std::sort(sorted_elements.begin(), sorted_elements.end(),
        [](const OpticalElement& a, const OpticalElement& b) {
            return a.position_x < b.position_x;
        });

    for (const auto& ray_in : incoming_rays) {
        std::vector<OpticalRay> trajectory;
        OpticalRay curr = ray_in;
        trajectory.push_back(curr);

        for (auto& elem : sorted_elements) {
            if (curr.terminated) break;
            if (elem.position_x <= curr.origin.x) continue;

            double dx = elem.position_x - curr.origin.x;
            if (std::abs(curr.direction.x) < 1e-9) {
                // Ray is travelling parallel to optical surface; misses
                continue;
            }

            double dist = dx / curr.direction.x;
            double hit_y = curr.origin.y + curr.direction.y * dist;

            // Check aperture clip
            if (std::abs(hit_y) > 0.5 * elem.height) {
                curr.terminated = true;
                break;
            }

            curr.origin = Vector2D{elem.position_x, hit_y};
            curr.optical_path_length += dist;

            if (elem.type == OpticalElementType::DetectorScreen) {
                elem.detected_ray_y_hits.push_back(hit_y);
                curr.terminated = true;
                trajectory.push_back(curr);
                break;
            }

            if (elem.type == OpticalElementType::ThinLens) {
                // Thin lens paraxial deflection: d'(y) = d(y) - y / f
                double new_dy = curr.direction.y - (hit_y / elem.focal_length);
                curr.direction = Vector2D{curr.direction.x, new_dy}.normalized();
                result.total_refractions++;
            } else if (elem.type == OpticalElementType::FlatMirror) {
                Vector2D normal{-1.0, 0.0};
                curr.direction = reflect(curr.direction, normal);
                result.total_reflections++;
            } else if (elem.type == OpticalElementType::RefractiveInterface) {
                Vector2D normal{1.0, 0.0};
                auto refracted = refract(curr.direction, normal, elem.n1, elem.n2);
                if (refracted.has_value()) {
                    curr.direction = *refracted;
                    result.total_refractions++;
                } else {
                    // Total Internal Reflection
                    curr.direction = reflect(curr.direction, Vector2D{-1.0, 0.0});
                    result.total_tir_events++;
                }
            }

            trajectory.push_back(curr);
        }

        result.ray_trajectories.push_back(std::move(trajectory));
    }

    elements_ = sorted_elements;
    return result;
}

// =============================================================================
// WaveOpticsModel Implementation
// =============================================================================

double WaveOpticsModel::rayleigh_range(double w0, double wavelength_m) noexcept {
    if (wavelength_m <= 0.0) return 0.0;
    constexpr double PI = 3.14159265358979323846;
    return (PI * w0 * w0) / wavelength_m;
}

double WaveOpticsModel::gaussian_beam_waist(double w0, double wavelength_m, double z) noexcept {
    double z_R = rayleigh_range(w0, wavelength_m);
    if (z_R <= 0.0) return w0;
    double ratio = z / z_R;
    return w0 * std::sqrt(1.0 + ratio * ratio);
}

// =============================================================================
// OpticsValidationBenchmark Implementation
// =============================================================================

double OpticsValidationBenchmark::image_distance(double focal_length, double object_distance) noexcept {
    double diff = object_distance - focal_length;
    if (std::abs(diff) < 1e-12) {
        return 1.0e12; // At focal point: image at infinity
    }
    return (focal_length * object_distance) / diff;
}

double OpticsValidationBenchmark::lensmaker_focal_length(double n, double r1, double r2) noexcept {
    double n_minus_1 = n - 1.0;
    if (std::abs(n_minus_1) < 1e-12) return 1.0e12;
    double curvature_term = (1.0 / r1) - (1.0 / r2);
    if (std::abs(curvature_term) < 1e-12) return 1.0e12;
    return 1.0 / (n_minus_1 * curvature_term);
}

} // namespace shunya::core::domain::optics
