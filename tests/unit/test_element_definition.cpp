#include "tests/test_framework.hpp"
#include "shunya/core/domain/element_definition.hpp"
#include "shunya/core/base/units.hpp"

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("ElementCategory conversions and coverage") {
    CHECK(element_category_to_string(ElementCategory::PhysicalObject) == "PhysicalObject");
    CHECK(element_category_to_string(ElementCategory::Sensor) == "Sensor");
    CHECK(element_category_to_string(ElementCategory::Actuator) == "Actuator");
    CHECK(element_category_to_string(ElementCategory::Device) == "Device");
    CHECK(element_category_to_string(ElementCategory::LogicBlock) == "LogicBlock");
    CHECK(element_category_to_string(ElementCategory::AbstractNode) == "AbstractNode");

    auto cat_opt = element_category_from_string("Actuator");
    REQUIRE(cat_opt.has_value());
    CHECK(cat_opt.value() == ElementCategory::Actuator);

    CHECK(!element_category_from_string("NonExistentCategory").has_value());
}

TEST_CASE("ParameterValue typed storage and stringification") {
    ParameterValue val_d(42.5);
    CHECK(val_d.type() == ParameterType::Double);
    CHECK(base::math::approx_equal(val_d.as_double(), 42.5));

    ParameterValue val_i(static_cast<int64_t>(1000));
    CHECK(val_i.type() == ParameterType::Int64);
    CHECK(val_i.as_int64() == 1000);

    ParameterValue val_b(true);
    CHECK(val_b.type() == ParameterType::Bool);
    CHECK(val_b.as_bool());

    ParameterValue val_s("Brushless_DC");
    CHECK(val_s.type() == ParameterType::String);
    CHECK(val_s.as_string() == "Brushless_DC");

    base::units::Quantity force(25.0, base::units::Force);
    ParameterValue val_q(force);
    CHECK(val_q.type() == ParameterType::Quantity);
    CHECK(base::math::approx_equal(val_q.as_quantity().value, 25.0));

    base::math::Vec3 vec{1.0, 2.0, 3.0};
    ParameterValue val_v(vec);
    CHECK(val_v.type() == ParameterType::Vec3);
    CHECK(base::math::approx_equal(val_v.as_vec3().y, 2.0));
}

TEST_CASE("ParameterSchema validation and constraints") {
    ParameterSchema resistance_schema;
    resistance_schema.name = "resistance";
    resistance_schema.display_name = "Phase Resistance";
    resistance_schema.type = ParameterType::Double;
    resistance_schema.unit = "ohm";
    resistance_schema.min_value = 0.001;
    resistance_schema.max_value = 1000.0;

    // Nominal within bounds
    CHECK(resistance_schema.validate(0.5).is_ok());

    // Below min
    auto under_res = resistance_schema.validate(0.0001);
    CHECK(under_res.is_err());
    CHECK(under_res.error().code == 3102);

    // Above max
    auto over_res = resistance_schema.validate(5000.0);
    CHECK(over_res.is_err());
    CHECK(over_res.error().code == 3103);

    // Type mismatch
    auto bad_type_res = resistance_schema.validate("invalid_str");
    CHECK(bad_type_res.is_err());
    CHECK(bad_type_res.error().code == 3101);

    // Enum schema
    ParameterSchema winding_type;
    winding_type.name = "winding";
    winding_type.type = ParameterType::Enum;
    winding_type.enum_options = {"Star", "Delta", "Independent"};

    CHECK(winding_type.validate(std::string("Star")).is_ok());
    CHECK(winding_type.validate(std::string("Delta")).is_ok());

    auto bad_enum = winding_type.validate(std::string("Polygon"));
    CHECK(bad_enum.is_err());
    CHECK(bad_enum.error().code == 3104);
}

TEST_CASE("PortSchema domain and direction connectivity rules") {
    PortSchema pin_a;
    pin_a.name = "pin_plus";
    pin_a.domain = PortDomain::Electrical;
    pin_a.direction = PortDirection::Bidirectional;

    PortSchema pin_b;
    pin_b.name = "pin_minus";
    pin_b.domain = PortDomain::Electrical;
    pin_b.direction = PortDirection::Bidirectional;

    PortSchema flange;
    flange.name = "flange_rot";
    flange.domain = PortDomain::MechanicalRotational;
    flange.direction = PortDirection::Bidirectional;

    // Same domain connects
    CHECK(pin_a.can_connect_to(pin_b));

    // Cross-domain rejected
    CHECK(!pin_a.can_connect_to(flange));

    // Direction rules
    PortSchema in_port{"in", "In", "", PortDomain::Signal, PortDirection::Input};
    PortSchema out_port{"out", "Out", "", PortDomain::Signal, PortDirection::Output};
    PortSchema in_port2{"in2", "In 2", "", PortDomain::Signal, PortDirection::Input};
    PortSchema out_port2{"out2", "Out 2", "", PortDomain::Signal, PortDirection::Output};

    CHECK(in_port.can_connect_to(out_port));
    CHECK(out_port.can_connect_to(in_port));
    CHECK(!in_port.can_connect_to(in_port2));  // Input to Input rejected
    CHECK(!out_port.can_connect_to(out_port2)); // Output to Output rejected
}

TEST_CASE("ElementDefinition complete specification and JSON roundtrip") {
    ElementDefinition motor_def(
        "shunya.actuators.pmsm_motor",
        "Permanent Magnet Synchronous Motor",
        ElementCategory::Actuator,
        base::Version(2, 1, 0, "release")
    );
    motor_def.set_description("High performance 3-phase synchronous electric motor");
    motor_def.set_author("Drivetrain Lab");

    // Add parameters
    ParameterSchema rs;
    rs.name = "stator_resistance";
    rs.display_name = "Stator Resistance";
    rs.type = ParameterType::Double;
    rs.unit = "ohm";
    motor_def.add_parameter(std::move(rs));

    ParameterSchema poles;
    poles.name = "pole_pairs";
    poles.display_name = "Number of Pole Pairs";
    poles.type = ParameterType::Int64;
    motor_def.add_parameter(std::move(poles));

    // Add ports
    PortSchema p_u{"phase_u", "Phase U", "", PortDomain::Electrical, PortDirection::Bidirectional, PortCardinality::Single, "V"};
    PortSchema p_v{"phase_v", "Phase V", "", PortDomain::Electrical, PortDirection::Bidirectional, PortCardinality::Single, "V"};
    PortSchema p_w{"phase_w", "Phase W", "", PortDomain::Electrical, PortDirection::Bidirectional, PortCardinality::Single, "V"};
    PortSchema p_shaft{"rotor_shaft", "Rotor Mechanical Shaft", "", PortDomain::MechanicalRotational, PortDirection::Bidirectional, PortCardinality::Single, "rad/s"};

    motor_def.add_port(p_u);
    motor_def.add_port(p_v);
    motor_def.add_port(p_w);
    motor_def.add_port(p_shaft);

    CHECK(motor_def.has_parameter("stator_resistance"));
    CHECK(motor_def.has_parameter("pole_pairs"));
    CHECK(!motor_def.has_parameter("flux_leakage"));

    CHECK(motor_def.has_port("phase_u"));
    CHECK(motor_def.has_port("rotor_shaft"));
    CHECK(motor_def.ports().size() == 4);

    // Serialization roundtrip
    std::string json = motor_def.to_json();
    CHECK(!json.empty());
    CHECK(json.find("\"shunya.actuators.pmsm_motor\"") != std::string::npos);
    CHECK(json.find("\"stator_resistance\"") != std::string::npos);
    CHECK(json.find("\"rotor_shaft\"") != std::string::npos);

    auto parsed_res = ElementDefinition::from_json(json);
    REQUIRE(parsed_res.is_ok());
    auto parsed = parsed_res.value();

    CHECK(parsed->type_name() == "shunya.actuators.pmsm_motor");
    CHECK(parsed->category() == ElementCategory::Actuator);
    CHECK(parsed->version().major == 2);
    CHECK(parsed->version().minor == 1);
    CHECK(parsed->parameters().size() == 2);
    CHECK(parsed->ports().size() == 4);
    CHECK(parsed->get_port_schema("rotor_shaft")->domain == PortDomain::MechanicalRotational);
}
