#include "tests/test_framework.hpp"

#include "shunya/core/base/units.hpp"
#include "shunya/core/base/expression_evaluator.hpp"
#include "shunya/core/domain/parameter_reference_manager.hpp"
#include "shunya/core/domain/property_node.hpp"
#include "shunya/core/domain/element_definition.hpp"
#include "shunya/core/domain/element_instance.hpp"

#include <chrono>
#include <cmath>
#include <algorithm>

using namespace shunya::core;

namespace {
bool is_approx(double a, double b, double eps = 1e-3) {
    return std::abs(a - b) <= eps;
}
} // namespace

// =============================================================================
// Task UI-02-001-02: Inline Dimensional Unit Conversions & Parsing
// =============================================================================
TEST_CASE("UI-02-001-02: Quantity String Parsing and SI Canonical Normalization") {
    // 1. Length conversions (mm, cm, km, in, ft)
    {
        auto res_mm = base::units::parse_quantity("25 mm");
        REQUIRE(res_mm.is_ok());
        CHECK(res_mm.value().dimension == base::units::Length);
        CHECK(is_approx(res_mm.value().value, 0.025));

        auto res_cm = base::units::parse_quantity("10 cm");
        REQUIRE(res_cm.is_ok());
        CHECK(is_approx(res_cm.value().value, 0.10));

        auto res_km = base::units::parse_quantity("2.5 km");
        REQUIRE(res_km.is_ok());
        CHECK(is_approx(res_km.value().value, 2500.0));

        auto res_in = base::units::parse_quantity("2 in");
        REQUIRE(res_in.is_ok());
        CHECK(is_approx(res_in.value().value, 0.0508));
    }

    // 2. Electrical conversions (Ohm, kOhm, MOhm, V, mV, mA, uF)
    {
        auto res_kohm = base::units::parse_quantity("3.5 kOhm");
        REQUIRE(res_kohm.is_ok());
        CHECK(res_kohm.value().dimension == base::units::ElectricalResistance);
        CHECK(is_approx(res_kohm.value().value, 3500.0));

        auto res_mohm = base::units::parse_quantity("2.2 MOhm");
        REQUIRE(res_mohm.is_ok());
        CHECK(is_approx(res_mohm.value().value, 2.2e6));

        auto res_mv = base::units::parse_quantity("150 mV");
        REQUIRE(res_mv.is_ok());
        CHECK(res_mv.value().dimension == base::units::Voltage);
        CHECK(is_approx(res_mv.value().value, 0.150));

        auto res_uf = base::units::parse_quantity("10 uF");
        REQUIRE(res_uf.is_ok());
        CHECK(res_uf.value().dimension == base::units::ElectricalCapacitance);
        CHECK(is_approx(res_uf.value().value, 10e-6, 1e-8));
    }

    // 3. Pressure conversions (Pa, kPa, MPa, psi, bar, atm)
    {
        auto res_kpa = base::units::parse_quantity("150 kPa");
        REQUIRE(res_kpa.is_ok());
        CHECK(res_kpa.value().dimension == base::units::Pressure);
        CHECK(is_approx(res_kpa.value().value, 150000.0));

        auto res_psi = base::units::parse_quantity("100 psi");
        REQUIRE(res_psi.is_ok());
        CHECK(is_approx(res_psi.value().value, 689475.729, 1.0));

        auto res_bar = base::units::parse_quantity("2.5 bar");
        REQUIRE(res_bar.is_ok());
        CHECK(is_approx(res_bar.value().value, 250000.0));
    }

    // 4. Temperature affine conversions (degC, degF, K)
    {
        auto res_c = base::units::parse_quantity("450 degC");
        REQUIRE(res_c.is_ok());
        CHECK(res_c.value().dimension == base::units::Temperature);
        CHECK(is_approx(res_c.value().value, 450.0 + 273.15));

        auto res_neg_c = base::units::parse_quantity("-40 degC");
        REQUIRE(res_neg_c.is_ok());
        CHECK(is_approx(res_neg_c.value().value, -40.0 + 273.15));

        auto res_f = base::units::parse_quantity("32 degF");
        REQUIRE(res_f.is_ok());
        CHECK(is_approx(res_f.value().value, 273.15));
    }

    // 5. Kinematics and compound dimensions (m/s, km/h, m/s^2)
    {
        auto res_speed = base::units::parse_quantity("36 km/h");
        REQUIRE(res_speed.is_ok());
        CHECK(res_speed.value().dimension == base::units::Velocity);
        CHECK(is_approx(res_speed.value().value, 10.0));

        auto res_accel = base::units::parse_quantity("9.81 m/s^2");
        REQUIRE(res_accel.is_ok());
        CHECK(res_accel.value().dimension == base::units::Acceleration);
        CHECK(is_approx(res_accel.value().value, 9.81));
    }

    // 6. Dimensionless and raw numbers
    {
        auto res_num = base::units::parse_quantity("42.5");
        REQUIRE(res_num.is_ok());
        CHECK(res_num.value().dimension.is_dimensionless());
        CHECK(is_approx(res_num.value().value, 42.5));
    }

    // 7. Error handling on invalid or unknown unit inputs
    {
        auto empty_res = base::units::parse_quantity("   ");
        CHECK(empty_res.is_err());

        auto invalid_num = base::units::parse_quantity("hello");
        CHECK(invalid_num.is_err());

        auto unknown_unit = base::units::parse_quantity("100 lightyears");
        CHECK(unknown_unit.is_err());
    }

    // 8. Unit Registry compatible unit queries
    {
        auto length_units = base::units::UnitRegistry::instance().get_compatible_units("mm");
        CHECK(!length_units.empty());
        CHECK(std::find(length_units.begin(), length_units.end(), "m") != length_units.end());
        CHECK(std::find(length_units.begin(), length_units.end(), "mm") != length_units.end());
        CHECK(std::find(length_units.begin(), length_units.end(), "in") != length_units.end());

        auto res_units = base::units::UnitRegistry::instance().get_compatible_units("kOhm");
        CHECK(std::find(res_units.begin(), res_units.end(), "Ohm") != res_units.end());
        CHECK(std::find(res_units.begin(), res_units.end(), "MOhm") != res_units.end());
    }
}

// =============================================================================
// Task UI-02-001-03: Parametric Mathematical Expression Evaluator
// =============================================================================
TEST_CASE("UI-02-001-03: Parametric Mathematical Formula Expression Evaluator") {
    base::ExpressionEvaluator evaluator;

    // 1. Operator precedence and arithmetic syntax
    {
        auto r1 = evaluator.evaluate("2 + 3 * 4");
        REQUIRE(r1.is_ok());
        CHECK(is_approx(r1.value(), 14.0));

        auto r2 = evaluator.evaluate("(2 + 3) * 4");
        REQUIRE(r2.is_ok());
        CHECK(is_approx(r2.value(), 20.0));

        auto r3 = evaluator.evaluate("10 - 4 - 2");
        REQUIRE(r3.is_ok());
        CHECK(is_approx(r3.value(), 4.0));

        auto r4 = evaluator.evaluate("2 ^ 3 ^ 2"); // Right-associative: 2^(9) = 512
        REQUIRE(r4.is_ok());
        CHECK(is_approx(r4.value(), 512.0));

        auto r5 = evaluator.evaluate("-5 + 10");
        REQUIRE(r5.is_ok());
        CHECK(is_approx(r5.value(), 5.0));

        auto r6 = evaluator.evaluate("=100 / 4"); // Optional leading '='
        REQUIRE(r6.is_ok());
        CHECK(is_approx(r6.value(), 25.0));
    }

    // 2. Built-in mathematical functions and constants
    {
        auto r_pi = evaluator.evaluate("sin(pi / 2)");
        REQUIRE(r_pi.is_ok());
        CHECK(is_approx(r_pi.value(), 1.0));

        auto r_cos = evaluator.evaluate("cos(0)");
        REQUIRE(r_cos.is_ok());
        CHECK(is_approx(r_cos.value(), 1.0));

        auto r_sqrt = evaluator.evaluate("sqrt(144)");
        REQUIRE(r_sqrt.is_ok());
        CHECK(is_approx(r_sqrt.value(), 12.0));

        auto r_min_max = evaluator.evaluate("max(10, min(50, 25))");
        REQUIRE(r_min_max.is_ok());
        CHECK(is_approx(r_min_max.value(), 25.0));

        auto r_abs = evaluator.evaluate("abs(-99.5)");
        REQUIRE(r_abs.is_ok());
        CHECK(is_approx(r_abs.value(), 99.5));
    }

    // 3. Inline physical unit suffixes in mathematical expressions
    {
        // 5mm = 0.005 m, 10mm = 0.010 m
        auto r_units = evaluator.evaluate("=5mm + 10mm");
        REQUIRE(r_units.is_ok());
        CHECK(is_approx(r_units.value(), 0.015));

        auto r_res = evaluator.evaluate("=1kOhm + 500Ohm");
        REQUIRE(r_res.is_ok());
        CHECK(is_approx(r_res.value(), 1500.0));
    }

    // 4. Variable reference and context evaluation
    {
        std::unordered_map<std::string, double> ctx = {
            {"width", 0.050},
            {"scale", 2.5},
            {"radius", 0.010}
        };

        auto r_ctx = evaluator.evaluate_with_context("=width * scale + 5mm", ctx);
        REQUIRE(r_ctx.is_ok());
        // 0.050 * 2.5 + 0.005 = 0.125 + 0.005 = 0.130
        CHECK(is_approx(r_ctx.value(), 0.130));

        auto vars = base::ExpressionEvaluator::extract_variables("=width * 2 + sin(radius * pi) + offset");
        CHECK(vars.size() == 3);
        CHECK(std::find(vars.begin(), vars.end(), "width") != vars.end());
        CHECK(std::find(vars.begin(), vars.end(), "radius") != vars.end());
        CHECK(std::find(vars.begin(), vars.end(), "offset") != vars.end());
    }

    // 5. Syntax error and division by zero protection
    {
        auto err_div = evaluator.evaluate("10 / 0");
        CHECK(err_div.is_err());
        CHECK(err_div.error().code == 3024);

        auto err_paren = evaluator.evaluate("(2 + 3 * 5");
        CHECK(err_paren.is_err());
    }
}

// =============================================================================
// Task UI-02-001-03: Parameter Reference Manager & Circular Dependency Detection
// =============================================================================
TEST_CASE("UI-02-001-03: Parameter Dependency Graph and Cycle Detection") {
    domain::ParameterReferenceManager param_mgr;

    // 1. Topological dependency evaluation and updates
    {
        REQUIRE(param_mgr.set_parameter("length", "100mm", "mm").is_ok());
        CHECK(is_approx(param_mgr.get_value("length").value(), 0.100));

        REQUIRE(param_mgr.set_parameter("width", "=length / 2", "mm").is_ok());
        CHECK(is_approx(param_mgr.get_value("width").value(), 0.050));

        REQUIRE(param_mgr.set_parameter("area", "=length * width", "m^2").is_ok());
        CHECK(is_approx(param_mgr.get_value("area").value(), 0.005));

        // When length updates, width and area must automatically re-evaluate
        REQUIRE(param_mgr.set_parameter("length", "200mm", "mm").is_ok());
        CHECK(is_approx(param_mgr.get_value("length").value(), 0.200));
        CHECK(is_approx(param_mgr.get_value("width").value(), 0.100));
        CHECK(is_approx(param_mgr.get_value("area").value(), 0.020));
    }

    // 2. Circular dependency detection prevents infinite recursion
    {
        REQUIRE(param_mgr.set_parameter("a", "10").is_ok());
        REQUIRE(param_mgr.set_parameter("b", "=a * 2").is_ok());
        REQUIRE(param_mgr.set_parameter("c", "=b + 5").is_ok());

        // Attempting to make 'a' depend on 'c' introduces cycle: a -> b -> c -> a
        auto cycle_res = param_mgr.set_parameter("a", "=c + 1");
        REQUIRE(cycle_res.is_err());
        CHECK(cycle_res.error().code == 3040); // Circular dependency detected

        // Direct self-reference cycle
        auto self_res = param_mgr.set_parameter("b", "=b + 1");
        REQUIRE(self_res.is_err());
        CHECK(self_res.error().code == 3040);
    }
}

// =============================================================================
// Task UI-02-001-01: Reflected Property Tree & PropertySheet
// =============================================================================
TEST_CASE("UI-02-001-01: Hierarchical Reflected Property Sheet and Sub-Millisecond Propagation") {
    // 1. Create Element Definition
    auto def = std::make_shared<domain::ElementDefinition>(
        "Resistor",
        "Passive Resistor",
        domain::ElementCategory::Device
    );

    domain::ParameterSchema res_schema;
    res_schema.name = "resistance";
    res_schema.display_name = "Resistance";
    res_schema.type = domain::ParameterType::Quantity;
    res_schema.unit = "kOhm";
    res_schema.default_value = domain::ParameterValue(1.0);
    res_schema.min_value = 0.0;
    def->add_parameter(res_schema);

    domain::ParameterSchema pwr_schema;
    pwr_schema.name = "power_rating";
    pwr_schema.display_name = "Power Rating";
    pwr_schema.type = domain::ParameterType::Quantity;
    pwr_schema.unit = "W";
    pwr_schema.default_value = domain::ParameterValue(0.25);
    def->add_parameter(pwr_schema);

    // 2. Create Instance
    auto instance = std::make_shared<domain::ElementInstance>(def, "R1");
    auto sheet = domain::PropertySheet::create_for_element_instance(instance);

    REQUIRE(sheet != nullptr);
    CHECK(sheet->categories().size() >= 3); // General, Transform, Parameters

    // 3. Reflection inspection of properties
    {
        const auto* name_prop = sheet->find_property("general.name");
        REQUIRE(name_prop != nullptr);
        CHECK(name_prop->value.as_string() == "R1");

        const auto* pos_prop = sheet->find_property("transform.position");
        REQUIRE(pos_prop != nullptr);
        CHECK(pos_prop->visual_type == domain::PropertyVisualType::Vector3);

        const auto* res_prop = sheet->find_property("parameter.resistance");
        REQUIRE(res_prop != nullptr);
        CHECK(res_prop->unit_symbol == "kOhm");
        CHECK(is_approx(res_prop->value.as_double(), 1.0));
    }

    // 4. Sub-millisecond property mutation propagation (< 1ms)
    {
        auto start = std::chrono::high_resolution_clock::now();

        // Mutate resistance in property sheet
        auto set_res = sheet->set_property_value("parameter.resistance", domain::ParameterValue(4.7));
        REQUIRE(set_res.is_ok());

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        ).count();

        // Check value in underlying element instance
        auto inst_val = instance->get_parameter("resistance");
        REQUIRE(inst_val.is_ok());
        CHECK(is_approx(inst_val.value().as_double(), 4.7));

        // Acceptance criteria: < 1ms (1000 microseconds)
        CHECK(elapsed < 1000);
    }

    // 5. Transform mutation propagates to ElementInstance
    {
        base::math::Vec3 new_pos(10.0, -5.0, 2.5);
        auto set_pos = sheet->set_property_value("transform.position", domain::ParameterValue(new_pos));
        REQUIRE(set_pos.is_ok());

        CHECK(is_approx(instance->transform().position.x, 10.0));
        CHECK(is_approx(instance->transform().position.y, -5.0));
        CHECK(is_approx(instance->transform().position.z, 2.5));
    }

    // 6. Listener notifications on external refresh
    {
        int notification_count = 0;
        sheet->add_listener([&](const std::string& /*id*/, const domain::ParameterValue& /*val*/) {
            ++notification_count;
        });

        REQUIRE(instance->set_parameter("resistance", domain::ParameterValue(10.0)).is_ok());
        sheet->refresh_values();

        CHECK(notification_count > 0);
        const auto* res_prop = sheet->find_property("parameter.resistance");
        REQUIRE(res_prop != nullptr);
        CHECK(is_approx(res_prop->value.as_double(), 10.0));
    }
}
