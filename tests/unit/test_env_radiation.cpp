#include "tests/test_framework.hpp"

#include "shunya/env/ambient_convection.hpp"
#include "shunya/env/deep_space_sink.hpp"

#include <cmath>

using namespace shunya::env;

TEST_CASE("ENV-02-002 Ambient McAdams wind-driven convection vs lumped capacitance model") {
    // McAdams correlation h = 5.7 + 3.8 * v
    const double h0 = AmbientConvectionBoundary::mcadams_h_w_m2_k(0.0);
    const double h10 = AmbientConvectionBoundary::mcadams_h_w_m2_k(10.0);
    CHECK(std::abs(h0 - 5.7) < 1e-9);
    CHECK(std::abs(h10 - 43.7) < 1e-9);
    // Accelerates cooling rate by well over a factor of 4
    CHECK(h10 / h0 > 7.0);

    // Heated aluminum heatsink: T0 = 373.15 K (100 C), Tamb = 298.15 K (25 C), A = 0.04 m^2, C = 180 J/K
    auto cool_0 = AmbientConvectionBoundary::simulate_lumped_cooling(
        373.15, 298.15, 0.04, 180.0, 0.0, 200.0, 0.5);
    auto cool_10 = AmbientConvectionBoundary::simulate_lumped_cooling(
        373.15, 298.15, 0.04, 180.0, 10.0, 200.0, 0.5);
    REQUIRE(cool_0.is_ok());
    REQUIRE(cool_10.is_ok());

    const double t_exact_10 = AmbientConvectionBoundary::analytical_lumped_temp_k(
        373.15, 298.15, 0.04, 180.0, 10.0, 200.0);
    CHECK(std::abs(cool_10.value().back() - t_exact_10) < 0.1);
    CHECK(cool_10.value().back() < cool_0.value().back() - 25.0);
}

TEST_CASE("ENV-02-002 Berdahl-Martin night-sky sub-ambient cooling and orbital 2.7 K deep space sink") {
    // 1. Berdahl-Martin sky temperature at T_amb = 293.15 K (20 C), T_dp = 278.15 K (5 C)
    const double eps_sky = DeepSpaceRadiationSink::clear_sky_emissivity(278.15);
    const double t_sky = DeepSpaceRadiationSink::effective_sky_temp_k(293.15, 278.15);
    CHECK(std::abs(eps_sky - 0.740825) < 1e-4);
    CHECK(std::abs(t_sky - 271.97) < 0.2);

    // 2. Insulated nocturnal radiative panel cools significantly BELOW ambient air temperature (293.15 K)
    auto nocturnal = DeepSpaceRadiationSink::simulate_nocturnal_cooling(
        293.15, 293.15, 278.15, 1.0, 3000.0, 0.95, 1.5, 3600.0, 5.0,
        SkySinkMode::ClearNightSkyBerdahlMartin);
    REQUIRE(nocturnal.is_ok());
    CHECK(nocturnal.value().initial_net_cooling_w > 90.0);
    CHECK(nocturnal.value().temperature_history_k.back() < 285.0); // > 8 K below ambient

    // 3. Orbital deep-space vacuum sink T_space = 2.7 K
    auto space = DeepSpaceRadiationSink::simulate_nocturnal_cooling(
        300.0, 300.0, 278.15, 1.0, 2000.0, 0.90, 0.0, 1800.0, 2.0,
        SkySinkMode::OrbitalDeepSpaceVacuum);
    REQUIRE(space.is_ok());
    CHECK(std::abs(space.value().effective_sky_temp_k - 2.7) < 1e-9);
    // Analytical pure T^4 cooling at t=1800s gives T = (T0^-3 + 3*k*t)^(-1/3) ~ 178.8 K
    CHECK(std::abs(space.value().temperature_history_k.back() - 178.8) < 1.5);
}
