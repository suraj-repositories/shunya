#include "tests/test_framework.hpp"
#include "shunya/core/base/units.hpp"

using namespace shunya::core::base::units;

TEST_CASE("UNIT-01: SI Base Units and Representation") {
    CHECK(Length.length == 1);
    CHECK(Length.symbol() == "m");

    CHECK(Mass.mass == 1);
    CHECK(Mass.symbol() == "kg");

    CHECK(Time.time == 1);
    CHECK(Time.symbol() == "s");

    CHECK(ElectricCurrent.current == 1);
    CHECK(ElectricCurrent.symbol() == "A");

    CHECK(Temperature.temperature == 1);
    CHECK(Temperature.symbol() == "K");

    CHECK(AmountOfSubstance.amount == 1);
    CHECK(AmountOfSubstance.symbol() == "mol");

    CHECK(LuminousIntensity.luminosity == 1);
    CHECK(LuminousIntensity.symbol() == "cd");

    CHECK(Dimensionless.is_dimensionless());
    CHECK(!Length.is_dimensionless());
}

TEST_CASE("UNIT-02: Derived Units and Dimensional Multiplication/Division") {
    // Velocity = Length / Time
    Dimension v_dim = Length / Time;
    CHECK(v_dim == Velocity);
    CHECK(v_dim.symbol() == "m/s");

    // Acceleration = Velocity / Time
    Dimension a_dim = Velocity / Time;
    CHECK(a_dim == Acceleration);
    CHECK(a_dim.symbol() == "m/s^2");

    // Force = Mass * Acceleration (Newton)
    Dimension f_dim = Mass * Acceleration;
    CHECK(f_dim == Force);
    CHECK(f_dim.symbol() == "N");

    // Energy = Force * Length (Joule)
    Dimension e_dim = Force * Length;
    CHECK(e_dim == Energy);
    CHECK(e_dim.symbol() == "J");

    // Power = Energy / Time (Watt)
    Dimension p_dim = Energy / Time;
    CHECK(p_dim == Power);
    CHECK(p_dim.symbol() == "W");

    // Voltage = Power / Current (Volt)
    Dimension volt_dim = Power / ElectricCurrent;
    CHECK(volt_dim == Voltage);
    CHECK(volt_dim.symbol() == "V");

    // Resistance = Voltage / Current (Ohm)
    Dimension r_dim = Voltage / ElectricCurrent;
    CHECK(r_dim == ElectricalResistance);
    CHECK(r_dim.symbol() == "Ohm");

    // Capacitance = (Current * Time) / Voltage (Farad)
    Dimension c_dim = ElectricCharge / Voltage;
    CHECK(c_dim == ElectricalCapacitance);
    CHECK(c_dim.symbol() == "F");
}

TEST_CASE("UNIT-03: Quantity Arithmetic and Dimensional Compatibility") {
    Quantity mass(2.5, Mass);               // 2.5 kg
    Quantity accel(9.8, Acceleration);     // 9.8 m/s^2

    // Multiplication creates new derived quantity (Force = 24.5 N)
    Quantity force = mass.mul(accel);
    CHECK(force.value == 24.5);
    CHECK(force.dimension == Force);
    CHECK(force.to_string() == "24.5 N");

    // Valid addition of matching dimensions
    Quantity f2(10.5, Force);
    auto sum_res = force.add(f2);
    REQUIRE(sum_res.is_ok());
    CHECK(sum_res.value().value == 35.0);
    CHECK(sum_res.value().dimension == Force);

    // Invalid addition of mismatched dimensions (e.g. Force + Velocity)
    Quantity vel(5.0, Velocity);
    auto invalid_add = force.add(vel);
    CHECK(!invalid_add.is_ok());
    CHECK(invalid_add.error().code == 3001);
    CHECK(invalid_add.error().message.find("Dimensional mismatch") != std::string::npos);

    // Invalid subtraction of mismatched dimensions (e.g. Voltage - Current)
    Quantity v(12.0, Voltage);
    Quantity i(2.0, ElectricCurrent);
    auto invalid_sub = v.sub(i);
    CHECK(!invalid_sub.is_ok());
    CHECK(invalid_sub.error().code == 3002);

    // Compatibility check API
    auto compat_ok = check_compatible(force, f2);
    CHECK(compat_ok.is_ok());

    auto compat_err = check_compatible(v, i);
    CHECK(!compat_err.is_ok());
    CHECK(compat_err.error().code == 3003);
}
