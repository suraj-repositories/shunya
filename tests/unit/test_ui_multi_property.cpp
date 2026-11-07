#include "tests/test_framework.hpp"
#include "shunya/core/domain/multi_property_analyzer.hpp"
#include "shunya/core/domain/property_node.hpp"
#include "shunya/core/domain/element_instance.hpp"
#include "shunya/core/domain/element_definition.hpp"
#include "shunya/core/domain/command_stack.hpp"

#include <memory>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

namespace {

std::shared_ptr<ElementDefinition> create_resistor_def() {
    auto def = std::make_shared<ElementDefinition>("Resistor", "Resistor", ElementCategory::Device);
    ParameterSchema r_schema;
    r_schema.name = "resistance";
    r_schema.display_name = "Resistance";
    r_schema.type = ParameterType::Quantity;
    r_schema.unit = "kOhm";
    r_schema.default_value = ParameterValue(1.0);
    def->add_parameter(r_schema);

    ParameterSchema tol_schema;
    tol_schema.name = "tolerance";
    tol_schema.display_name = "Tolerance";
    tol_schema.type = ParameterType::Double;
    tol_schema.default_value = ParameterValue(0.05);
    def->add_parameter(tol_schema);

    return def;
}

std::shared_ptr<ElementDefinition> create_capacitor_def() {
    auto def = std::make_shared<ElementDefinition>("Capacitor", "Capacitor", ElementCategory::Device);
    ParameterSchema c_schema;
    c_schema.name = "capacitance";
    c_schema.display_name = "Capacitance";
    c_schema.type = ParameterType::Quantity;
    c_schema.unit = "uF";
    c_schema.default_value = ParameterValue(10.0);
    def->add_parameter(c_schema);

    ParameterSchema volt_schema;
    volt_schema.name = "voltage_rating";
    volt_schema.display_name = "Voltage Rating";
    volt_schema.type = ParameterType::Double;
    volt_schema.default_value = ParameterValue(50.0);
    def->add_parameter(volt_schema);

    return def;
}

} // namespace

TEST_CASE("UI Multi-Property: Empty Selection Yields Empty Intersection") {
    std::vector<std::shared_ptr<PropertySheet>> empty_selection;
    auto intersected = PropertyIntersectionAnalyzer::analyze(empty_selection);
    REQUIRE(intersected.empty());
}

TEST_CASE("UI Multi-Property: Single Selection Returns All Properties") {
    auto r_def = create_resistor_def();
    auto inst = std::make_shared<ElementInstance>(r_def, "R1");
    auto sheet = PropertySheet::create_for_element_instance(inst);

    std::vector<std::shared_ptr<PropertySheet>> selection = {sheet};
    auto intersected = PropertyIntersectionAnalyzer::analyze(selection);

    REQUIRE(!intersected.empty());
    bool has_resistance = false;
    for (const auto& item : intersected) {
        if (item.property_id == "parameter.resistance") {
            has_resistance = true;
            CHECK(!item.is_mixed);
            CHECK(item.unified_value.as_double() == 1.0);
            CHECK(item.unit_symbol == "kOhm");
        }
    }
    CHECK(has_resistance);
}

TEST_CASE("UI Multi-Property: 50 Identical Resistors Intersect Unified Value") {
    auto r_def = create_resistor_def();
    std::vector<std::shared_ptr<PropertySheet>> selection;
    for (int i = 0; i < 50; ++i) {
        auto inst = std::make_shared<ElementInstance>(r_def, "R" + std::to_string(i + 1));
        selection.push_back(PropertySheet::create_for_element_instance(inst));
    }

    auto intersected = PropertyIntersectionAnalyzer::analyze(selection);
    REQUIRE(!intersected.empty());

    bool found_resistance = false;
    for (const auto& item : intersected) {
        if (item.property_id == "parameter.resistance") {
            found_resistance = true;
            CHECK(!item.is_mixed);
            CHECK(item.unified_value.as_double() == 1.0);
            CHECK(item.unit_symbol == "kOhm");
        }
    }
    CHECK(found_resistance);
}

TEST_CASE("UI Multi-Property: 50 Resistors with Divergent Values Yields Mixed Placeholder") {
    auto r_def = create_resistor_def();
    std::vector<std::shared_ptr<PropertySheet>> selection;
    for (int i = 0; i < 50; ++i) {
        auto inst = std::make_shared<ElementInstance>(r_def, "R" + std::to_string(i + 1));
        auto sheet = PropertySheet::create_for_element_instance(inst);
        (void)sheet->set_property_value("parameter.resistance", ParameterValue(10.0 * (i + 1)));
        selection.push_back(sheet);
    }

    auto intersected = PropertyIntersectionAnalyzer::analyze(selection);
    REQUIRE(!intersected.empty());

    bool found_resistance = false;
    for (const auto& item : intersected) {
        if (item.property_id == "parameter.resistance") {
            found_resistance = true;
            CHECK(item.is_mixed == true);
            CHECK(item.display_name == "Resistance");
        }
    }
    CHECK(found_resistance);
}

TEST_CASE("UI Multi-Property: Heterogeneous Selection (Resistor and Capacitor) Only Common Properties") {
    auto r_def = create_resistor_def();
    auto c_def = create_capacitor_def();

    auto r_inst = std::make_shared<ElementInstance>(r_def, "R1");
    auto c_inst = std::make_shared<ElementInstance>(c_def, "C1");

    auto r_sheet = PropertySheet::create_for_element_instance(r_inst);
    auto c_sheet = PropertySheet::create_for_element_instance(c_inst);

    std::vector<std::shared_ptr<PropertySheet>> selection = {r_sheet, c_sheet};
    auto intersected = PropertyIntersectionAnalyzer::analyze(selection);

    // Resistance and Capacitance must NOT be in the intersection
    for (const auto& item : intersected) {
        CHECK(item.property_id != "parameter.resistance");
        CHECK(item.property_id != "parameter.capacitance");
    }

    // Shared properties like transform.position, transform.scale must be present
    bool has_pos = false;
    bool has_scale = false;
    for (const auto& item : intersected) {
        if (item.property_id == "transform.position") has_pos = true;
        if (item.property_id == "transform.scale") has_scale = true;
    }
    CHECK(has_pos);
    CHECK(has_scale);
}

TEST_CASE("UI Multi-Property: BatchSetPropertyCommand Modifies 100 Entities with Undo/Redo") {
    auto r_def = create_resistor_def();
    std::vector<std::shared_ptr<PropertySheet>> selection;
    for (int i = 0; i < 100; ++i) {
        auto inst = std::make_shared<ElementInstance>(r_def, "R" + std::to_string(i + 1));
        auto sheet = PropertySheet::create_for_element_instance(inst);
        (void)sheet->set_property_value("parameter.resistance", ParameterValue(2.2));
        selection.push_back(sheet);
    }

    auto batch_cmd = std::make_unique<BatchSetPropertyCommand>("Batch Set 100 Resistors");
    for (const auto& sheet : selection) {
        const auto* prop = sheet->find_property("parameter.resistance");
        REQUIRE(prop != nullptr);
        batch_cmd->add_modification(sheet, "parameter.resistance", prop->value, ParameterValue(47.0));
    }

    CommandStack stack;
    auto push_res = stack.push_and_execute(std::move(batch_cmd));
    REQUIRE(push_res.is_ok());

    // Verify all 100 sheets updated to 47.0
    for (const auto& sheet : selection) {
        const auto* prop = sheet->find_property("parameter.resistance");
        REQUIRE(prop != nullptr);
        CHECK(prop->value.as_double() == 47.0);
    }

    // Undo: all 100 sheets should revert to 2.2
    CHECK(stack.can_undo());
    auto undo_res = stack.undo();
    REQUIRE(undo_res.is_ok());
    for (const auto& sheet : selection) {
        const auto* prop = sheet->find_property("parameter.resistance");
        REQUIRE(prop != nullptr);
        CHECK(prop->value.as_double() == 2.2);
    }

    // Redo: all 100 sheets should be 47.0 again
    CHECK(stack.can_redo());
    auto redo_res = stack.redo();
    REQUIRE(redo_res.is_ok());
    for (const auto& sheet : selection) {
        const auto* prop = sheet->find_property("parameter.resistance");
        REQUIRE(prop != nullptr);
        CHECK(prop->value.as_double() == 47.0);
    }
}
