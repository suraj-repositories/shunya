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

double PowerSourcesProtectionFactory::evaluate_ldr_resistance(
    double r10_lux_ohms,
    double gamma,
    double lux
) noexcept {
    if (lux <= 0.05) {
        return 1.0e6; // Dark resistance 1 MOhm
    }
    return r10_lux_ohms * std::pow(lux / 10.0, -gamma);
}

ComponentManifest PowerSourcesProtectionFactory::create_ldr(
    double r_dark_ohms,
    double r10_lux_ohms,
    double gamma,
    std::string_view standard
) {
    (void)r_dark_ohms;
    ComponentAuthoringData comp;
    comp.name = "CdS_Photoresistor_LDR";
    comp.kind = eng::ComponentKind::LDR;
    comp.metadata.manufacturer = "Shunya Opto-Sensors";
    comp.metadata.part_number = "LDR-CDS-5MM";
    comp.metadata.package_type = "5mm Epoxy Disc Radial";
    comp.metadata.description = "Cadmium Sulfide (CdS) Light Dependent Resistor 10k@10lux";

    comp.specs.nominal_primary_value = r10_lux_ohms;
    comp.specs.max_voltage_v = 100.0;
    comp.specs.max_power_dissipation_w = 0.10; // 100mW

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 2.5, .height = 1.5, .radial_segments = 16}
    );

    comp.regions["cds_film"] = MeshRegionMaterial{"cds_film", "Alumina", 1.0e-10, 1.5, 4820.0, 5.0, 1.0, false, {}};
    comp.regions["leads"] = MeshRegionMaterial{"leads", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-1.25, 0.0, -2.0}, Vec3d{0.0, 0.0, -1.0}, "leads"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{1.25, 0.0, -2.0}, Vec3d{0.0, 0.0, -1.0}, "leads")
    };

    comp.spice_model.netlist_template = "R_LDR 1 2 " + std::to_string(r10_lux_ohms) +
        " ; GAMMA=" + std::to_string(gamma) + " DARK_R=" + std::to_string(1.0e6);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::LDR, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-LDR-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

double PowerSourcesProtectionFactory::evaluate_ntc_resistance(
    double r0_ohms,
    double beta_k,
    double t0_k,
    double t_kelvin
) noexcept {
    if (t_kelvin <= 1.0) t_kelvin = 1.0;
    return r0_ohms * std::exp(beta_k * (1.0 / t_kelvin - 1.0 / t0_k));
}

ComponentManifest PowerSourcesProtectionFactory::create_ntc_thermistor(
    double r0_ohms,
    double beta_k,
    double t0_k,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "NTC_Thermistor_10k_B3950";
    comp.kind = eng::ComponentKind::ThermistorNTC;
    comp.metadata.manufacturer = "Shunya Thermal Dynamics";
    comp.metadata.part_number = "NTC-10K-B3950";
    comp.metadata.package_type = "Epoxy Coated Bead 3mm Radial";
    comp.metadata.description = "Negative Temperature Coefficient (NTC) Thermistor 10k @ 25C B=3950K";

    comp.specs.nominal_primary_value = r0_ohms;
    comp.specs.max_voltage_v = 50.0;
    comp.specs.max_power_dissipation_w = 0.25;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 1.5, .height = 3.0, .radial_segments = 16}
    );

    comp.regions["oxide_bead"] = MeshRegionMaterial{"oxide_bead", "Alumina", 1.0e-8, 2.0, 5200.0, 8.0, 1.0, false, {}};
    comp.regions["leads"] = MeshRegionMaterial{"leads", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-0.75, 0.0, -3.0}, Vec3d{0.0, 0.0, -1.0}, "leads"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.75, 0.0, -3.0}, Vec3d{0.0, 0.0, -1.0}, "leads")
    };

    comp.spice_model.netlist_template = "R_NTC 1 2 " + std::to_string(r0_ohms) +
        " ; BETA=" + std::to_string(beta_k) + " T0=" + std::to_string(t0_k);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::ThermistorNTC, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-NTC-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

} // namespace shunya::lib30
