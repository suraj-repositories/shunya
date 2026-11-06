#include "shunya/core/base/units.hpp"

#include <iomanip>
#include <cmath>

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

// -------------------------------------------------------------
// UnitRegistry Implementation
// -------------------------------------------------------------
UnitRegistry& UnitRegistry::instance() {
    static UnitRegistry inst;
    return inst;
}

UnitRegistry::UnitRegistry() {
    populate_standard_units();
}

void UnitRegistry::register_unit(UnitDefinition def) {
    units_[def.symbol] = std::move(def);
}

const UnitDefinition* UnitRegistry::find_unit(std::string_view symbol) const noexcept {
    auto it = units_.find(std::string(symbol));
    if (it != units_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool UnitRegistry::has_unit(std::string_view symbol) const noexcept {
    return units_.find(std::string(symbol)) != units_.end();
}

void UnitRegistry::populate_standard_units() {
    // Base SI units
    register_unit({"m", "metre", Length, 1.0, 0.0});
    register_unit({"kg", "kilogram", Mass, 1.0, 0.0});
    register_unit({"s", "second", Time, 1.0, 0.0});
    register_unit({"A", "ampere", ElectricCurrent, 1.0, 0.0});
    register_unit({"K", "kelvin", Temperature, 1.0, 0.0});
    register_unit({"mol", "mole", AmountOfSubstance, 1.0, 0.0});
    register_unit({"cd", "candela", LuminousIntensity, 1.0, 0.0});

    // Metric length
    register_unit({"mm", "millimetre", Length, 1e-3, 0.0});
    register_unit({"cm", "centimetre", Length, 1e-2, 0.0});
    register_unit({"km", "kilometre", Length, 1e3, 0.0});
    register_unit({"um", "micrometre", Length, 1e-6, 0.0});
    register_unit({"nm", "nanometre", Length, 1e-9, 0.0});

    // Imperial length
    register_unit({"in", "inch", Length, 0.0254, 0.0});
    register_unit({"ft", "foot", Length, 0.3048, 0.0});
    register_unit({"yd", "yard", Length, 0.9144, 0.0});
    register_unit({"mi", "mile", Length, 1609.344, 0.0});

    // Mass
    register_unit({"g", "gram", Mass, 1e-3, 0.0});
    register_unit({"mg", "milligram", Mass, 1e-6, 0.0});
    register_unit({"tonne", "tonne", Mass, 1e3, 0.0});
    register_unit({"lb", "pound", Mass, 0.45359237, 0.0});
    register_unit({"oz", "ounce", Mass, 0.028349523125, 0.0});

    // Time
    register_unit({"ms", "millisecond", Time, 1e-3, 0.0});
    register_unit({"us", "microsecond", Time, 1e-6, 0.0});
    register_unit({"ns", "nanosecond", Time, 1e-9, 0.0});
    register_unit({"min", "minute", Time, 60.0, 0.0});
    register_unit({"h", "hour", Time, 3600.0, 0.0});
    register_unit({"day", "day", Time, 86400.0, 0.0});

    // Mechanical
    register_unit({"N", "newton", Force, 1.0, 0.0});
    register_unit({"kN", "kilonewton", Force, 1e3, 0.0});
    register_unit({"lbf", "pound-force", Force, 4.4482216152605, 0.0});
    register_unit({"Pa", "pascal", Pressure, 1.0, 0.0});
    register_unit({"kPa", "kilopascal", Pressure, 1e3, 0.0});
    register_unit({"MPa", "megapascal", Pressure, 1e6, 0.0});
    register_unit({"GPa", "gigapascal", Pressure, 1e9, 0.0});
    register_unit({"bar", "bar", Pressure, 1e5, 0.0});
    register_unit({"atm", "atmosphere", Pressure, 101325.0, 0.0});
    register_unit({"psi", "pounds per square inch", Pressure, 6894.757293168, 0.0});
    register_unit({"J", "joule", Energy, 1.0, 0.0});
    register_unit({"kJ", "kilojoule", Energy, 1e3, 0.0});
    register_unit({"MJ", "megajoule", Energy, 1e6, 0.0});
    register_unit({"cal", "calorie", Energy, 4.184, 0.0});
    register_unit({"kcal", "kilocalorie", Energy, 4184.0, 0.0});
    register_unit({"eV", "electronvolt", Energy, 1.602176634e-19, 0.0});
    register_unit({"kWh", "kilowatt-hour", Energy, 3.6e6, 0.0});
    register_unit({"W", "watt", Power, 1.0, 0.0});
    register_unit({"kW", "kilowatt", Power, 1e3, 0.0});
    register_unit({"MW", "megawatt", Power, 1e6, 0.0});
    register_unit({"hp", "horsepower", Power, 745.69987158227022, 0.0});

    // Electrical
    register_unit({"V", "volt", Voltage, 1.0, 0.0});
    register_unit({"mV", "millivolt", Voltage, 1e-3, 0.0});
    register_unit({"kV", "kilovolt", Voltage, 1e3, 0.0});
    register_unit({"uV", "microvolt", Voltage, 1e-6, 0.0});
    register_unit({"mA", "milliampere", ElectricCurrent, 1e-3, 0.0});
    register_unit({"uA", "microampere", ElectricCurrent, 1e-6, 0.0});
    register_unit({"Ohm", "ohm", ElectricalResistance, 1.0, 0.0});
    register_unit({"kOhm", "kiloohm", ElectricalResistance, 1e3, 0.0});
    register_unit({"MOhm", "megaohm", ElectricalResistance, 1e6, 0.0});
    register_unit({"mOhm", "milliohm", ElectricalResistance, 1e-3, 0.0});
    register_unit({"F", "farad", ElectricalCapacitance, 1.0, 0.0});
    register_unit({"mF", "millifarad", ElectricalCapacitance, 1e-3, 0.0});
    register_unit({"uF", "microfarad", ElectricalCapacitance, 1e-6, 0.0});
    register_unit({"nF", "nanofarad", ElectricalCapacitance, 1e-9, 0.0});
    register_unit({"pF", "picofarad", ElectricalCapacitance, 1e-12, 0.0});
    register_unit({"H", "henry", Inductance, 1.0, 0.0});
    register_unit({"mH", "millihenry", Inductance, 1e-3, 0.0});
    register_unit({"uH", "microhenry", Inductance, 1e-6, 0.0});
    register_unit({"Hz", "hertz", Frequency, 1.0, 0.0});
    register_unit({"kHz", "kilohertz", Frequency, 1e3, 0.0});
    register_unit({"MHz", "megahertz", Frequency, 1e6, 0.0});
    register_unit({"GHz", "gigahertz", Frequency, 1e9, 0.0});

    // Temperature (affine transforms)
    register_unit({"degC", "degree Celsius", Temperature, 1.0, 273.15});
    register_unit({"degF", "degree Fahrenheit", Temperature, 5.0 / 9.0, 273.15 - (32.0 * 5.0 / 9.0)});
}

Result<Quantity> UnitRegistry::to_canonical(double display_val, std::string_view unit_symbol) const {
    const auto* def = find_unit(unit_symbol);
    if (!def) {
        return Result<Quantity>::err({
            .code = 3010,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Unknown unit symbol: " + std::string(unit_symbol),
            .details = ""
        });
    }

    double canonical_val = (display_val * def->scale_to_canonical) + def->offset_to_canonical;
    return Quantity(canonical_val, def->dimension);
}

Result<double> UnitRegistry::from_canonical(const Quantity& q, std::string_view target_unit_symbol) const {
    const auto* def = find_unit(target_unit_symbol);
    if (!def) {
        return Result<double>::err({
            .code = 3011,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Unknown unit symbol: " + std::string(target_unit_symbol),
            .details = ""
        });
    }

    if (q.dimension != def->dimension) {
        return Result<double>::err({
            .code = 3012,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Cannot convert " + q.dimension.symbol() + " to " + std::string(target_unit_symbol),
            .details = ""
        });
    }

    double display_val = (q.value - def->offset_to_canonical) / def->scale_to_canonical;
    return display_val;
}

std::string UnitRegistry::suggest_best_unit(const Dimension& dim, double value) const {
    double abs_v = std::abs(value);
    if (dim == Voltage) {
        if (abs_v >= 1e3) return "kV";
        if (abs_v >= 1.0) return "V";
        if (abs_v >= 1e-3) return "mV";
        return "uV";
    }
    if (dim == ElectricalResistance) {
        if (abs_v >= 1e6) return "MOhm";
        if (abs_v >= 1e3) return "kOhm";
        if (abs_v >= 1.0) return "Ohm";
        return "mOhm";
    }
    if (dim == ElectricalCapacitance) {
        if (abs_v >= 1e-3) return "mF";
        if (abs_v >= 1e-6) return "uF";
        if (abs_v >= 1e-9) return "nF";
        return "pF";
    }
    if (dim == Frequency) {
        if (abs_v >= 1e9) return "GHz";
        if (abs_v >= 1e6) return "MHz";
        if (abs_v >= 1e3) return "kHz";
        return "Hz";
    }
    if (dim == Length) {
        if (abs_v >= 1e3) return "km";
        if (abs_v >= 1.0) return "m";
        if (abs_v >= 1e-2) return "cm";
        if (abs_v >= 1e-3) return "mm";
        if (abs_v >= 1e-6) return "um";
        return "nm";
    }
    return dim.symbol();
}

std::string UnitRegistry::format(const Quantity& q, int precision, bool auto_prefix) const {
    std::string unit = auto_prefix ? suggest_best_unit(q.dimension, q.value) : q.dimension.symbol();
    double disp_val = q.value;

    if (auto_prefix && unit != q.dimension.symbol()) {
        auto res = from_canonical(q, unit);
        if (res.is_ok()) {
            disp_val = res.value();
        }
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << disp_val << " " << unit;
    return ss.str();
}

} // namespace shunya::core::base::units
