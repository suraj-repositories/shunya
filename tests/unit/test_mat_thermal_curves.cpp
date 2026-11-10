#include "tests/test_framework.hpp"

#include "shunya/mat/monotonic_spline.hpp"
#include "shunya/mat/thermal_property_evaluator.hpp"
#include "shunya/mat/thermal_strain.hpp"

#include <cmath>

using namespace shunya::mat;

TEST_CASE("MonotonicCubicSpline & ThermalPropertyEvaluator: zero overshoot and positive NIST Cu k(T) 4K..1356K") {
    // Step data overshoot prevention: must remain strictly within [1.0, 10.0]
    auto step_res = MonotonicCubicSpline::create({0.0, 1.0, 2.0, 3.0, 4.0}, {1.0, 1.0, 10.0, 10.0, 10.0});
    REQUIRE(step_res.is_ok());
    const auto& sp = step_res.value();
    for (int i = 0; i <= 40; ++i) {
        const double x = static_cast<double>(i) * 0.1;
        const double y = sp.evaluate(x);
        REQUIRE(y >= 1.0 - 1e-9 && y <= 10.0 + 1e-9);
    }

    auto cu = ThermalPropertyEvaluator::copper_nist_cryogenic();
    for (double T = 4.0; T <= 1356.0; T += 12.0) {
        REQUIRE(cu.thermal_conductivity_w_mk(T) > 0.0);
        REQUIRE(cu.specific_heat_j_kg_k(T) > 0.0);
    }
    REQUIRE(std::abs(cu.thermal_conductivity_w_mk(298.15) - 401.0) < 1e-6);
    REQUIRE(cu.thermal_conductivity_w_mk(20.0) > 1000.0);
}

TEST_CASE("ThermalStrainEvaluator: zero strain at T_ref, nonlinear integration, and Timoshenko bimetallic strip") {
    // Constant alpha (Aluminum ~ 23.1e-6 /K)
    ThermalStrainEvaluator al_strain(23.1e-6, 293.15);
    REQUIRE(std::abs(al_strain.linear_thermal_strain(293.15)) < 1e-12);
    // 1 m Al bar heated by 100 K expands by 2.31 mm
    REQUIRE(std::abs(al_strain.linear_thermal_strain(393.15) - 2.31e-3) < 1e-7);
    REQUIRE(std::abs(al_strain.volumetric_thermal_strain(393.15) - 6.93e-3) < 1e-7);

    // Non-linear linear-ramping alpha(T) integral matches exact trapezoid area
    auto lin_alpha = MonotonicCubicSpline::create({200.0, 300.0, 400.0}, {10e-6, 12e-6, 14e-6}).value();
    ThermalStrainEvaluator nl(lin_alpha, 300.0);
    REQUIRE(std::abs(nl.linear_thermal_strain(300.0)) < 1e-12);
    // Integral from 300 to 400 of linear ramp 12e-6..14e-6 = 13e-6 * 100 = 1.3e-3
    REQUIRE(std::abs(nl.linear_thermal_strain(400.0) - 1.3e-3) < 1e-6);

    // Restrained steel thermal stress: compression (< 0) upon heating
    REQUIRE(ThermalStrainEvaluator(11.8e-6, 293.15).constrained_thermal_stress_pa(200e9, 393.15) < -230e6);

    // Bimetallic Brass (20.5e-6, 100 GPa) / Invar (1.2e-6, 145 GPa) strip (0.5 mm each, L=100 mm, dT=50 K)
    auto bi = ThermalStrainEvaluator::compute_bimetallic_deflection(
        0.10, 0.0005, 145e9, 1.2e-6, 0.0005, 100e9, 20.5e-6, 50.0);
    REQUIRE(bi.curvature_per_m > 0.5);
    REQUIRE(bi.tip_deflection_m > 0.0025);
}
