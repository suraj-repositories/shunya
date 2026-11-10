#include "tests/test_framework.hpp"

#include "shunya/env/gravity_field.hpp"
#include "shunya/env/solar_position.hpp"

#include <cmath>

using namespace shunya::env;

TEST_CASE("ENV-01-002 Planetary gravity presets, Newtonian central gradient, and pendulum period") {
    GravityFieldModel g_field;
    CHECK(std::abs(g_field.magnitude_at() - 9.80665) < 1e-6);

    // Switch to Lunar preset: immediate 1.62 m/s^2 acceleration
    g_field.set_preset(GravityPreset::Moon);
    CHECK(std::abs(g_field.magnitude_at() - 1.62) < 1e-6);
    CHECK(std::abs(g_field.acceleration_at().y + 1.62) < 1e-6);

    // Switch to Mars and Jupiter and compare 1m pendulum period T = 2*pi*sqrt(L/g)
    const double t_moon = g_field.pendulum_period_s(1.0);
    g_field.set_preset(GravityPreset::Earth);
    const double t_earth = g_field.pendulum_period_s(1.0);
    g_field.set_preset(GravityPreset::Jupiter);
    const double t_jupiter = g_field.pendulum_period_s(1.0);

    CHECK(t_moon > t_earth);
    CHECK(t_earth > t_jupiter);
    CHECK(std::abs(t_earth - 2.0064) < 0.01);

    // ZeroG yields 0 vector and infinite pendulum period
    g_field.set_preset(GravityPreset::ZeroG);
    CHECK(g_field.magnitude_at() == 0.0);
    CHECK(std::isinf(g_field.pendulum_period_s(1.0)));

    // Central Newtonian altitude lapse: g at LEO (400 km above Earth) vs surface
    auto central = GravityFieldModel::preset_config(GravityPreset::Earth);
    central.preset = GravityPreset::CentralNewtonian;
    g_field.set_config(central);
    const double g_surf = g_field.magnitude_at({0.0, 0.0, 0.0});
    const double g_400km = g_field.magnitude_at({0.0, 400000.0, 0.0});
    CHECK(std::abs(g_surf - 9.82) < 0.05);
    CHECK(std::abs(g_400km - 8.69) < 0.05);
}

TEST_CASE("ENV-01-002 NOAA solar position and clear-sky irradiance at equinox and mid-latitudes") {
    // 1. Vernal equinox 2026: March 20, ~14:46 UTC exact declination zero;
    // at equator (0,0) solar noon (12:07:36 UTC) -> zenith ~0, elevation ~90 deg, GHI ~ 1000 W/m^2
    ObserverLocation equator{0.0, 0.0, 0.0};
    SolarDateTime eq_noon{2026, 3, 20, 12.126};
    auto eq_res = SolarPositionModel::evaluate_utc(eq_noon, equator);
    REQUIRE(eq_res.is_ok());
    CHECK(eq_res.value().elevation_deg > 89.7);
    CHECK(eq_res.value().irradiance.global_horizontal_w_m2 > 950.0);
    CHECK(eq_res.value().irradiance.global_horizontal_w_m2 < 1100.0);

    // 2. Compare against NOAA Solar Calculator reference point:
    // June 21, 2024 12:00 UTC at Greenwich (51.4779 N, 0.0015 W)
    // Summer solstice near solar noon: declination ~ +23.437 deg, elevation ~ 61.95 deg, azimuth ~ 179.4 deg
    ObserverLocation greenwich{51.4779, -0.0015, 0.0};
    SolarDateTime solstice{2024, 6, 21, 12.0};
    auto sol_res = SolarPositionModel::evaluate_utc(solstice, greenwich);
    REQUIRE(sol_res.is_ok());
    // NOAA analytical values for 2024-06-21 12:00 UTC at (51.4779, -0.0015):
    // Declination = +23.435 deg, Elevation ~ 61.97 deg, Azimuth ~ 179.07 deg (EoT = -1.9 min -> just East of South)
    CHECK(std::abs(sol_res.value().declination_deg - 23.435) < 0.05);
    CHECK(std::abs(sol_res.value().elevation_deg - 61.97) < 0.05);
    CHECK(std::abs(sol_res.value().azimuth_deg - 179.07) < 0.05);

    // Nighttime check (00:00 UTC at Greenwich -> negative elevation, zero solar flux)
    SolarDateTime midnight{2024, 6, 21, 0.0};
    auto night_res = SolarPositionModel::evaluate_utc(midnight, greenwich);
    REQUIRE(night_res.is_ok());
    CHECK(night_res.value().elevation_deg < 0.0);
    CHECK(night_res.value().irradiance.global_horizontal_w_m2 == 0.0);
}
