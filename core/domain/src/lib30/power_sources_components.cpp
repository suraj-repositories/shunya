#include "shunya/lib30/power_sources_components.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/core/base/uuid.hpp"

namespace shunya::lib30 {

using namespace ecad3d;
using namespace cad3d;
using shunya::core::domain::ElectricalPinPolarity;

// ============================================================================
// Component 21: Chemical Battery (PH4-07.05.001)
// ============================================================================

ComponentManifest PowerSourcesProtectionFactory::create_battery_cell(
    double nominal_voltage_volts,
    double capacity_amp_hours,
    double internal_resistance_ohms,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "LiIon_18650_Cell";
    comp.kind = eng::ComponentKind::Battery;
    comp.metadata.manufacturer = "Shunya Energy Storage";
    comp.metadata.part_number = "INR18650-26J";
    comp.metadata.package_type = "Cylindrical 18650 Button Top";
    comp.metadata.description = "Lithium-Ion 18650 Secondary Cell 3.7V 2600mAh";

    comp.specs.nominal_primary_value = nominal_voltage_volts;
    comp.specs.max_voltage_v = 4.20; // Charge cutoff
    comp.specs.max_current_a = capacity_amp_hours * 3.0; // 3C discharge rating
    comp.specs.max_power_dissipation_w = nominal_voltage_volts * comp.specs.max_current_a;

    comp.selected_material_names = {"Nickel", "Aluminum", "Copper"};
    // 18mm diameter (radius 9mm), 65mm height
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 9.0, .height = 65.0, .radial_segments = 24}
    );

    comp.regions["term_pos"] = MeshRegionMaterial{"term_pos", "Nickel", 1.4e7, 90.0, 8900.0, 1.0, 1.0, true, {}};
    comp.regions["term_neg"] = MeshRegionMaterial{"term_neg", "Nickel", 1.4e7, 90.0, 8900.0, 1.0, 1.0, true, {}};
    comp.regions["can_wrap"] = MeshRegionMaterial{"can_wrap", "Aluminum", 1.0e-12, 0.2, 1100.0, 3.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("POS", 1, ElectricalPinPolarity::Positive, Point3D{0.0, 0.0, 33.0}, Vec3d{0.0, 0.0, 1.0}, "term_pos"),
        PhysicalTerminal3D("NEG", 2, ElectricalPinPolarity::Negative, Point3D{0.0, 0.0, -32.5}, Vec3d{0.0, 0.0, -1.0}, "term_neg")
    };

    comp.spice_model.netlist_template = "V_CELL 1_INT 2 DC " + std::to_string(nominal_voltage_volts) +
        "\nR_INT 1 1_INT " + std::to_string(internal_resistance_ohms);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Battery, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-BAT-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 22: Ideal DC Voltage Source (PH4-07.05.002)
// ============================================================================

ComponentManifest PowerSourcesProtectionFactory::create_dc_voltage_source(
    double setpoint_voltage_volts,
    double current_limit_amperes,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "DC_Voltage_Source";
    comp.kind = eng::ComponentKind::DCVoltageSource;
    comp.metadata.manufacturer = "Shunya Instrumentation";
    comp.metadata.part_number = "PSU-DC-12V";
    comp.metadata.package_type = "Benchtop Supply Terminal Post";
    comp.metadata.description = "Regulated Precision DC Voltage Source";

    comp.specs.nominal_primary_value = setpoint_voltage_volts;
    comp.specs.max_voltage_v = setpoint_voltage_volts;
    comp.specs.max_current_a = current_limit_amperes;
    comp.specs.max_power_dissipation_w = setpoint_voltage_volts * current_limit_amperes;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 25.0, .height = 25.0, .depth = 15.0}
    );

    comp.regions["term_pos"] = MeshRegionMaterial{"term_pos", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["term_neg"] = MeshRegionMaterial{"term_neg", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["chassis"] = MeshRegionMaterial{"chassis", "Alumina", 1.0e-12, 1.0, 2200.0, 3.5, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("POS", 1, ElectricalPinPolarity::Positive, Point3D{-7.0, 0.0, 8.0}, Vec3d{0.0, 0.0, 1.0}, "term_pos"),
        PhysicalTerminal3D("NEG", 2, ElectricalPinPolarity::Negative, Point3D{7.0, 0.0, 8.0}, Vec3d{0.0, 0.0, 1.0}, "term_neg")
    };

    comp.spice_model.netlist_template = "V_DC 1 2 DC " + std::to_string(setpoint_voltage_volts);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::DCVoltageSource, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-DCSRC-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 23: AC Voltage Source (PH4-07.05.003)
// ============================================================================

ComponentManifest PowerSourcesProtectionFactory::create_ac_voltage_source(
    double peak_voltage_volts,
    double frequency_hz,
    double phase_degrees,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "AC_Sinusoidal_Source";
    comp.kind = eng::ComponentKind::ACVoltageSource;
    comp.metadata.manufacturer = "Shunya Instrumentation";
    comp.metadata.part_number = "GEN-AC-SINE";
    comp.metadata.package_type = "BNC Signal Jack";
    comp.metadata.description = "Sinusoidal AC Generator Source " + std::to_string(frequency_hz) + " Hz";

    comp.specs.nominal_primary_value = peak_voltage_volts;
    comp.specs.max_voltage_v = peak_voltage_volts;
    comp.specs.max_current_a = 5.0;
    comp.specs.max_power_dissipation_w = 0.5 * peak_voltage_volts * 5.0;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 6.0, .height = 14.0, .radial_segments = 16}
    );

    comp.regions["term_live"] = MeshRegionMaterial{"term_live", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["term_neutral"] = MeshRegionMaterial{"term_neutral", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["bnc_shell"] = MeshRegionMaterial{"bnc_shell", "Alumina", 1.0e-12, 1.0, 2200.0, 3.5, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("LIVE", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, 7.0}, Vec3d{0.0, 0.0, 1.0}, "term_live"),
        PhysicalTerminal3D("NEUTRAL", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 4.0, 0.0}, Vec3d{0.0, 1.0, 0.0}, "term_neutral")
    };

    comp.spice_model.netlist_template = "V_AC 1 2 SIN(0 " + std::to_string(peak_voltage_volts) + " " +
        std::to_string(frequency_hz) + " 0 0 " + std::to_string(phase_degrees) + ")";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::ACVoltageSource, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-ACSRC-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 24: Earth / Ground Reference (PH4-07.05.004)
// ============================================================================

ComponentManifest PowerSourcesProtectionFactory::create_ground_reference(
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Earth_Ground_Reference";
    comp.kind = eng::ComponentKind::GroundReference;
    comp.metadata.manufacturer = "Shunya Reference Systems";
    comp.metadata.part_number = "GND-STUD-M4";
    comp.metadata.package_type = "M4 Chassis Grounding Lug";
    comp.metadata.description = "Zero-Potential Earth / Circuit Ground Reference Node";

    comp.specs.nominal_primary_value = 0.0; // 0.0 Volts potential
    comp.specs.max_voltage_v = 0.0;
    comp.specs.max_current_a = 100.0; // Fault current sink capacity
    comp.specs.max_power_dissipation_w = 100.0;

    comp.selected_material_names = {"Copper"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 4.0, .height = 10.0, .radial_segments = 16}
    );

    comp.regions["gnd_stud"] = MeshRegionMaterial{"gnd_stud", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};

    comp.terminals = {
        PhysicalTerminal3D("GND", 1, ElectricalPinPolarity::Negative, Point3D{0.0, 0.0, 5.0}, Vec3d{0.0, 0.0, 1.0}, "gnd_stud")
    };

    comp.spice_model.netlist_template = "* NODE 1 TIED DIRECTLY TO GROUND 0\nV_GND 1 0 0.0";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::GroundReference, standard, 1);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-GND-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 25: Fast-Acting Fuse (PH4-07.05.005)
// ============================================================================

ComponentManifest PowerSourcesProtectionFactory::create_fuse(
    double rated_current_amperes,
    double cold_resistance_ohms,
    double max_voltage_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Fast_Acting_Glass_Fuse_5x20";
    comp.kind = eng::ComponentKind::Fuse;
    comp.metadata.manufacturer = "Shunya Protection Systems";
    comp.metadata.part_number = "FUSE-5X20-" + std::to_string(static_cast<int>(rated_current_amperes)) + "A";
    comp.metadata.package_type = "5x20mm Glass Cartridge Cartridge";
    comp.metadata.description = "Fast-Acting Over-Current Cartridge Fuse " + std::to_string(rated_current_amperes) + " A";

    comp.specs.nominal_primary_value = rated_current_amperes;
    comp.specs.max_voltage_v = max_voltage_volts;
    comp.specs.max_current_a = rated_current_amperes;
    comp.specs.max_power_dissipation_w = rated_current_amperes * rated_current_amperes * cold_resistance_ohms;

    comp.selected_material_names = {"Copper", "Alumina"};
    // 5mm diameter (radius 2.5mm), 20mm length
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 2.5, .height = 20.0, .radial_segments = 16}
    );

    comp.regions["cap_1"] = MeshRegionMaterial{"cap_1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["cap_2"] = MeshRegionMaterial{"cap_2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["glass_tube"] = MeshRegionMaterial{"glass_tube", "Alumina", 1.0e-12, 1.1, 2230.0, 4.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "cap_1"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, 10.0}, Vec3d{0.0, 0.0, 1.0}, "cap_2")
    };

    comp.spice_model.netlist_template = "R_FUSE 1 2 " + std::to_string(cold_resistance_ohms) +
        " ; BLOW_CURRENT=" + std::to_string(rated_current_amperes) + "A";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Fuse, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-FUSE-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

} // namespace shunya::lib30
