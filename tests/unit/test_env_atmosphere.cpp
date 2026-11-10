#include "tests/test_framework.hpp"

#include "shunya/env/atmospheric_model.hpp"
#include "shunya/env/psychrometric_model.hpp"
#include "shunya/env/wind_field_model.hpp"

#include <cmath>
#include <vector>

using namespace shunya::env;

TEST_CASE("ENV-01-001 US Standard Atmosphere 1976 altitude layers") {
    // Sea level (0 m)
    auto sea = AtmosphericModel::evaluate_geopotential(0.0);
    REQUIRE(sea.is_ok());
    CHECK(std::abs(sea.value().temperature_k - 288.15) < 1e-3);
    CHECK(std::abs(sea.value().pressure_pa - 101325.0) < 1.0);
    CHECK(std::abs(sea.value().density_kg_m3 - 1.2250) < 1e-3);
    CHECK(std::abs(sea.value().speed_of_sound_m_s - 340.29) < 0.1);
    CHECK(sea.value().layer_index == 0);

    // Tropopause (11,000 m geopotential): P = 22.632 kPa, T = 216.65 K
    auto trop = AtmosphericModel::evaluate_geopotential(11000.0);
    REQUIRE(trop.is_ok());
    CHECK(std::abs(trop.value().temperature_k - 216.65) < 1e-3);
    CHECK(std::abs(trop.value().pressure_pa - 22632.06) < 5.0);
    CHECK(std::abs(trop.value().density_kg_m3 - 0.3639) < 1e-3);

    // Stratosphere (32,000 m): P ~ 868 Pa, T = 228.65 K
    auto strat = AtmosphericModel::evaluate_geopotential(32000.0);
    REQUIRE(strat.is_ok());
    CHECK(std::abs(strat.value().temperature_k - 228.65) < 0.05);
    CHECK(std::abs(strat.value().pressure_pa - 868.02) < 2.0);

    // Mesosphere (80,000 m geometric)
    auto meso = AtmosphericModel::evaluate_geometric(80000.0);
    REQUIRE(meso.is_ok());
    CHECK(meso.value().temperature_k < 205.0);
    CHECK(meso.value().pressure_pa > 0.5);
    CHECK(meso.value().pressure_pa < 2.0);

    // Out-of-bounds rejection
    CHECK_FALSE(AtmosphericModel::evaluate_geometric(95000.0).is_ok());
}

TEST_CASE("ENV-01-001 Psychrometric Magnus-Tetens humidity, dew point, and moist density") {
    // 50% RH at 20 degC (293.15 K): ASHRAE dew point ~ 9.27 degC (282.42 K)
    auto mid = PsychrometricModel::evaluate(293.15, 50.0, 101325.0);
    REQUIRE(mid.is_ok());
    const double td_c = mid.value().dew_point_k - 273.15;
    CHECK(std::abs(td_c - 9.27) < 0.1);
    CHECK(mid.value().wet_bulb_k < 293.15);
    CHECK(mid.value().wet_bulb_k > mid.value().dew_point_k);

    // Dry air (0% RH at 20 C): rho_dry = 101325 / (287.058 * 293.15) = 1.2041 kg/m^3
    auto dry = PsychrometricModel::evaluate(293.15, 0.0, 101325.0);
    REQUIRE(dry.is_ok());
    CHECK(std::abs(dry.value().moist_air_density_kg_m3 - 1.2041) < 1e-3);
    // Moist air is less dense than dry air at identical T & P
    CHECK(mid.value().moist_air_density_kg_m3 < dry.value().moist_air_density_kg_m3);

    // Saturated air (100% RH at 35 degC): T_wb == T_dp == T_db
    auto sat = PsychrometricModel::evaluate(308.15, 100.0, 101325.0);
    REQUIRE(sat.is_ok());
    CHECK(std::abs(sat.value().dew_point_k - 308.15) < 0.02);
    CHECK(std::abs(sat.value().wet_bulb_k - 308.15) < 0.02);
}

TEST_CASE("ENV-01-001 Wind field logarithmic boundary layer shear and Dryden turbulence PSD") {
    WindFieldConfig cfg;
    cfg.reference_speed_m_s = 12.0;
    cfg.reference_height_m = 10.0;
    cfg.roughness_length_z0_m = 0.05;
    cfg.enable_turbulence = false;

    WindFieldModel wind(cfg);
    CHECK(std::abs(wind.mean_speed_at_height(10.0) - 12.0) < 1e-9);
    CHECK(wind.mean_speed_at_height(0.02) == 0.0); // below z0
    CHECK(wind.mean_speed_at_height(2.0) < wind.mean_speed_at_height(10.0));
    CHECK(wind.mean_speed_at_height(50.0) > wind.mean_speed_at_height(10.0));

    // Verify logarithmic scaling ratio ln(z2/z0) / ln(z1/z0)
    const double ratio_expected = std::log(50.0 / 0.05) / std::log(10.0 / 0.05);
    CHECK(std::abs((wind.mean_speed_at_height(50.0) / 12.0) - ratio_expected) < 1e-6);

    // Enable Dryden turbulence and verify nonzero RMS variance and decaying high-frequency PSD
    cfg.enable_turbulence = true;
    cfg.turbulence_intensity = 0.15; // sigma_u = 1.8 m/s
    WindFieldModel turb_wind(cfg);

    double sum_sq_u = 0.0;
    const int n_samples = 400;
    for (int i = 0; i < n_samples; ++i) {
        const auto v = turb_wind.velocity_at(10.0, i * 0.25);
        const double du = v.x - 12.0;
        sum_sq_u += du * du;
    }
    const double rms_u = std::sqrt(sum_sq_u / n_samples);
    CHECK(rms_u > 1.0);
    CHECK(rms_u < 2.6);
    CHECK(turb_wind.turbulence().dryden_psd_u(0.1) > turb_wind.turbulence().dryden_psd_u(5.0));
}
