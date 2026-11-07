#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::core::domain::optics {

// =============================================================================
// OPT-01 & OPT-02: Optical Materials, Dispersion & Spectrum
// =============================================================================

enum class OpticsModelType {
    GeometricRayOptics,
    WaveGaussianBeam,
    ElectromagneticFDTD
};

struct SellmeierCoefficients {
    double b1{1.03961212};
    double c1{0.00600069867}; // um^2
    double b2{0.231792344};
    double c2{0.0200179144};
    double b3{1.01046945};
    double c3{103.560653};
};

class OpticalMaterial {
public:
    explicit OpticalMaterial(std::string name, double constant_n = 1.0, double extinction_k = 0.0)
        : name_(std::move(name)), constant_n_(constant_n), extinction_k_(extinction_k) {}

    static OpticalMaterial create_dispersive_glass(std::string name, SellmeierCoefficients sellmeier) {
        OpticalMaterial mat(std::move(name));
        mat.sellmeier_ = sellmeier;
        mat.is_dispersive_ = true;
        return mat;
    }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    // Refractive index n at wavelength (in micrometers, default d-line 0.58756 um)
    [[nodiscard]] double refractive_index(double wavelength_um = 0.58756) const noexcept;

    // Absorption coefficient alpha = 4 * pi * k / lambda (m^-1)
    [[nodiscard]] double absorption_coefficient(double wavelength_um = 0.58756) const noexcept;

    // Abbe number V_d = (n_d - 1) / (n_F - n_C)
    [[nodiscard]] double abbe_number() const noexcept;

private:
    std::string name_;
    double constant_n_{1.0};
    double extinction_k_{0.0};
    bool is_dispersive_{false};
    SellmeierCoefficients sellmeier_{};
};

// =============================================================================
// OPT-03: Rays & Optical Geometry (Lenses, Mirrors, Detectors)
// =============================================================================

struct Vector2D {
    double x{0.0};
    double y{0.0};

    [[nodiscard]] double length() const noexcept { return std::sqrt(x * x + y * y); }
    [[nodiscard]] Vector2D normalized() const noexcept {
        double len = length();
        return len > 0.0 ? Vector2D{x / len, y / len} : Vector2D{0.0, 0.0};
    }
    [[nodiscard]] double dot(const Vector2D& o) const noexcept { return x * o.x + y * o.y; }
};

struct OpticalRay {
    Vector2D origin{};
    Vector2D direction{1.0, 0.0}; // Normalized direction
    double wavelength_um{0.58756}; // Default yellow d-line 587.56 nm
    double intensity{1.0};         // Normalized power (0.0 to 1.0)
    double optical_path_length{0.0};
    bool terminated{false};
};

enum class OpticalElementType {
    ThinLens,
    FlatMirror,
    CurvedMirror,
    DetectorScreen,
    RefractiveInterface
};

struct OpticalElement {
    std::string id;
    OpticalElementType type{OpticalElementType::ThinLens};
    double position_x{0.0};    // Position along optical axis
    double height{0.05};       // Aperture height (m)
    double focal_length{0.1};  // Thin lens focal length (m)
    double curvature_radius{0.2}; // For curved mirror/surface
    double n1{1.0};            // Refractive index on left
    double n2{1.5};            // Refractive index on right
    std::vector<double> detected_ray_y_hits{}; // Log of hit positions on detector
};

// =============================================================================
// OPT-04 & OPT-06: Ray Tracing Engine & Snell's Law
// =============================================================================

struct RayTraceResult {
    std::vector<std::vector<OpticalRay>> ray_trajectories;
    size_t total_rays_traced{0};
    size_t total_reflections{0};
    size_t total_refractions{0};
    size_t total_tir_events{0}; // Total Internal Reflection
};

class RayTracerEngine {
public:
    RayTracerEngine() = default;

    void add_element(OpticalElement elem) { elements_.push_back(std::move(elem)); }
    [[nodiscard]] const std::vector<OpticalElement>& elements() const noexcept { return elements_; }
    void clear_elements() noexcept { elements_.clear(); }

    // Evaluates Snell's law: n1 * sin(theta1) = n2 * sin(theta2)
    // Returns transmitted direction, or nullopt if Total Internal Reflection (TIR) occurs
    static std::optional<Vector2D> refract(
        const Vector2D& inc_dir,
        const Vector2D& normal,
        double n1,
        double n2);

    // Evaluates Law of Reflection: r = d - 2 * (d . n) * n
    static Vector2D reflect(const Vector2D& inc_dir, const Vector2D& normal);

    // Calculates critical angle for TIR: theta_c = asin(n2 / n1) for n1 > n2
    static std::optional<double> critical_angle_radians(double n1, double n2);

    // Traces a batch of rays through the optical bench elements
    [[nodiscard]] RayTraceResult trace_rays(const std::vector<OpticalRay>& incoming_rays);

private:
    std::vector<OpticalElement> elements_;
};

// =============================================================================
// OPT-05: Wave & Electromagnetic Optics Abstraction
// =============================================================================

class WaveOpticsModel {
public:
    // Gaussian beam waist w(z) = w0 * sqrt(1 + (z / z_R)^2), where z_R = pi * w0^2 / lambda
    static double gaussian_beam_waist(double w0, double wavelength_m, double z) noexcept;

    // Rayleigh range z_R = pi * w0^2 / lambda
    static double rayleigh_range(double w0, double wavelength_m) noexcept;
};

// =============================================================================
// OPT-06: Analytical Validation Benchmark
// =============================================================================

class OpticsValidationBenchmark {
public:
    // Thin lens imaging formula: 1/f = 1/d_o + 1/d_i => d_i = (f * d_o) / (d_o - f)
    static double image_distance(double focal_length, double object_distance) noexcept;

    // Lensmaker equation: 1/f = (n - 1) * (1/R1 - 1/R2)
    static double lensmaker_focal_length(double n, double r1, double r2) noexcept;
};

} // namespace shunya::core::domain::optics
