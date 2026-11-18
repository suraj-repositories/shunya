#include "shunya/lib30/ic_actuator_components.hpp"
#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/ecad3d/component_package.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::lib30 {

using namespace ecad3d;
using namespace cad3d;
using namespace core::domain;

// =============================================================================
// Component 28: Piezoelectric Buzzer
// =============================================================================

double ActuatorsTransducersFactory::evaluate_spl_output(
    double operating_voltage_v,
    double rated_voltage_v,
    double resonant_frequency_hz,
    double actual_frequency_hz
) noexcept {
    if (operating_voltage_v <= 0.1 || rated_voltage_v <= 0.0) return 0.0;
    const double v_ratio = std::clamp(operating_voltage_v / rated_voltage_v, 0.01, 2.0);
    const double delta_f = std::abs(actual_frequency_hz - resonant_frequency_hz);
    const double bandwidth = std::max(10.0, resonant_frequency_hz * 0.10); // Q ~ 10
    const double freq_attenuation_db = 10.0 * std::log10(1.0 + (delta_f / bandwidth) * (delta_f / bandwidth));
    constexpr double kNominalSpl = 85.0; // dB at 10cm at rated voltage and resonant frequency
    const double spl = kNominalSpl + 20.0 * std::log10(v_ratio) - freq_attenuation_db;
    return std::max(0.0, spl);
}

ComponentManifest ActuatorsTransducersFactory::create_piezo_buzzer(
    double resonant_frequency_hz,
    double rated_voltage_volts,
    double sound_pressure_level_db,
    std::string_view standard
) {
    (void)sound_pressure_level_db;
    ComponentAuthoringData comp;
    comp.name = "Piezoelectric_Acoustic_Buzzer_12mm";
    comp.kind = eng::ComponentKind::PiezoBuzzer;
    comp.metadata.manufacturer = "Shunya Acoustic Engineering";
    comp.metadata.part_number = "PIEZO-BUZ-12MM-4KHZ";
    comp.metadata.package_type = "12mm Cylindrical Cavity Through-Hole";
    comp.metadata.description = "Resonant Piezoelectric Transducer Buzzer 4kHz 85dB @ 10cm";

    comp.specs.nominal_primary_value = resonant_frequency_hz;
    comp.specs.max_voltage_v = rated_voltage_volts * 1.5;
    comp.specs.max_current_a = 0.030; // 30mA max
    comp.specs.max_power_dissipation_w = comp.specs.max_voltage_v * comp.specs.max_current_a;

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 6.0, .height = 7.5, .radial_segments = 24}
    );

    comp.regions["piezo_element"] = MeshRegionMaterial{"piezo_element", "Alumina", 1.0e-9, 2.0, 7500.0, 1500.0, 1.0, false, {}};
    comp.regions["housing"] = MeshRegionMaterial{"housing", "Alumina", 1.0e-12, 0.2, 1050.0, 3.0, 1.0, false, {}};
    comp.regions["leads"] = MeshRegionMaterial{"leads", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::Positive, Point3D{-2.5, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "leads"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::Negative, Point3D{2.5, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "leads")
    };

    comp.spice_model.netlist_template = "B_BUZZER 1 2 V=I(V_SENSE)*16.0 ; F_RES=" +
        std::to_string(resonant_frequency_hz) + "Hz V_RATED=" + std::to_string(rated_voltage_volts);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::PiezoBuzzer, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-BUZ-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// =============================================================================
// Component 29: DC Permanent Magnet Motor
// =============================================================================

double ActuatorsTransducersFactory::calculate_motor_stall_torque(
    double applied_voltage_v,
    double armature_resistance_ohms,
    double torque_constant_nm_per_a
) noexcept {
    if (armature_resistance_ohms <= 1e-6) return 0.0;
    const double stall_current = applied_voltage_v / armature_resistance_ohms;
    return stall_current * torque_constant_nm_per_a;
}

double ActuatorsTransducersFactory::calculate_motor_no_load_speed_rad_s(
    double applied_voltage_v,
    double back_emf_constant_v_per_rad_s
) noexcept {
    if (back_emf_constant_v_per_rad_s <= 1e-6) return 0.0;
    return applied_voltage_v / back_emf_constant_v_per_rad_s;
}

ComponentManifest ActuatorsTransducersFactory::create_dc_motor(
    double rated_voltage_volts,
    double armature_resistance_ohms,
    double armature_inductance_henries,
    double torque_constant_nm_per_a,
    double back_emf_constant_v_per_rad_s,
    std::string_view standard
) {
    (void)armature_inductance_henries;
    ComponentAuthoringData comp;
    comp.name = "DC_Permanent_Magnet_Motor_RS380";
    comp.kind = eng::ComponentKind::DCMotor;
    comp.metadata.manufacturer = "Shunya Motion Drives";
    comp.metadata.part_number = "MOTOR-DC-RS380-12V";
    comp.metadata.package_type = "RS380 Cylindrical Can with 2.3mm Shaft";
    comp.metadata.description = "Permanent Magnet Brushed DC Motor 12V Ra=1.5 Ohm Kt=0.025 Nm/A";

    comp.specs.nominal_primary_value = rated_voltage_volts;
    comp.specs.max_voltage_v = rated_voltage_volts * 1.5;
    comp.specs.max_current_a = rated_voltage_volts / std::max(1e-3, armature_resistance_ohms); // Stall current
    comp.specs.max_power_dissipation_w = 50.0; // 50W continuous dissipation

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 14.0, .height = 38.0, .radial_segments = 24}
    );

    comp.regions["rotor_windings"] = MeshRegionMaterial{"rotor_windings", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["stator_case"] = MeshRegionMaterial{"stator_case", "Alumina", 1.0e-3, 50.0, 7850.0, 1000.0, 1.0, false, {}};
    comp.regions["terminals"] = MeshRegionMaterial{"terminals", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::Positive, Point3D{-8.0, 0.0, -20.0}, Vec3d{0.0, 0.0, -1.0}, "terminals"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::Negative, Point3D{8.0, 0.0, -20.0}, Vec3d{0.0, 0.0, -1.0}, "terminals")
    };

    comp.spice_model.netlist_template = "R_A 1 3 " + std::to_string(armature_resistance_ohms) +
        "\nL_A 3 4 " + std::to_string(armature_inductance_henries) +
        "\nB_EMF 4 2 V=V(SHAFT_OMEGA)*" + std::to_string(back_emf_constant_v_per_rad_s) +
        "\nB_TORQUE SHAFT_TORQUE 0 I=I(B_EMF)*" + std::to_string(torque_constant_nm_per_a);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::DCMotor, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-MOTOR-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// =============================================================================
// Component 30: Operational Amplifier (LM741 / Rail-to-Rail)
// =============================================================================

double ActuatorsTransducersFactory::evaluate_opamp_output(
    double v_pos_in,
    double v_neg_in,
    double open_loop_gain,
    double v_pos_rail,
    double v_neg_rail
) noexcept {
    const double v_diff = v_pos_in - v_neg_in;
    const double v_raw = v_diff * open_loop_gain;
    return std::clamp(v_raw, v_neg_rail, v_pos_rail);
}

ComponentManifest ActuatorsTransducersFactory::create_opamp(
    double open_loop_gain,
    double input_resistance_ohms,
    double output_resistance_ohms,
    double positive_rail_volts,
    double negative_rail_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Operational_Amplifier_LM741_DIP8";
    comp.kind = eng::ComponentKind::OpAmp;
    comp.metadata.manufacturer = "Shunya Linear Microelectronics";
    comp.metadata.part_number = "OPAMP-LM741-DIP8";
    comp.metadata.package_type = "DIP-8 Dual In-Line Plastic";
    comp.metadata.description = "General Purpose Operational Amplifier AOL=1e5 Rin=2M Rout=75";

    comp.specs.nominal_primary_value = open_loop_gain;
    comp.specs.max_voltage_v = (positive_rail_volts - negative_rail_volts);
    comp.specs.max_current_a = 0.040; // 40mA short-circuit output limit
    comp.specs.max_power_dissipation_w = 0.50; // 500mW DIP package limit

    comp.selected_material_names = {"Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 10.0, .height = 6.0, .depth = 3.5}
    );

    comp.regions["silicon_die"] = MeshRegionMaterial{"silicon_die", "Alumina", 1.0e-3, 150.0, 2330.0, 11.7, 1.0, false, {}};
    comp.regions["epoxy_body"] = MeshRegionMaterial{"epoxy_body", "Alumina", 1.0e-12, 0.3, 1800.0, 3.8, 1.0, false, {}};
    comp.regions["leads"] = MeshRegionMaterial{"leads", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-3.81, -4.0, -2.0}, Vec3d{0.0, -1.0, 0.0}, "leads"), // IN-
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-1.27, -4.0, -2.0}, Vec3d{0.0, -1.0, 0.0}, "leads"), // IN+
        PhysicalTerminal3D("T3", 3, ElectricalPinPolarity::Positive,             Point3D{3.81, 4.0, -2.0},  Vec3d{0.0, 1.0, 0.0}, "leads"),  // OUT
        PhysicalTerminal3D("T4", 4, ElectricalPinPolarity::Positive,             Point3D{1.27, 4.0, -2.0},  Vec3d{0.0, 1.0, 0.0}, "leads"),  // V+
        PhysicalTerminal3D("T5", 5, ElectricalPinPolarity::Negative,             Point3D{1.27, -4.0, -2.0}, Vec3d{0.0, -1.0, 0.0}, "leads")  // V-
    };

    comp.spice_model.netlist_template = "R_IN 2 1 " + std::to_string(input_resistance_ohms) +
        "\nE_GAIN 6 0 2 1 " + std::to_string(open_loop_gain) +
        "\nR_OUT 6 3 " + std::to_string(output_resistance_ohms);

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::OpAmp, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-OPAMP-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

} // namespace shunya::lib30
