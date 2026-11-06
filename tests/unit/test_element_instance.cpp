#include "tests/test_framework.hpp"
#include "shunya/core/domain/element_instance.hpp"
#include "shunya/core/domain/element_definition.hpp"

using namespace shunya::core;
using namespace shunya::core::domain;

namespace {

std::shared_ptr<ElementDefinition> create_mock_resistor_def() {
    auto def = std::make_shared<ElementDefinition>(
        "shunya.devices.electrical.resistor",
        "Linear Resistor",
        ElementCategory::Device
    );

    ParameterSchema r_schema;
    r_schema.name = "resistance";
    r_schema.display_name = "Resistance";
    r_schema.type = ParameterType::Double;
    r_schema.default_value = 100.0;
    r_schema.unit = "ohm";
    r_schema.min_value = 0.001;
    r_schema.max_value = 1e6;
    def->add_parameter(std::move(r_schema));

    ParameterSchema tol_schema;
    tol_schema.name = "tolerance";
    tol_schema.display_name = "Tolerance";
    tol_schema.type = ParameterType::Double;
    tol_schema.default_value = 0.05;
    tol_schema.is_read_only = true;
    def->add_parameter(std::move(tol_schema));

    PortSchema p1{"p", "Positive Pin", "", PortDomain::Electrical, PortDirection::Bidirectional, PortCardinality::Single};
    PortSchema p2{"n", "Negative Pin", "", PortDomain::Electrical, PortDirection::Bidirectional, PortCardinality::Single};
    def->add_port(std::move(p1));
    def->add_port(std::move(p2));

    return def;
}

std::shared_ptr<ElementDefinition> create_mock_voltage_source_def() {
    auto def = std::make_shared<ElementDefinition>(
        "shunya.sources.dc_voltage",
        "DC Voltage Source",
        ElementCategory::Source
    );

    ParameterSchema v_schema;
    v_schema.name = "voltage";
    v_schema.type = ParameterType::Double;
    v_schema.default_value = 12.0;
    v_schema.unit = "V";
    def->add_parameter(std::move(v_schema));

    PortSchema pos{"pos", "Positive Terminal", "", PortDomain::Electrical, PortDirection::Output, PortCardinality::Single};
    PortSchema neg{"neg", "Negative Terminal", "", PortDomain::Electrical, PortDirection::Output, PortCardinality::Single};
    def->add_port(std::move(pos));
    def->add_port(std::move(neg));

    return def;
}

} // namespace

TEST_CASE("ElementInstance parameter access, overrides and read-only guards") {
    auto res_def = create_mock_resistor_def();
    ElementInstance inst(res_def, "R1");

    // Default value before override
    auto r_val = inst.get_parameter("resistance");
    REQUIRE(r_val.is_ok());
    CHECK(base::math::approx_equal(r_val.value().as_double(), 100.0));
    CHECK(!inst.has_parameter_override("resistance"));

    // Override with valid value
    REQUIRE(inst.set_parameter("resistance", 220.0).is_ok());
    CHECK(inst.has_parameter_override("resistance"));
    CHECK(base::math::approx_equal(inst.get_parameter("resistance").value().as_double(), 220.0));

    // Try modifying read-only parameter
    auto ro_res = inst.set_parameter("tolerance", 0.01);
    CHECK(ro_res.is_err());
    CHECK(ro_res.error().code == 3203);

    // Reset to default
    inst.reset_parameter_to_default("resistance");
    CHECK(!inst.has_parameter_override("resistance"));
    CHECK(base::math::approx_equal(inst.get_parameter("resistance").value().as_double(), 100.0));
}

TEST_CASE("ElementInstance lifecycle transitions and simulated time") {
    auto res_def = create_mock_resistor_def();
    ElementInstance inst(res_def, "R_Load");

    CHECK(inst.lifecycle_state() == LifecycleState::Uninitialized);
    CHECK(base::math::approx_equal(inst.simulated_time(), 0.0));

    // Initialize
    REQUIRE(inst.initialize().is_ok());
    CHECK(inst.lifecycle_state() == LifecycleState::Configured);

    // Update time step
    REQUIRE(inst.update(0.01).is_ok());
    CHECK(inst.lifecycle_state() == LifecycleState::Active);
    CHECK(base::math::approx_equal(inst.simulated_time(), 0.01));

    REQUIRE(inst.update(0.02).is_ok());
    CHECK(base::math::approx_equal(inst.simulated_time(), 0.03));

    // Pause and resume
    REQUIRE(inst.pause().is_ok());
    CHECK(inst.lifecycle_state() == LifecycleState::Paused);
    REQUIRE(inst.update(0.05).is_ok()); // Time should freeze
    CHECK(base::math::approx_equal(inst.simulated_time(), 0.03));

    REQUIRE(inst.resume().is_ok());
    CHECK(inst.lifecycle_state() == LifecycleState::Active);
    REQUIRE(inst.update(0.01).is_ok());
    CHECK(base::math::approx_equal(inst.simulated_time(), 0.04));

    // Reset
    REQUIRE(inst.reset().is_ok());
    CHECK(inst.lifecycle_state() == LifecycleState::Configured);
    CHECK(base::math::approx_equal(inst.simulated_time(), 0.0));
}

TEST_CASE("ElementInstance duplication preserves overrides and transforms") {
    auto res_def = create_mock_resistor_def();
    ElementInstance r1(res_def, "R1");
    r1.transform().position = base::math::Vec3{5.0, 10.0, 0.0};
    REQUIRE(r1.set_parameter("resistance", 470.0).is_ok());

    auto r2 = r1.duplicate("R2", base::math::Vec3{1.0, 0.0, 0.0});
    REQUIRE(r2 != nullptr);
    CHECK(r2->id() != r1.id());
    CHECK(r2->name() == "R2");
    CHECK(base::math::approx_equal(r2->transform().position.x, 6.0));
    CHECK(base::math::approx_equal(r2->transform().position.y, 10.0));
    CHECK(base::math::approx_equal(r2->get_parameter("resistance").value().as_double(), 470.0));
}

TEST_CASE("ElementInspector property introspection and dynamic edits") {
    auto res_def = create_mock_resistor_def();
    ElementInstance inst(res_def, "R_Insp");
    REQUIRE(inst.set_parameter("resistance", 330.0).is_ok());

    auto props = ElementInspector::inspect(inst);
    REQUIRE(props.size() == 2);

    CHECK(props[0].name == "resistance");
    CHECK(props[0].display_label == "Resistance");
    CHECK(props[0].current_value_str.find("330") != std::string::npos);
    CHECK(props[0].unit == "ohm");
    CHECK(props[0].is_overridden);
    CHECK(!props[0].is_read_only);

    CHECK(props[1].name == "tolerance");
    CHECK(props[1].is_read_only);
    CHECK(!props[1].is_overridden);

    // Apply valid change via inspector
    REQUIRE(ElementInspector::apply_change(inst, "resistance", 680.0).is_ok());
    CHECK(base::math::approx_equal(inst.get_parameter("resistance").value().as_double(), 680.0));

    // Apply invalid change (out of bounds)
    auto bad_change = ElementInspector::apply_change(inst, "resistance", -5.0);
    CHECK(bad_change.is_err());
    CHECK(bad_change.error().code == 3102);
}

TEST_CASE("ElementRegistry catalog, instantiation, and multi-domain connection wiring") {
    ElementRegistry registry;
    auto r_def = create_mock_resistor_def();
    auto v_def = create_mock_voltage_source_def();

    registry.register_definition(r_def);
    registry.register_definition(v_def);

    CHECK(registry.has_definition(r_def->id()));
    CHECK(registry.has_definition("shunya.sources.dc_voltage"));
    CHECK(registry.list_definitions().size() == 2);
    CHECK(registry.list_by_category(ElementCategory::Source).size() == 1);

    // Create instances
    auto r1_res = registry.create_instance("shunya.devices.electrical.resistor", "Resistor_1");
    auto v1_res = registry.create_instance("shunya.sources.dc_voltage", "Source_1");
    REQUIRE(r1_res.is_ok());
    REQUIRE(v1_res.is_ok());
    auto r1 = r1_res.value();
    auto v1 = v1_res.value();

    CHECK(registry.instance_count() == 2);

    // Connect Source_1.pos to Resistor_1.p
    auto conn_res = registry.connect(v1->id(), "pos", r1->id(), "p");
    REQUIRE(conn_res.is_ok());

    CHECK(v1->get_port("pos")->is_connected());
    CHECK(r1->get_port("p")->is_connected());
    CHECK(v1->get_port("pos")->connections()[0].target_instance_id == r1->id());
    CHECK(r1->get_port("p")->connections()[0].target_instance_id == v1->id());

    // Single cardinality violation test
    auto r2_res = registry.create_instance("shunya.devices.electrical.resistor", "Resistor_2");
    REQUIRE(r2_res.is_ok());
    auto r2 = r2_res.value();

    auto conflict_conn = registry.connect(r2->id(), "p", r1->id(), "p");
    CHECK(conflict_conn.is_err());
    CHECK(conflict_conn.error().code == 3201); // Port already connected

    // Disconnect
    REQUIRE(registry.disconnect(v1->id(), "pos", r1->id(), "p").is_ok());
    CHECK(!v1->get_port("pos")->is_connected());
    CHECK(!r1->get_port("p")->is_connected());
}
