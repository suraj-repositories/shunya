#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/math.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <array>
#include <compare>
#include <cmath>

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

inline constexpr Dimension Resistance = ElectricalResistance;
inline constexpr Dimension Capacitance = ElectricalCapacitance;

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
    constexpr bool operator==(const Quantity& rhs) const noexcept = default;
};

inline Quantity operator*(double scalar, const Quantity& q) noexcept {
    return q * scalar;
}

[[nodiscard]] Result<void> check_compatible(const Dimension& a, const Dimension& b);
[[nodiscard]] Result<void> check_compatible(const Quantity& a, const Quantity& b);

// -------------------------------------------------------------
// Unit Conversion Definitions (UNIT-04)
// -------------------------------------------------------------
struct UnitDefinition {
    std::string symbol;
    std::string name;
    Dimension dimension;
    double scale_to_canonical = 1.0;
    double offset_to_canonical = 0.0; // canonical = (display * scale) + offset
};

class UnitRegistry {
public:
    static UnitRegistry& instance();

    UnitRegistry();

    void register_unit(UnitDefinition def);
    [[nodiscard]] const UnitDefinition* find_unit(std::string_view symbol) const noexcept;
    [[nodiscard]] bool has_unit(std::string_view symbol) const noexcept;

    [[nodiscard]] Result<Quantity> to_canonical(double display_val, std::string_view unit_symbol) const;
    [[nodiscard]] Result<double> from_canonical(const Quantity& q, std::string_view target_unit_symbol) const;

    // Unit-aware formatting (UNIT-06)
    [[nodiscard]] std::string format(const Quantity& q, int precision = 4, bool auto_prefix = false) const;
    [[nodiscard]] std::string suggest_best_unit(const Dimension& dim, double value) const;

private:
    void populate_standard_units();
    std::unordered_map<std::string, UnitDefinition> units_;
};

// -------------------------------------------------------------
// Vector and Tensor Quantities (UNIT-05)
// -------------------------------------------------------------
class VectorQuantity {
public:
    math::Vec3 value{0.0, 0.0, 0.0};
    Dimension dimension = Dimensionless;

    constexpr VectorQuantity() noexcept = default;
    constexpr VectorQuantity(math::Vec3 val, Dimension dim) noexcept : value(val), dimension(dim) {}

    [[nodiscard]] Result<VectorQuantity> add(const VectorQuantity& rhs) const {
        if (dimension != rhs.dimension) {
            return Result<VectorQuantity>::err({
                .code = 3004,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Dimensional mismatch in vector addition: " + dimension.symbol() + " vs " + rhs.dimension.symbol(),
                .details = ""
            });
        }
        return VectorQuantity(value + rhs.value, dimension);
    }

    [[nodiscard]] Result<VectorQuantity> sub(const VectorQuantity& rhs) const {
        if (dimension != rhs.dimension) {
            return Result<VectorQuantity>::err({
                .code = 3005,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Dimensional mismatch in vector subtraction: " + dimension.symbol() + " vs " + rhs.dimension.symbol(),
                .details = ""
            });
        }
        return VectorQuantity(value - rhs.value, dimension);
    }

    [[nodiscard]] Quantity dot(const VectorQuantity& rhs) const noexcept {
        return Quantity(value.dot(rhs.value), dimension * rhs.dimension);
    }

    [[nodiscard]] VectorQuantity cross(const VectorQuantity& rhs) const noexcept {
        return VectorQuantity(value.cross(rhs.value), dimension * rhs.dimension);
    }

    [[nodiscard]] Quantity magnitude() const noexcept {
        return Quantity(value.length(), dimension);
    }

    [[nodiscard]] VectorQuantity operator*(double scalar) const noexcept {
        return VectorQuantity(value * scalar, dimension);
    }

    [[nodiscard]] VectorQuantity operator/(double scalar) const {
        if (scalar == 0.0) throw std::domain_error("VectorQuantity division by zero");
        return VectorQuantity(value / scalar, dimension);
    }
};

class TensorQuantity {
public:
    std::array<double, 9> data{
        0, 0, 0,
        0, 0, 0,
        0, 0, 0
    };
    Dimension dimension = Dimensionless;

    constexpr TensorQuantity() noexcept = default;
    constexpr TensorQuantity(std::array<double, 9> d, Dimension dim) noexcept : data(d), dimension(dim) {}

    [[nodiscard]] Quantity trace() const noexcept {
        return Quantity(data[0] + data[4] + data[8], dimension);
    }

    [[nodiscard]] Result<TensorQuantity> add(const TensorQuantity& rhs) const {
        if (dimension != rhs.dimension) {
            return Result<TensorQuantity>::err({
                .code = 3006,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Dimensional mismatch in tensor addition",
                .details = ""
            });
        }
        std::array<double, 9> res{};
        for (std::size_t i = 0; i < 9; ++i) res[i] = data[i] + rhs.data[i];
        return TensorQuantity(res, dimension);
    }

    [[nodiscard]] VectorQuantity transform(const math::Vec3& vec) const noexcept {
        math::Vec3 res{
            data[0] * vec.x + data[1] * vec.y + data[2] * vec.z,
            data[3] * vec.x + data[4] * vec.y + data[5] * vec.z,
            data[6] * vec.x + data[7] * vec.y + data[8] * vec.z
        };
        return VectorQuantity(res, dimension);
    }
};

// -------------------------------------------------------------
// Compile-Time NTTP Units and Quantities (IMP-01)
// -------------------------------------------------------------

template <Dimension Dim>
class QuantityT {
public:
    static constexpr Dimension dimension = Dim;

    constexpr explicit QuantityT(double val = 0.0) noexcept : value_(val) {}

    [[nodiscard]] constexpr double value() const noexcept { return value_; }

    constexpr QuantityT operator+() const noexcept { return *this; }
    constexpr QuantityT operator-() const noexcept { return QuantityT(-value_); }

    constexpr QuantityT& operator+=(const QuantityT& rhs) noexcept {
        value_ += rhs.value_;
        return *this;
    }

    constexpr QuantityT& operator-=(const QuantityT& rhs) noexcept {
        value_ -= rhs.value_;
        return *this;
    }

    constexpr QuantityT& operator*=(double scalar) noexcept {
        value_ *= scalar;
        return *this;
    }

    constexpr QuantityT& operator/=(double scalar) {
        if (scalar == 0.0) throw std::domain_error("Division by zero in QuantityT");
        value_ /= scalar;
        return *this;
    }

    constexpr std::partial_ordering operator<=>(const QuantityT& rhs) const noexcept {
        return value_ <=> rhs.value_;
    }

    constexpr bool operator==(const QuantityT& rhs) const noexcept {
        return value_ == rhs.value_;
    }

    [[nodiscard]] Quantity to_dynamic() const noexcept {
        return Quantity(value_, Dim);
    }

    [[nodiscard]] static Result<QuantityT> from_dynamic(const Quantity& q) noexcept {
        if (q.dimension != Dim) {
            return Result<QuantityT>::err({
                .code = 3007,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Dimensional mismatch converting dynamic Quantity to QuantityT",
                .details = {}
            });
        }
        return Result<QuantityT>::ok(QuantityT(q.value));
    }

private:
    double value_{0.0};
};

template <Dimension D>
constexpr QuantityT<D> operator+(QuantityT<D> lhs, QuantityT<D> rhs) noexcept {
    return QuantityT<D>(lhs.value() + rhs.value());
}

template <Dimension D>
constexpr QuantityT<D> operator-(QuantityT<D> lhs, QuantityT<D> rhs) noexcept {
    return QuantityT<D>(lhs.value() - rhs.value());
}

template <Dimension D1, Dimension D2>
constexpr QuantityT<D1 * D2> operator*(QuantityT<D1> lhs, QuantityT<D2> rhs) noexcept {
    return QuantityT<D1 * D2>(lhs.value() * rhs.value());
}

template <Dimension D1, Dimension D2>
constexpr QuantityT<D1 / D2> operator/(QuantityT<D1> lhs, QuantityT<D2> rhs) {
    if (rhs.value() == 0.0) throw std::domain_error("Division by zero in QuantityT");
    return QuantityT<D1 / D2>(lhs.value() / rhs.value());
}

template <Dimension D>
constexpr QuantityT<D> operator*(QuantityT<D> q, double scalar) noexcept {
    return QuantityT<D>(q.value() * scalar);
}

template <Dimension D>
constexpr QuantityT<D> operator*(double scalar, QuantityT<D> q) noexcept {
    return QuantityT<D>(q.value() * scalar);
}

template <Dimension D>
constexpr QuantityT<D> operator/(QuantityT<D> q, double scalar) {
    if (scalar == 0.0) throw std::domain_error("Division by zero in QuantityT");
    return QuantityT<D>(q.value() / scalar);
}

template <Dimension D>
constexpr QuantityT<Dimensionless / D> operator/(double scalar, QuantityT<D> q) {
    if (q.value() == 0.0) throw std::domain_error("Division by zero in QuantityT");
    return QuantityT<Dimensionless / D>(scalar / q.value());
}

// -------------------------------------------------------------
// Compile-Time NTTP Vector Quantities (IMP-01)
// -------------------------------------------------------------

template <Dimension Dim>
class VectorQuantityT {
public:
    static constexpr Dimension dimension = Dim;

    constexpr explicit VectorQuantityT(math::Vec3 val = {}) noexcept : value_(val) {}
    constexpr VectorQuantityT(double x, double y, double z) noexcept : value_(x, y, z) {}

    [[nodiscard]] constexpr math::Vec3 value() const noexcept { return value_; }
    [[nodiscard]] constexpr double x() const noexcept { return value_.x; }
    [[nodiscard]] constexpr double y() const noexcept { return value_.y; }
    [[nodiscard]] constexpr double z() const noexcept { return value_.z; }

    [[nodiscard]] QuantityT<Dim> magnitude() const noexcept {
        return QuantityT<Dim>(value_.length());
    }

    constexpr VectorQuantityT operator+() const noexcept { return *this; }
    constexpr VectorQuantityT operator-() const noexcept { return VectorQuantityT(-value_); }

    constexpr VectorQuantityT& operator+=(const VectorQuantityT& rhs) noexcept {
        value_ += rhs.value_;
        return *this;
    }

    constexpr VectorQuantityT& operator-=(const VectorQuantityT& rhs) noexcept {
        value_ -= rhs.value_;
        return *this;
    }

    [[nodiscard]] VectorQuantity to_dynamic() const noexcept {
        return VectorQuantity(value_, Dim);
    }

    [[nodiscard]] static Result<VectorQuantityT> from_dynamic(const VectorQuantity& v) noexcept {
        if (v.dimension != Dim) {
            return Result<VectorQuantityT>::err({
                .code = 3008,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Dimensional mismatch converting dynamic VectorQuantity to VectorQuantityT",
                .details = {}
            });
        }
        return Result<VectorQuantityT>::ok(VectorQuantityT(v.value));
    }

private:
    math::Vec3 value_{};
};

template <Dimension D>
constexpr VectorQuantityT<D> operator+(VectorQuantityT<D> lhs, VectorQuantityT<D> rhs) noexcept {
    return VectorQuantityT<D>(lhs.value() + rhs.value());
}

template <Dimension D>
constexpr VectorQuantityT<D> operator-(VectorQuantityT<D> lhs, VectorQuantityT<D> rhs) noexcept {
    return VectorQuantityT<D>(lhs.value() - rhs.value());
}

template <Dimension D>
constexpr VectorQuantityT<D> operator*(VectorQuantityT<D> vec, double scalar) noexcept {
    return VectorQuantityT<D>(vec.value() * scalar);
}

template <Dimension D>
constexpr VectorQuantityT<D> operator*(double scalar, VectorQuantityT<D> vec) noexcept {
    return VectorQuantityT<D>(vec.value() * scalar);
}

template <Dimension D1, Dimension D2>
constexpr VectorQuantityT<D1 * D2> operator*(VectorQuantityT<D1> vec, QuantityT<D2> q) noexcept {
    return VectorQuantityT<D1 * D2>(vec.value() * q.value());
}

template <Dimension D1, Dimension D2>
constexpr VectorQuantityT<D1 * D2> operator*(QuantityT<D1> q, VectorQuantityT<D2> vec) noexcept {
    return VectorQuantityT<D1 * D2>(vec.value() * q.value());
}

template <Dimension D1, Dimension D2>
constexpr VectorQuantityT<D1 / D2> operator/(VectorQuantityT<D1> vec, QuantityT<D2> q) {
    if (q.value() == 0.0) throw std::domain_error("Division by zero in VectorQuantityT");
    return VectorQuantityT<D1 / D2>(vec.value() / q.value());
}

template <Dimension D1, Dimension D2>
constexpr QuantityT<D1 * D2> dot(VectorQuantityT<D1> a, VectorQuantityT<D2> b) noexcept {
    return QuantityT<D1 * D2>(a.value().dot(b.value()));
}

template <Dimension D1, Dimension D2>
constexpr VectorQuantityT<D1 * D2> cross(VectorQuantityT<D1> a, VectorQuantityT<D2> b) noexcept {
    return VectorQuantityT<D1 * D2>(a.value().cross(b.value()));
}

// -------------------------------------------------------------
// Typed Dimensions and Aliases (IMP-01)
// -------------------------------------------------------------

using DimensionlessT = QuantityT<Dimensionless>;
using LengthT = QuantityT<Length>;
using MassT = QuantityT<Mass>;
using TimeT = QuantityT<Time>;
using ElectricCurrentT = QuantityT<ElectricCurrent>;
using TemperatureT = QuantityT<Temperature>;
using AmountOfSubstanceT = QuantityT<AmountOfSubstance>;
using LuminousIntensityT = QuantityT<LuminousIntensity>;

using FrequencyT = QuantityT<Frequency>;
using AreaT = QuantityT<Area>;
using VolumeT = QuantityT<Volume>;
using VelocityT = QuantityT<Velocity>;
using AccelerationT = QuantityT<Acceleration>;
using ForceT = QuantityT<Force>;
using PressureT = QuantityT<Pressure>;
using EnergyT = QuantityT<Energy>;
using PowerT = QuantityT<Power>;
using VoltageT = QuantityT<Voltage>;
using ResistanceT = QuantityT<Resistance>;
using CapacitanceT = QuantityT<Capacitance>;
using InductanceT = QuantityT<Inductance>;

using Position3DT = VectorQuantityT<Length>;
using Velocity3DT = VectorQuantityT<Velocity>;
using Acceleration3DT = VectorQuantityT<Acceleration>;
using Force3DT = VectorQuantityT<Force>;

// -------------------------------------------------------------
// User-Defined Literals (IMP-01)
// -------------------------------------------------------------

inline namespace literals {
    constexpr LengthT operator""_m(long double val) { return LengthT(static_cast<double>(val)); }
    constexpr LengthT operator""_m(unsigned long long val) { return LengthT(static_cast<double>(val)); }
    constexpr LengthT operator""_cm(long double val) { return LengthT(static_cast<double>(val) * 1e-2); }
    constexpr LengthT operator""_mm(long double val) { return LengthT(static_cast<double>(val) * 1e-3); }

    constexpr MassT operator""_kg(long double val) { return MassT(static_cast<double>(val)); }
    constexpr MassT operator""_kg(unsigned long long val) { return MassT(static_cast<double>(val)); }
    constexpr MassT operator""_g(long double val) { return MassT(static_cast<double>(val) * 1e-3); }

    constexpr TimeT operator""_s(long double val) { return TimeT(static_cast<double>(val)); }
    constexpr TimeT operator""_s(unsigned long long val) { return TimeT(static_cast<double>(val)); }
    constexpr TimeT operator""_ms(long double val) { return TimeT(static_cast<double>(val) * 1e-3); }

    constexpr ForceT operator""_N(long double val) { return ForceT(static_cast<double>(val)); }
    constexpr ForceT operator""_N(unsigned long long val) { return ForceT(static_cast<double>(val)); }

    constexpr EnergyT operator""_J(long double val) { return EnergyT(static_cast<double>(val)); }
    constexpr EnergyT operator""_J(unsigned long long val) { return EnergyT(static_cast<double>(val)); }

    constexpr PowerT operator""_W(long double val) { return PowerT(static_cast<double>(val)); }
    constexpr PowerT operator""_W(unsigned long long val) { return PowerT(static_cast<double>(val)); }

    constexpr VoltageT operator""_V(long double val) { return VoltageT(static_cast<double>(val)); }
    constexpr VoltageT operator""_V(unsigned long long val) { return VoltageT(static_cast<double>(val)); }

    constexpr ElectricCurrentT operator""_A(long double val) { return ElectricCurrentT(static_cast<double>(val)); }
    constexpr ElectricCurrentT operator""_A(unsigned long long val) { return ElectricCurrentT(static_cast<double>(val)); }

    constexpr ResistanceT operator""_ohm(long double val) { return ResistanceT(static_cast<double>(val)); }
    constexpr ResistanceT operator""_ohm(unsigned long long val) { return ResistanceT(static_cast<double>(val)); }
}



} // namespace shunya::core::base::units
