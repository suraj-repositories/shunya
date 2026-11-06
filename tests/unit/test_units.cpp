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
    Dimension v_dim = Length / Time;
    CHECK(v_dim == Velocity);
    CHECK(v_dim.symbol() == "m/s");

    Dimension a_dim = Velocity / Time;
    CHECK(a_dim == Acceleration);
    CHECK(a_dim.symbol() == "m/s^2");

    Dimension f_dim = Mass * Acceleration;
    CHECK(f_dim == Force);
    CHECK(f_dim.symbol() == "N");

    Dimension e_dim = Force * Length;
    CHECK(e_dim == Energy);
    CHECK(e_dim.symbol() == "J");

    Dimension p_dim = Energy / Time;
    CHECK(p_dim == Power);
    CHECK(p_dim.symbol() == "W");

    Dimension volt_dim = Power / ElectricCurrent;
    CHECK(volt_dim == Voltage);
    CHECK(volt_dim.symbol() == "V");

    Dimension r_dim = Voltage / ElectricCurrent;
    CHECK(r_dim == ElectricalResistance);
    CHECK(r_dim.symbol() == "Ohm");

    Dimension c_dim = ElectricCharge / Voltage;
    CHECK(c_dim == ElectricalCapacitance);
    CHECK(c_dim.symbol() == "F");
}

TEST_CASE("UNIT-03: Quantity Arithmetic and Dimensional Compatibility") {
    Quantity mass(2.5, Mass);
    Quantity accel(9.8, Acceleration);

    Quantity force = mass.mul(accel);
    CHECK(force.value == 24.5);
    CHECK(force.dimension == Force);
    CHECK(force.to_string() == "24.5 N");

    Quantity f2(10.5, Force);
    auto sum_res = force.add(f2);
    REQUIRE(sum_res.is_ok());
    CHECK(sum_res.value().value == 35.0);
    CHECK(sum_res.value().dimension == Force);

    Quantity vel(5.0, Velocity);
    auto invalid_add = force.add(vel);
    CHECK(!invalid_add.is_ok());
    CHECK(invalid_add.error().code == 3001);

    Quantity v(12.0, Voltage);
    Quantity i(2.0, ElectricCurrent);
    auto invalid_sub = v.sub(i);
    CHECK(!invalid_sub.is_ok());
    CHECK(invalid_sub.error().code == 3002);

    auto compat_ok = check_compatible(force, f2);
    CHECK(compat_ok.is_ok());

    auto compat_err = check_compatible(v, i);
    CHECK(!compat_err.is_ok());
    CHECK(compat_err.error().code == 3003);
}

TEST_CASE("UNIT-04: Unit Conversion to and from Canonical Storage") {
    auto& reg = UnitRegistry::instance();

    // 1. Length: 100 mm to canonical m (0.1 m)
    auto len_res = reg.to_canonical(100.0, "mm");
    REQUIRE(len_res.is_ok());
    CHECK(len_res.value().value == 0.1);
    CHECK(len_res.value().dimension == Length);

    // 0.1 m back to mm
    auto mm_res = reg.from_canonical(len_res.value(), "mm");
    REQUIRE(mm_res.is_ok());
    CHECK(mm_res.value() == 100.0);

    // 2. Imperial Length: 2 inches to m
    auto inch_res = reg.to_canonical(2.0, "in");
    REQUIRE(inch_res.is_ok());
    CHECK(inch_res.value().value == 0.0508);

    // 3. Pressure: 1 atmosphere to Pa
    auto atm_res = reg.to_canonical(1.0, "atm");
    REQUIRE(atm_res.is_ok());
    CHECK(atm_res.value().value == 101325.0);

    // 4. Temperature with offset: 100 degC to 373.15 K
    auto c_res = reg.to_canonical(100.0, "degC");
    REQUIRE(c_res.is_ok());
    CHECK(c_res.value().value == 373.15);

    auto degc_back = reg.from_canonical(c_res.value(), "degC");
    REQUIRE(degc_back.is_ok());
    CHECK(std::abs(degc_back.value() - 100.0) < 1e-9);

    // 5. Fahrenheit: 32 degF to 273.15 K
    auto f_res = reg.to_canonical(32.0, "degF");
    REQUIRE(f_res.is_ok());
    CHECK(std::abs(f_res.value().value - 273.15) < 1e-9);
}

TEST_CASE("UNIT-05: Vector and Tensor Quantities") {
    using namespace shunya::core::base::math;

    // Vector Force = (10, 20, 30) N
    VectorQuantity f1(Vec3{10.0, 20.0, 30.0}, Force);
    VectorQuantity f2(Vec3{5.0, 10.0, 15.0}, Force);

    auto f_sum = f1.add(f2);
    REQUIRE(f_sum.is_ok());
    CHECK(f_sum.value().value.x == 15.0);
    CHECK(f_sum.value().value.y == 30.0);
    CHECK(f_sum.value().value.z == 45.0);
    CHECK(f_sum.value().dimension == Force);

    // Vector dot product
    VectorQuantity disp(Vec3{2.0, 0.0, 0.0}, Length);
    Quantity work = f1.dot(disp);
    CHECK(work.value == 20.0);
    CHECK(work.dimension == Energy);

    // Tensor Quantity (e.g. stress tensor)
    std::array<double, 9> stress_data{
        100, 0, 0,
        0, 200, 0,
        0, 0, 300
    };
    TensorQuantity stress(stress_data, Pressure);
    Quantity tr = stress.trace();
    CHECK(tr.value == 600.0);
    CHECK(tr.dimension == Pressure);

    // Tensor transforms vector: (1, 1, 1) -> (100, 200, 300)
    VectorQuantity transformed = stress.transform(Vec3{1, 1, 1});
    CHECK(transformed.value.x == 100.0);
    CHECK(transformed.value.y == 200.0);
    CHECK(transformed.value.z == 300.0);
    CHECK(transformed.dimension == Pressure);
}

TEST_CASE("UNIT-06: Unit-Aware UI Formatting and Suggestion") {
    auto& reg = UnitRegistry::instance();

    Quantity v1(0.005, Voltage); // 5 mV
    std::string suggested = reg.suggest_best_unit(v1.dimension, v1.value);
    CHECK(suggested == "mV");

    std::string formatted = reg.format(v1, 1, true);
    CHECK(formatted == "5.0 mV");

    Quantity freq(10.5e6, Frequency); // 10.5 MHz
    CHECK(reg.suggest_best_unit(freq.dimension, freq.value) == "MHz");
    CHECK(reg.format(freq, 2, true) == "10.50 MHz");
}

TEST_CASE("UNIT-07: Validation Suite: Round-trips and Error Rejection") {
    auto& reg = UnitRegistry::instance();

    // Incompatible unit conversion rejection: cannot convert Voltage to kg
    Quantity v(12.0, Voltage);
    auto bad_conv = reg.from_canonical(v, "kg");
    CHECK(!bad_conv.is_ok());
    CHECK(bad_conv.error().code == 3012);

    // Unknown unit symbol rejection
    auto unknown_unit = reg.to_canonical(10.0, "nonexistent_unit_xyz");
    CHECK(!unknown_unit.is_ok());
    CHECK(unknown_unit.error().code == 3010);

    // Exact Round-trip tolerance verification across pressure units
    double test_psi = 14.696;
    auto can_psi = reg.to_canonical(test_psi, "psi");
    REQUIRE(can_psi.is_ok());
    auto back_psi = reg.from_canonical(can_psi.value(), "psi");
    REQUIRE(back_psi.is_ok());
    CHECK(std::abs(back_psi.value() - test_psi) < 1e-9);
}
