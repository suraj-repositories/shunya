#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/env/solar_flux.hpp"

namespace shunya::env {

struct SolarDateTime {
    int year{2026};
    int month{3};
    int day{20};
    double utc_hour{12.0}; // [0, 24)
};

struct ObserverLocation {
    double latitude_deg{0.0};   // [-90, +90]
    double longitude_deg{0.0};  // [-180, +180], East positive
    double altitude_m{0.0};
};

struct SolarPositionResult {
    double julian_day{0.0};
    double declination_deg{0.0};
    double equation_of_time_min{0.0};
    double solar_noon_utc_hours{12.0};
    double azimuth_deg{180.0};    // 0 = North, 90 = East, 180 = South, 270 = West
    double elevation_deg{0.0};    // [-90, +90] above horizon
    double zenith_deg{90.0};
    SolarIrradianceSample irradiance;
    core::base::math::Vector3<double> sun_direction_enu{0.0, 0.0, 0.0}; // East, North, Up unit vector
};

/**
 * @brief NOAA-grade astronomical solar position and clear-sky irradiance calculator.
 */
class SolarPositionModel {
public:
    [[nodiscard]] static core::base::Result<SolarPositionResult> evaluate_utc(
        const SolarDateTime& utc_time,
        const ObserverLocation& location);

    [[nodiscard]] static int day_of_year(int year, int month, int day) noexcept;
};

} // namespace shunya::env
