#include "tests/test_framework.hpp"
#include "shunya/core/domain/optics.hpp"

#include <cmath>
#include <vector>

using namespace shunya::core::domain::optics;

// =============================================================================
// OPT-01 & OPT-02: Optical Materials, Dispersion and Abbe Number
// =============================================================================

TEST_CASE("OPT-01 & OPT-02: Optical Material Sellmeier Dispersion and Abbe Number") {
    // Standard BK7 glass Sellmeier parameters
    SellmeierCoefficients bk7_sellmeier{
        1.03961212, 0.00600069867,
        0.231792344, 0.0200179144,
        1.01046945, 103.560653
    };

    auto bk7 = OpticalMaterial::create_dispersive_glass("N-BK7", bk7_sellmeier);

    // Refractive index at yellow d-line (0.58756 um) is ~1.5168
    double n_d = bk7.refractive_index(0.58756);
    CHECK(std::abs(n_d - 1.5168) < 1e-3);

    // Blue light (0.486 um) has higher index than red light (0.656 um)
    double n_blue = bk7.refractive_index(0.48613);
    double n_red = bk7.refractive_index(0.65627);
    CHECK(n_blue > n_red);

    // Abbe number of N-BK7 is ~64.17 (low dispersion crown glass)
    double v_d = bk7.abbe_number();
    CHECK(v_d > 60.0);
    CHECK(v_d < 70.0);

    // Non-dispersive air
    OpticalMaterial air("Air", 1.00029, 0.0);
    CHECK(std::abs(air.refractive_index(0.5) - 1.00029) < 1e-6);
    CHECK(air.absorption_coefficient(0.5) == 0.0);
}

// =============================================================================
// OPT-03 & OPT-04: Snell's Law, Critical Angle and Total Internal Reflection
// =============================================================================

TEST_CASE("OPT-03 & OPT-04: Snell Refraction and Total Internal Reflection") {
    // Air (n1 = 1.0) to Glass (n2 = 1.5)
    Vector2D inc_ray{std::sin(0.5235987756), -std::cos(0.5235987756)}; // 30 degrees incidence
    Vector2D normal{0.0, 1.0};

    auto refracted = RayTracerEngine::refract(inc_ray, normal, 1.0, 1.5);
    REQUIRE(refracted.has_value());

    // By Snell's Law: 1.0 * sin(30 deg) = 1.5 * sin(theta2) => sin(theta2) = 0.5 / 1.5 = 1/3
    // theta2 = asin(1/3) ~ 0.3398 rad
    double theta2 = std::asin(std::abs(refracted->x));
    CHECK(std::abs(theta2 - std::asin(1.0 / 3.0)) < 1e-4);

    // Glass (n1 = 1.5) to Air (n2 = 1.0) Critical Angle
    auto crit_angle = RayTracerEngine::critical_angle_radians(1.5, 1.0);
    REQUIRE(crit_angle.has_value());
    // theta_c = asin(1 / 1.5) = asin(0.6667) ~ 0.7297 rad (41.81 degrees)
    CHECK(std::abs(*crit_angle - std::asin(1.0 / 1.5)) < 1e-4);

    // Angle of incidence greater than critical angle: 60 degrees (1.047 rad) > 41.81 degrees
    Vector2D tir_ray{std::sin(1.047), -std::cos(1.047)};
    auto tir_refracted = RayTracerEngine::refract(tir_ray, normal, 1.5, 1.0);
    CHECK_FALSE(tir_refracted.has_value()); // Total Internal Reflection occurs
}

// =============================================================================
// OPT-04: Ray Tracing through Thin Lens onto Detector Screen
// =============================================================================

TEST_CASE("OPT-04: Ray Tracing through Thin Lens Focusing onto Detector") {
    RayTracerEngine engine;

    // Lens at x = 0.5m with focal length f = 0.2m
    OpticalElement lens;
    lens.id = "lens_1";
    lens.type = OpticalElementType::ThinLens;
    lens.position_x = 0.5;
    lens.height = 0.1;
    lens.focal_length = 0.2;
    engine.add_element(lens);

    // Detector screen at focal plane x = 0.5 + 0.2 = 0.7m
    OpticalElement detector;
    detector.id = "screen";
    detector.type = OpticalElementType::DetectorScreen;
    detector.position_x = 0.7;
    detector.height = 0.1;
    engine.add_element(detector);

    // Collimated parallel beam entering horizontally at different heights: y = +0.02, 0.0, -0.02
    std::vector<OpticalRay> rays{
        OpticalRay{Vector2D{0.0, 0.02}, Vector2D{1.0, 0.0}, 0.587, 1.0, 0.0, false},
        OpticalRay{Vector2D{0.0, 0.00}, Vector2D{1.0, 0.0}, 0.587, 1.0, 0.0, false},
        OpticalRay{Vector2D{0.0, -0.02}, Vector2D{1.0, 0.0}, 0.587, 1.0, 0.0, false}
    };

    auto trace_res = engine.trace_rays(rays);
    CHECK(trace_res.total_rays_traced == 3u);
    CHECK(trace_res.total_refractions == 3u);

    // All parallel rays must converge at the focal point (y = 0.0) at the detector screen
    const auto& elems = engine.elements();
    REQUIRE(elems.size() == 2u);
    const auto& det_screen = elems[1];
    REQUIRE(det_screen.detected_ray_y_hits.size() == 3u);

    for (double hit_y : det_screen.detected_ray_y_hits) {
        CHECK(std::abs(hit_y) < 1e-4); // Perfect focus at detector
    }
}

// =============================================================================
// OPT-05: Wave Optics Gaussian Beam Propagation
// =============================================================================

TEST_CASE("OPT-05: Wave Optics Gaussian Beam Rayleigh Range and Expansion") {
    double w0 = 1.0e-3; // 1 mm beam waist
    double lambda = 632.8e-9; // He-Ne laser 632.8 nm

    double z_R = WaveOpticsModel::rayleigh_range(w0, lambda);
    CHECK(z_R > 4.0); // ~4.96 meters

    // At z = 0, beam waist w = w0
    double w_at_0 = WaveOpticsModel::gaussian_beam_waist(w0, lambda, 0.0);
    CHECK(std::abs(w_at_0 - w0) < 1e-9);

    // At z = z_R, beam waist w = w0 * sqrt(2) ~ 1.414 * w0
    double w_at_zR = WaveOpticsModel::gaussian_beam_waist(w0, lambda, z_R);
    CHECK(std::abs(w_at_zR - (w0 * std::sqrt(2.0))) < 1e-9);
}

// =============================================================================
// OPT-06: Thin Lens Imaging and Lensmaker Benchmarks
// =============================================================================

TEST_CASE("OPT-06: Thin Lens Imaging and Lensmaker Benchmarks") {
    double f = 0.1; // 100 mm focal length
    double d_o = 0.3; // 300 mm object distance

    // 1/f = 1/d_o + 1/d_i => 1/0.1 = 1/0.3 + 1/d_i => 10 = 3.333 + 1/d_i => 1/d_i = 6.6667 => d_i = 0.15 m
    double d_i = OpticsValidationBenchmark::image_distance(f, d_o);
    CHECK(std::abs(d_i - 0.15) < 1e-4);

    // Symmetric biconvex lens: n = 1.5, R1 = 0.1 m, R2 = -0.1 m
    // 1/f = (1.5 - 1) * (1/0.1 - (-1/0.1)) = 0.5 * (10 + 10) = 0.5 * 20 = 10 => f = 0.1 m
    double f_calc = OpticsValidationBenchmark::lensmaker_focal_length(1.5, 0.1, -0.1);
    CHECK(std::abs(f_calc - 0.1) < 1e-4);
}
