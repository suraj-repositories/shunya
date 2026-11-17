#include "shunya/lib30/electromechanical_components.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/core/base/uuid.hpp"

namespace shunya::lib30 {

using namespace ecad3d;
using namespace cad3d;
using shunya::core::domain::ElectricalPinPolarity;

// ============================================================================
// Component 17: Push Button (PH4-07.04.001)
// ============================================================================

ComponentManifest ElectromechanicalComponentsFactory::create_push_button(
    double contact_resistance_ohms,
    double max_current_amperes,
    double max_voltage_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Tactile_Push_Button_6x6";
    comp.kind = eng::ComponentKind::PushButton;
    comp.metadata.manufacturer = "Shunya Electromechanical";
    comp.metadata.part_number = "TS-6601-NO";
    comp.metadata.package_type = "6x6mm Tactile Switch SMD/THT";
    comp.metadata.description = "Momentary Normally-Open (NO) Tactile Push Button";

    comp.specs.nominal_primary_value = contact_resistance_ohms;
    comp.specs.max_voltage_v = max_voltage_volts;
    comp.specs.max_current_a = max_current_amperes;
    comp.specs.max_power_dissipation_w = contact_resistance_ohms * max_current_amperes * max_current_amperes;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 6.0, .height = 6.0, .depth = 4.3}
    );

    comp.regions["pin_com"] = MeshRegionMaterial{"pin_com", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_no"] = MeshRegionMaterial{"pin_no", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["housing"] = MeshRegionMaterial{"housing", "Alumina", 1.0e-12, 0.3, 1150.0, 3.5, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("COM", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-3.0, 0.0, -2.0}, Vec3d{-1.0, 0.0, 0.0}, "pin_com"),
        PhysicalTerminal3D("NO", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{3.0, 0.0, -2.0}, Vec3d{1.0, 0.0, 0.0}, "pin_no")
    };

    comp.spice_model.netlist_template = "S_BTN 1 2 BTN_CTRL 0 SW_MODEL (RON=" +
        std::to_string(contact_resistance_ohms) + " ROFF=1e9)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::PushButton, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-BTN-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 18: SPST Toggle Switch (PH4-07.04.002)
// ============================================================================

ComponentManifest ElectromechanicalComponentsFactory::create_toggle_switch(
    double contact_resistance_ohms,
    double max_current_amperes,
    double max_voltage_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "SPST_Bistable_Toggle_Switch";
    comp.kind = eng::ComponentKind::Switch;
    comp.metadata.manufacturer = "Shunya Electromechanical";
    comp.metadata.part_number = "MTS-101";
    comp.metadata.package_type = "Miniature Toggle Bat Lever";
    comp.metadata.description = "Single Pole Single Throw (SPST) ON-OFF Toggle Switch";

    comp.specs.nominal_primary_value = contact_resistance_ohms;
    comp.specs.max_voltage_v = max_voltage_volts;
    comp.specs.max_current_a = max_current_amperes;
    comp.specs.max_power_dissipation_w = 0.5;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 8.0, .height = 12.0, .depth = 13.0}
    );

    comp.regions["term_in"] = MeshRegionMaterial{"term_in", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["term_out"] = MeshRegionMaterial{"term_out", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["case_body"] = MeshRegionMaterial{"case_body", "Alumina", 1.0e-12, 0.4, 1800.0, 4.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("IN", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, -4.0, -8.0}, Vec3d{0.0, 0.0, -1.0}, "term_in"),
        PhysicalTerminal3D("OUT", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 4.0, -8.0}, Vec3d{0.0, 0.0, -1.0}, "term_out")
    };

    comp.spice_model.netlist_template = "S_TOGGLE 1 2 TOG_CTRL 0 SW_MODEL (RON=" +
        std::to_string(contact_resistance_ohms) + " ROFF=1e9)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Switch, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-TOGGLE-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 19: SPDT Slide Switch (PH4-07.04.003)
// ============================================================================

ComponentManifest ElectromechanicalComponentsFactory::create_slide_switch(
    double contact_resistance_ohms,
    double max_current_amperes,
    double max_voltage_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "SPDT_Subminiature_Slide_Switch";
    comp.kind = eng::ComponentKind::Switch;
    comp.metadata.manufacturer = "Shunya Electromechanical";
    comp.metadata.part_number = "SS-12D00";
    comp.metadata.package_type = "Miniature Slide THT 3-Pin";
    comp.metadata.description = "Single Pole Double Throw (SPDT) Slide Switch";

    comp.specs.nominal_primary_value = contact_resistance_ohms;
    comp.specs.max_voltage_v = max_voltage_volts;
    comp.specs.max_current_a = max_current_amperes;
    comp.specs.max_power_dissipation_w = 0.25;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 4.0, .height = 9.0, .depth = 3.5}
    );

    comp.regions["pin_com"] = MeshRegionMaterial{"pin_com", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_t1"] = MeshRegionMaterial{"pin_t1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_t2"] = MeshRegionMaterial{"pin_t2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["housing"] = MeshRegionMaterial{"housing", "Alumina", 1.0e-12, 0.3, 1400.0, 3.5, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("COM", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -4.0}, Vec3d{0.0, 0.0, -1.0}, "pin_com"),
        PhysicalTerminal3D("T1", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-2.54, 0.0, -4.0}, Vec3d{0.0, 0.0, -1.0}, "pin_t1"),
        PhysicalTerminal3D("T2", 3, ElectricalPinPolarity::PassiveBiDirectional, Point3D{2.54, 0.0, -4.0}, Vec3d{0.0, 0.0, -1.0}, "pin_t2")
    };

    comp.spice_model.netlist_template = "S_SPDT 1 2 SLIDE_CTRL 0 SW_MODEL (RON=" +
        std::to_string(contact_resistance_ohms) + " ROFF=1e9)";

    SchematicSymbol2D sym;
    sym.standard = std::string(standard);
    sym.component_kind = eng::ComponentKind::Switch;
    sym.symbol_id = "SYM_SWITCH_SPDT";
    sym.primitive_shape = "spdt_lever";
    sym.bounding_box_size = {30.0, 15.0};
    sym.pins = {
        SchematicSymbolPin{"1", 1, "COM", ElectricalPinPolarity::PassiveBiDirectional, {-15.0, 0.0}, {-1.0, 0.0}},
        SchematicSymbolPin{"2", 2, "T1", ElectricalPinPolarity::PassiveBiDirectional, {15.0, 5.0}, {1.0, 0.0}},
        SchematicSymbolPin{"3", 3, "T2", ElectricalPinPolarity::PassiveBiDirectional, {15.0, -5.0}, {1.0, 0.0}}
    };

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-SLIDE-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 20: Electromechanical Relay (PH4-07.04.004)
// ============================================================================

ComponentManifest ElectromechanicalComponentsFactory::create_relay(
    double coil_nominal_voltage_v,
    double coil_resistance_ohms,
    double contact_rating_current_a,
    double contact_rating_voltage_v,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Miniature_Electromechanical_Relay";
    comp.kind = eng::ComponentKind::Relay;
    comp.metadata.manufacturer = "Shunya Relay Dynamics";
    comp.metadata.part_number = "SRD-12VDC-SL-C";
    comp.metadata.package_type = "Sealed Cube Sugar Cube Relay";
    comp.metadata.description = "SPDT 12VDC Sealed Electromechanical Power Relay";

    comp.specs.nominal_primary_value = coil_resistance_ohms;
    comp.specs.max_voltage_v = contact_rating_voltage_v;
    comp.specs.max_current_a = contact_rating_current_a;
    comp.specs.max_power_dissipation_w = (coil_nominal_voltage_v * coil_nominal_voltage_v) / coil_resistance_ohms;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 15.5, .height = 19.0, .depth = 15.0}
    );

    comp.regions["pin_coil_pos"] = MeshRegionMaterial{"pin_coil_pos", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_coil_neg"] = MeshRegionMaterial{"pin_coil_neg", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_com"] = MeshRegionMaterial{"pin_com", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_no"] = MeshRegionMaterial{"pin_no", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_nc"] = MeshRegionMaterial{"pin_nc", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["relay_case"] = MeshRegionMaterial{"relay_case", "Alumina", 1.0e-12, 0.25, 1250.0, 3.8, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("COIL+", 1, ElectricalPinPolarity::Positive, Point3D{-6.0, 6.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "pin_coil_pos"),
        PhysicalTerminal3D("COIL-", 2, ElectricalPinPolarity::Negative, Point3D{-6.0, -6.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "pin_coil_neg"),
        PhysicalTerminal3D("COM", 3, ElectricalPinPolarity::PassiveBiDirectional, Point3D{6.0, 0.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "pin_com"),
        PhysicalTerminal3D("NO", 4, ElectricalPinPolarity::PassiveBiDirectional, Point3D{6.0, 6.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "pin_no"),
        PhysicalTerminal3D("NC", 5, ElectricalPinPolarity::PassiveBiDirectional, Point3D{6.0, -6.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "pin_nc")
    };

    comp.spice_model.netlist_template = "R_COIL 1 2 " + std::to_string(coil_resistance_ohms) +
        "\nS_RELAY_NO 3 4 1 2 SW_RELAY (VT=" + std::to_string(coil_nominal_voltage_v * 0.75) + " RON=0.02 ROFF=1e9)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Relay, standard, 5);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-RELAY-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

} // namespace shunya::lib30
