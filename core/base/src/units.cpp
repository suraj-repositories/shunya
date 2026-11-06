#include "shunya/core/base/units.hpp"

namespace shunya::core::base::units {

std::string Dimension::symbol() const {
    if (is_dimensionless()) return "1";
    if (*this == Length) return "m";
    if (*this == Mass) return "kg";
    if (*this == Time) return "s";
    if (*this == ElectricCurrent) return "A";
    if (*this == Temperature) return "K";
    if (*this == AmountOfSubstance) return "mol";
    if (*this == LuminousIntensity) return "cd";

    if (*this == Frequency) return "Hz";
    if (*this == Force) return "N";
    if (*this == Pressure) return "Pa";
    if (*this == Energy) return "J";
    if (*this == Power) return "W";
    if (*this == ElectricCharge) return "C";
    if (*this == Voltage) return "V";
    if (*this == ElectricalResistance) return "Ohm";
    if (*this == ElectricalCapacitance) return "F";
    if (*this == ElectricalConductance) return "S";
    if (*this == MagneticFlux) return "Wb";
    if (*this == MagneticField) return "T";
    if (*this == Inductance) return "H";
    if (*this == Area) return "m^2";
    if (*this == Volume) return "m^3";
    if (*this == Velocity) return "m/s";
    if (*this == Acceleration) return "m/s^2";

    // Generic SI representation: e.g. m^1*kg^1*s^-2
    std::ostringstream ss;
    bool first = true;

    auto append_unit = [&](std::string_view name, int8_t exp) {
        if (exp == 0) return;
        if (!first) ss << "*";
        ss << name;
        if (exp != 1) ss << "^" << static_cast<int>(exp);
        first = false;
    };

    append_unit("m", length);
    append_unit("kg", mass);
    append_unit("s", time);
    append_unit("A", current);
    append_unit("K", temperature);
    append_unit("mol", amount);
    append_unit("cd", luminosity);

    return ss.str();
}

std::string Quantity::to_string() const {
    std::ostringstream ss;
    ss << value;
    std::string s = dimension.symbol();
    if (s != "1") {
        ss << " " << s;
    }
    return ss.str();
}

Result<void> check_compatible(const Dimension& a, const Dimension& b) {
    if (a != b) {
        return Result<void>::err({
            .code = 3003,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Incompatible physical dimensions: " + a.symbol() + " vs " + b.symbol(),
            .details = ""
        });
    }
    return Result<void>::ok();
}

Result<void> check_compatible(const Quantity& a, const Quantity& b) {
    return check_compatible(a.dimension, b.dimension);
}

} // namespace shunya::core::base::units
