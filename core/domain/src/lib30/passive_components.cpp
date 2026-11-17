#include "shunya/lib30/passive_components.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/core/base/uuid.hpp"

namespace shunya::lib30 {

using namespace ecad3d;
using namespace cad3d;
using shunya::core::domain::ElectricalPinPolarity;

// ============================================================================
// Component 1: Resistor (PH4-07.01.001)
// ============================================================================

ComponentManifest PassiveComponentsFactory::create_resistor(
    double resistance_ohms,
    double tolerance_percent,
    double power_rating_watts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Standard_Resistor";
    comp.kind = eng::ComponentKind::Resistor;
    comp.metadata.manufacturer = "Shunya Reference Precision";
    comp.metadata.part_number = "SHUNYA-RES-" + std::to_string(static_cast<int>(resistance_ohms)) + "R";
    comp.metadata.package_type = "Axial Through-Hole";
    comp.metadata.description = "Fixed Through-Hole Resistor " + std::to_string(resistance_ohms) + " Ohm";

    comp.specs.nominal_primary_value = resistance_ohms;
    comp.specs.tolerance_percent = tolerance_percent;
    comp.specs.max_power_dissipation_w = power_rating_watts;
    comp.specs.max_voltage_v = std::sqrt(power_rating_watts * resistance_ohms);
    comp.specs.max_current_a = std::sqrt(power_rating_watts / std::max(1.0e-3, resistance_ohms));

    comp.selected_material_names = {"Copper", "Alumina", "Nichrome"};
    // Cylinder body: radius 1.2mm, height 6.0mm
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 1.2, .height = 6.0, .radial_segments = 16}
    );

    comp.regions["lead_1"] = MeshRegionMaterial{"lead_1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_2"] = MeshRegionMaterial{"lead_2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["body"] = MeshRegionMaterial{"body", "Alumina", 1.0e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -4.0}, Vec3d{0.0, 0.0, -1.0}, "lead_1"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, 4.0}, Vec3d{0.0, 0.0, 1.0}, "lead_2")
    };

    comp.spice_model.netlist_template = "R1 1 2 " + std::to_string(resistance_ohms);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Resistor, standard);
    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-RES-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 2: Variable Resistor / Rheostat (PH4-07.01.002)
// ============================================================================

ComponentManifest PassiveComponentsFactory::create_rheostat(
    double r_min_ohms,
    double r_max_ohms,
    double wiper_position_ratio,
    double power_rating_watts
) {
    ComponentAuthoringData comp;
    comp.name = "Rheostat_Wirewound";
    comp.kind = eng::ComponentKind::Resistor;
    comp.metadata.manufacturer = "Shunya High Power";
    comp.metadata.part_number = "SHUNYA-RHEO-" + std::to_string(static_cast<int>(r_max_ohms)) + "R";
    comp.metadata.package_type = "Tubular Ceramic Slider";
    comp.metadata.description = "Heavy Duty Slide Rheostat 0-" + std::to_string(r_max_ohms) + " Ohm";

    double effective_r = r_min_ohms + wiper_position_ratio * (r_max_ohms - r_min_ohms);
    comp.specs.nominal_primary_value = effective_r;
    comp.specs.tolerance_percent = 10.0;
    comp.specs.max_power_dissipation_w = power_rating_watts;

    comp.selected_material_names = {"Nichrome", "Brass", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 15.0, .height = 100.0, .radial_segments = 24}
    );

    comp.regions["terminal_base"] = MeshRegionMaterial{"terminal_base", "Brass", 1.6e7, 115.0, 8500.0, 1.0, 1.0, true, {}};
    comp.regions["wiper_slider"] = MeshRegionMaterial{"wiper_slider", "Brass", 1.6e7, 115.0, 8500.0, 1.0, 1.0, true, {}};
    comp.regions["tube_ceramic"] = MeshRegionMaterial{"tube_ceramic", "Alumina", 1.0e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -50.0}, Vec3d{0.0, 0.0, -1.0}, "terminal_base"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{15.0, 0.0, 50.0 * (wiper_position_ratio * 2.0 - 1.0)}, Vec3d{1.0, 0.0, 0.0}, "wiper_slider")
    };

    comp.spice_model.netlist_template = "R_RHEO 1 2 " + std::to_string(effective_r);

    SchematicSymbol2D sym;
    sym.symbol_id = "SYM_RHEOSTAT";
    sym.standard = "IEEE";
    sym.component_kind = eng::ComponentKind::Resistor;
    sym.primitive_shape = "adjustable_resistor";
    sym.pins = {
        SchematicSymbolPin("1", 1, "1", ElectricalPinPolarity::PassiveBiDirectional, {-15.0, 0.0}, {-1.0, 0.0}),
        SchematicSymbolPin("2", 2, "W", ElectricalPinPolarity::PassiveBiDirectional, {15.0, 0.0}, {1.0, 0.0})
    };

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-RHEO-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 3: Potentiometer (PH4-07.01.003)
// ============================================================================

ComponentManifest PassiveComponentsFactory::create_potentiometer(
    double total_resistance_ohms,
    double wiper_ratio,
    double power_rating_watts
) {
    ComponentAuthoringData comp;
    comp.name = "Rotary_Potentiometer";
    comp.kind = eng::ComponentKind::Resistor;
    comp.metadata.manufacturer = "Shunya Controls";
    comp.metadata.part_number = "SHUNYA-POT-" + std::to_string(static_cast<int>(total_resistance_ohms)) + "R";
    comp.metadata.package_type = "Rotary 16mm Panel Mount";
    comp.metadata.description = "Linear Taper Rotary Potentiometer " + std::to_string(total_resistance_ohms) + " Ohm";

    comp.specs.nominal_primary_value = total_resistance_ohms;
    comp.specs.tolerance_percent = 10.0;
    comp.specs.max_power_dissipation_w = power_rating_watts;

    comp.selected_material_names = {"Copper", "Carbon", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 8.0, .height = 10.0, .radial_segments = 24}
    );

    comp.regions["pin_1"] = MeshRegionMaterial{"pin_1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_2"] = MeshRegionMaterial{"pin_2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["pin_3"] = MeshRegionMaterial{"pin_3", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["housing"] = MeshRegionMaterial{"housing", "Alumina", 1.0e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-4.0, -8.0, -5.0}, Vec3d{0.0, -1.0, 0.0}, "pin_1"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, -8.0, -5.0}, Vec3d{0.0, -1.0, 0.0}, "pin_2"),
        PhysicalTerminal3D("T3", 3, ElectricalPinPolarity::PassiveBiDirectional, Point3D{4.0, -8.0, -5.0}, Vec3d{0.0, -1.0, 0.0}, "pin_3")
    };

    double r1 = total_resistance_ohms * wiper_ratio;
    double r2 = total_resistance_ohms * (1.0 - wiper_ratio);
    comp.spice_model.netlist_template = "R1 1 2 " + std::to_string(r1) + " \n R2 2 3 " + std::to_string(r2);

    SchematicSymbol2D sym;
    sym.symbol_id = "SYM_POTENTIOMETER";
    sym.standard = "IEEE";
    sym.component_kind = eng::ComponentKind::Resistor;
    sym.primitive_shape = "potentiometer_3pin";
    sym.pins = {
        SchematicSymbolPin("1", 1, "1", ElectricalPinPolarity::PassiveBiDirectional, {-15.0, 0.0}, {-1.0, 0.0}),
        SchematicSymbolPin("2", 2, "W", ElectricalPinPolarity::PassiveBiDirectional, {0.0, 15.0}, {0.0, 1.0}),
        SchematicSymbolPin("3", 3, "2", ElectricalPinPolarity::PassiveBiDirectional, {15.0, 0.0}, {1.0, 0.0})
    };

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-POT-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 4: Ceramic Capacitor (PH4-07.01.004)
// ============================================================================

ComponentManifest PassiveComponentsFactory::create_ceramic_capacitor(
    double capacitance_farads,
    double max_voltage_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Ceramic_Disc_Capacitor";
    comp.kind = eng::ComponentKind::Capacitor;
    comp.metadata.manufacturer = "Shunya Dielectrics";
    comp.metadata.part_number = "SHUNYA-CCAP-" + std::to_string(static_cast<int>(capacitance_farads * 1e9)) + "N";
    comp.metadata.package_type = "Radial Disc";
    comp.metadata.description = "Ceramic Disc Capacitor " + std::to_string(capacitance_farads * 1e9) + " nF " + std::to_string(static_cast<int>(max_voltage_volts)) + "V";

    comp.specs.nominal_primary_value = capacitance_farads;
    comp.specs.tolerance_percent = 10.0;
    comp.specs.max_voltage_v = max_voltage_volts;

    comp.selected_material_names = {"Copper", "Silver", "Alumina"};
    // Disc cylinder: radius 3.5mm, thickness 1.5mm
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 3.5, .height = 1.5, .radial_segments = 24}
    );

    comp.regions["lead_1"] = MeshRegionMaterial{"lead_1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_2"] = MeshRegionMaterial{"lead_2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["dielectric"] = MeshRegionMaterial{"dielectric", "Alumina", 1.0e-12, 30.0, 3950.0, 1200.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-1.25, 0.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "lead_1"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{1.25, 0.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "lead_2")
    };

    comp.spice_model.netlist_template = "C1 1 2 " + std::to_string(capacitance_farads);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Capacitor, standard);
    // Non-polarized ceramic pins are passive bidirectional
    sym.pins[0].polarity = ElectricalPinPolarity::PassiveBiDirectional;
    sym.pins[1].polarity = ElectricalPinPolarity::PassiveBiDirectional;

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-CCAP-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 5: Polarized Electrolytic Capacitor (PH4-07.01.005)
// ============================================================================

ComponentManifest PassiveComponentsFactory::create_electrolytic_capacitor(
    double capacitance_farads,
    double max_voltage_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Aluminum_Electrolytic_Capacitor";
    comp.kind = eng::ComponentKind::Capacitor;
    comp.metadata.manufacturer = "Shunya Electro-Chemical";
    comp.metadata.part_number = "SHUNYA-ECAP-" + std::to_string(static_cast<int>(capacitance_farads * 1e6)) + "U";
    comp.metadata.package_type = "Radial Can";
    comp.metadata.description = "Polarized Aluminum Electrolytic Capacitor " + std::to_string(capacitance_farads * 1e6) + " uF " + std::to_string(static_cast<int>(max_voltage_volts)) + "V";

    comp.specs.nominal_primary_value = capacitance_farads;
    comp.specs.tolerance_percent = 20.0;
    comp.specs.max_voltage_v = max_voltage_volts;

    comp.selected_material_names = {"Aluminum", "Copper", "Alumina"};
    // Radial can cylinder: radius 4.0mm, height 11.0mm
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 4.0, .height = 11.0, .radial_segments = 24}
    );

    comp.regions["lead_pos"] = MeshRegionMaterial{"lead_pos", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_neg"] = MeshRegionMaterial{"lead_neg", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["can_body"] = MeshRegionMaterial{"can_body", "Alumina", 1.0e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    // Polarized pins: Pin 1 is Positive (longer lead), Pin 2 is Negative
    comp.terminals = {
        PhysicalTerminal3D("T_POS", 1, ElectricalPinPolarity::Positive, Point3D{-1.5, 0.0, -15.0}, Vec3d{0.0, 0.0, -1.0}, "lead_pos"),
        PhysicalTerminal3D("T_NEG", 2, ElectricalPinPolarity::Negative, Point3D{1.5, 0.0, -12.0}, Vec3d{0.0, 0.0, -1.0}, "lead_neg")
    };

    comp.spice_model.netlist_template = "C_ELEC 1 2 " + std::to_string(capacitance_farads);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Capacitor, standard);
    sym.pins[0].polarity = ElectricalPinPolarity::Positive;
    sym.pins[0].pin_label = "+";
    sym.pins[1].polarity = ElectricalPinPolarity::Negative;
    sym.pins[1].pin_label = "-";

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-ECAP-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

} // namespace shunya::lib30
