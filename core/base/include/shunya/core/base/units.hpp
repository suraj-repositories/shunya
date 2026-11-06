#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <sstream>

namespace shunya::core::base::units {

struct Dimension {
    int8_t length = 0;       // metre (m)
    int8_t mass = 0;         // kilogram (kg)
    int8_t time = 0;         // second (s)
    int8_t current = 0;      // ampere (A)
    int8_t temperature = 0;  // kelvin (K)
    int8_t amount = 0;       // mole (mol)
    int8_t luminosity = 0;   // candela (cd)

    constexpr auto operator<=>(const Dimension&) const = default;
    constexpr bool operator==(const Dimension&) const = default;

    [[nodiscard]] constexpr bool is_dimensionless() const noexcept {
        return length == 0 && mass == 0 && time == 0 && current == 0 &&
               temperature == 0 && amount == 0 && luminosity == 0;
    }

    constexpr Dimension operator*(const Dimension& rhs) const noexcept {
        return {
            static_cast<int8_t>(length + rhs.length),
            static_cast<int8_t>(mass + rhs.mass),
            static_cast<int8_t>(time + rhs.time),
            static_cast<int8_t>(current + rhs.current),
            static_cast<int8_t>(temperature + rhs.temperature),
            static_cast<int8_t>(amount + rhs.amount),
            static_cast<int8_t>(luminosity + rhs.luminosity)
        };
    }

    constexpr Dimension operator/(const Dimension& rhs) const noexcept {
        return {
            static_cast<int8_t>(length - rhs.length),
            static_cast<int8_t>(mass - rhs.mass),
            static_cast<int8_t>(time - rhs.time),
            static_cast<int8_t>(current - rhs.current),
            static_cast<int8_t>(temperature - rhs.temperature),
            static_cast<int8_t>(amount - rhs.amount),
            static_cast<int8_t>(luminosity - rhs.luminosity)
        };
    }

    constexpr Dimension pow(int exp) const noexcept {
        return {
            static_cast<int8_t>(length * exp),
            static_cast<int8_t>(mass * exp),
            static_cast<int8_t>(time * exp),
            static_cast<int8_t>(current * exp),
            static_cast<int8_t>(temperature * exp),
            static_cast<int8_t>(amount * exp),
            static_cast<int8_t>(luminosity * exp)
        };
    }

    [[nodiscard]] std::string symbol() const;
};

// -------------------------------------------------------------
// Base Dimensions (UNIT-01)
// -------------------------------------------------------------
inline constexpr Dimension Dimensionless{};
inline constexpr Dimension Length{1, 0, 0, 0, 0, 0, 0};
inline constexpr Dimension Mass{0, 1, 0, 0, 0, 0, 0};
inline constexpr Dimension Time{0, 0, 1, 0, 0, 0, 0};
inline constexpr Dimension ElectricCurrent{0, 0, 0, 1, 0, 0, 0};
inline constexpr Dimension Temperature{0, 0, 0, 0, 1, 0, 0};
inline constexpr Dimension AmountOfSubstance{0, 0, 0, 0, 0, 1, 0};
inline constexpr Dimension LuminousIntensity{0, 0, 0, 0, 0, 0, 1};

// -------------------------------------------------------------
// Derived Dimensions (UNIT-02)
// -------------------------------------------------------------
inline constexpr Dimension Frequency{0, 0, -1, 0, 0, 0, 0};                    // s^-1 (Hz)
inline constexpr Dimension Area{2, 0, 0, 0, 0, 0, 0};                          // m^2
inline constexpr Dimension Volume{3, 0, 0, 0, 0, 0, 0};                        // m^3
inline constexpr Dimension Velocity{1, 0, -1, 0, 0, 0, 0};                     // m/s
inline constexpr Dimension Acceleration{1, 0, -2, 0, 0, 0, 0};                 // m/s^2
inline constexpr Dimension Force{1, 1, -2, 0, 0, 0, 0};                        // kg*m/s^2 (Newton, N)
inline constexpr Dimension Pressure{-1, 1, -2, 0, 0, 0, 0};                    // N/m^2 (Pascal, Pa)
inline constexpr Dimension Energy{2, 1, -2, 0, 0, 0, 0};                       // N*m (Joule, J)
inline constexpr Dimension Power{2, 1, -3, 0, 0, 0, 0};                        // J/s (Watt, W)
inline constexpr Dimension ElectricCharge{0, 0, 1, 1, 0, 0, 0};                // A*s (Coulomb, C)
inline constexpr Dimension Voltage{2, 1, -3, -1, 0, 0, 0};                     // W/A (Volt, V)
inline constexpr Dimension ElectricalCapacitance{-2, -1, 4, 2, 0, 0, 0};       // C/V (Farad, F)
inline constexpr Dimension ElectricalResistance{2, 1, -3, -2, 0, 0, 0};        // V/A (Ohm, Ω)
inline constexpr Dimension ElectricalConductance{-2, -1, 3, 2, 0, 0, 0};       // A/V (Siemens, S)
inline constexpr Dimension MagneticFlux{2, 1, -2, -1, 0, 0, 0};                // V*s (Weber, Wb)
inline constexpr Dimension MagneticField{0, 1, -2, -1, 0, 0, 0};               // Wb/m^2 (Tesla, T)
inline constexpr Dimension Inductance{2, 1, -2, -2, 0, 0, 0};                  // Wb/A (Henry, H)

// -------------------------------------------------------------
// Quantity and Dimensional Analysis (UNIT-03)
// -------------------------------------------------------------
class Quantity {
public:
    double value = 0.0;
    Dimension dimension = Dimensionless;

    constexpr Quantity() noexcept = default;
    constexpr Quantity(double val, Dimension dim) noexcept : value(val), dimension(dim) {}

    [[nodiscard]] Result<Quantity> add(const Quantity& rhs) const {
        if (dimension != rhs.dimension) {
            return Result<Quantity>::err({
                .code = 3001,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Dimensional mismatch in addition: " + dimension.symbol() + " vs " + rhs.dimension.symbol(),
                .details = ""
            });
        }
        return Quantity(value + rhs.value, dimension);
    }

    [[nodiscard]] Result<Quantity> sub(const Quantity& rhs) const {
        if (dimension != rhs.dimension) {
            return Result<Quantity>::err({
                .code = 3002,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Dimensional mismatch in subtraction: " + dimension.symbol() + " vs " + rhs.dimension.symbol(),
                .details = ""
            });
        }
        return Quantity(value - rhs.value, dimension);
    }

    [[nodiscard]] Quantity mul(const Quantity& rhs) const noexcept {
        return Quantity(value * rhs.value, dimension * rhs.dimension);
    }

    [[nodiscard]] Quantity div(const Quantity& rhs) const {
        if (rhs.value == 0.0) {
            throw std::domain_error("Quantity division by zero");
        }
        return Quantity(value / rhs.value, dimension / rhs.dimension);
    }

    constexpr Quantity operator*(double scalar) const noexcept {
        return Quantity(value * scalar, dimension);
    }

    Quantity operator/(double scalar) const {
        if (scalar == 0.0) {
            throw std::domain_error("Quantity division by scalar zero");
        }
        return Quantity(value / scalar, dimension);
    }

    [[nodiscard]] std::string to_string() const;
};

inline Quantity operator*(double scalar, const Quantity& q) noexcept {
    return q * scalar;
}

[[nodiscard]] Result<void> check_compatible(const Dimension& a, const Dimension& b);
[[nodiscard]] Result<void> check_compatible(const Quantity& a, const Quantity& b);

} // namespace shunya::core::base::units
