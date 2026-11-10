#include "shunya/env/solar_position.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

double wrap_360(double deg) noexcept {
    double r = std::fmod(deg, 360.0);
    if (r < 0.0) r += 360.0;
    return r;
}

double wrap_1440(double minutes) noexcept {
    double r = std::fmod(minutes, 1440.0);
    if (r < 0.0) r += 1440.0;
    return r;
}

double to_julian_day(int year, int month, int day, double utc_hour) noexcept {
    int y = year;
    int m = month;
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    const int a = y / 100;
    const int b = 2 - a + (a / 4);
    const double jd_midnight = std::floor(365.25 * (y + 4716)) +
                               std::floor(30.6001 * (m + 1)) +
                               static_cast<double>(day) + b - 1524.5;
    return jd_midnight + (utc_hour / 24.0);
}

} // namespace

int SolarPositionModel::day_of_year(int year, int month, int day) noexcept {
    static constexpr int kMonthDays[12]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    int doy = day;
    for (int m = 1; m < month && m <= 12; ++m) {
        doy += kMonthDays[m - 1];
        if (m == 2 && is_leap) {
            doy += 1;
        }
    }
    return std::clamp(doy, 1, 366);
}

core::base::Result<SolarPositionResult> SolarPositionModel::evaluate_utc(
    const SolarDateTime& utc_time,
    const ObserverLocation& location) {
    if (utc_time.month < 1 || utc_time.month > 12 || utc_time.day < 1 || utc_time.day > 31) {
        return core::base::Error{
            321, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid calendar month/day", ""
        };
    }
    if (location.latitude_deg < -90.0 || location.latitude_deg > 90.0 ||
        location.longitude_deg < -180.0 || location.longitude_deg > 180.0) {
        return core::base::Error{
            322, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Observer latitude/longitude out of bounds", ""
        };
    }

    const double jd = to_julian_day(utc_time.year, utc_time.month, utc_time.day, utc_time.utc_hour);
    const double t = (jd - 2451545.0) / 36525.0;

    // NOAA Solar Equations
    const double l0_deg = wrap_360(280.46646 + t * (36000.76983 + t * 0.0003032));
    const double m_deg = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    const double m_rad = m_deg * kDegToRad;
    const double ecc = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);

    const double sun_eq_ctr =
        std::sin(m_rad) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
        std::sin(2.0 * m_rad) * (0.019993 - 0.000101 * t) +
        std::sin(3.0 * m_rad) * 0.000289;

    const double sun_true_long_deg = l0_deg + sun_eq_ctr;
    const double omega_rad = (125.04 - 1934.136 * t) * kDegToRad;
    const double sun_app_long_rad =
        (sun_true_long_deg - 0.00569 - 0.00478 * std::sin(omega_rad)) * kDegToRad;

    const double mean_obliq_arcsec =
        21.448 - t * (46.8150 + t * (0.00059 - t * 0.001813));
    const double mean_obliq_deg = 23.0 + (26.0 + mean_obliq_arcsec / 60.0) / 60.0;
    const double obliq_corr_rad =
        (mean_obliq_deg + 0.00256 * std::cos(omega_rad)) * kDegToRad;

    const double sin_decl = std::sin(obliq_corr_rad) * std::sin(sun_app_long_rad);
    const double decl_rad = std::asin(std::clamp(sin_decl, -1.0, 1.0));
    const double decl_deg = decl_rad * kRadToDeg;

    const double var_y = std::pow(std::tan(obliq_corr_rad * 0.5), 2);
    const double l0_rad = l0_deg * kDegToRad;
    const double eot_min = 4.0 * kRadToDeg * (
        var_y * std::sin(2.0 * l0_rad) -
        2.0 * ecc * std::sin(m_rad) +
        4.0 * ecc * var_y * std::sin(m_rad) * std::cos(2.0 * l0_rad) -
        0.5 * var_y * var_y * std::sin(4.0 * l0_rad) -
        1.25 * ecc * ecc * std::sin(2.0 * m_rad)
    );

    const double true_solar_time_min =
        wrap_1440(utc_time.utc_hour * 60.0 + eot_min + 4.0 * location.longitude_deg);
    double hour_angle_deg = true_solar_time_min / 4.0 - 180.0;
    const double ha_rad = hour_angle_deg * kDegToRad;
    const double lat_rad = location.latitude_deg * kDegToRad;

    // Exact East-North-Up unit direction to Sun
    const double east  = -std::cos(decl_rad) * std::sin(ha_rad);
    const double north = std::cos(lat_rad) * std::sin(decl_rad) -
                         std::sin(lat_rad) * std::cos(decl_rad) * std::cos(ha_rad);
    const double up    = std::sin(lat_rad) * std::sin(decl_rad) +
                         std::cos(lat_rad) * std::cos(decl_rad) * std::cos(ha_rad);

    const double elev_geom_deg = std::asin(std::clamp(up, -1.0, 1.0)) * kRadToDeg;

    // NOAA atmospheric refraction correction
    double refract_deg = 0.0;
    if (elev_geom_deg > 85.0) {
        refract_deg = 0.0;
    } else if (elev_geom_deg > 5.0) {
        const double t_el = std::tan(elev_geom_deg * kDegToRad);
        refract_deg = (58.1 / t_el - 0.07 / std::pow(t_el, 3) + 0.000086 / std::pow(t_el, 5)) / 3600.0;
    } else if (elev_geom_deg > -0.575) {
        refract_deg = (1735.0 + elev_geom_deg * (-518.2 + elev_geom_deg *
                      (103.4 + elev_geom_deg * (-12.79 + elev_geom_deg * 0.711)))) / 3600.0;
    }

    const double elev_deg = std::min(90.0, elev_geom_deg + refract_deg);
    const double zenith_deg = 90.0 - elev_deg;
    const double az_deg = wrap_360(std::atan2(east, north) * kRadToDeg);

    const int doy = day_of_year(utc_time.year, utc_time.month, utc_time.day);
    SolarPositionResult res;
    res.julian_day = jd;
    res.declination_deg = decl_deg;
    res.equation_of_time_min = eot_min;
    res.solar_noon_utc_hours = (720.0 - 4.0 * location.longitude_deg - eot_min) / 60.0;
    res.azimuth_deg = az_deg;
    res.elevation_deg = elev_deg;
    res.zenith_deg = zenith_deg;
    res.irradiance = SolarFluxModel::evaluate_clear_sky(elev_deg, doy, location.altitude_m);
    res.sun_direction_enu = {east, north, up};
    return res;
}

} // namespace shunya::env
