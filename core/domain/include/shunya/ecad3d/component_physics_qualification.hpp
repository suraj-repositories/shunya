#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/ecad3d/component_designer_workspace.hpp"
#include "shunya/ecad3d/symbol_association_engine.hpp"
#include "shunya/matdb/element_database.hpp"

#include <string>
#include <vector>

namespace shunya::ecad3d {

/**
 * @brief Multi-region material assignment validation report.
 * (Roadmap task PH4-06.03.001)
 */
struct MaterialAssignmentReport {
    bool is_valid{true};
    std::vector<std::string> violations;
    std::vector<std::string> warnings;
};

/**
 * @brief Automated electrical continuity & isolation check report.
 * (Roadmap task PH4-06.03.002)
 */
struct ContinuityReport {
    enum class Status {
        Continuous,
        OpenCircuit,
        ShortCircuitIsolationBreach,
        DegradedPath
    };
    Status status{Status::Continuous};
    double path_length_mm{0.0};
    double estimated_dc_conductance_siemens{0.0};
    std::vector<std::string> diagnostic_notes;
};

/**
 * @brief Geometric parameters for an individual lead/terminal contact.
 * (Roadmap task PH4-06.03.003)
 */
struct LeadGeometricSpec {
    double length_mm{1.0};
    double cross_section_area_mm2{0.2};
    double material_resistivity_ohm_m{1.68e-8}; // Copper by default
};

/**
 * @brief ESR derivation result from lead geometry and internal path resistivity.
 * (Roadmap task PH4-06.03.003)
 */
struct ESRCalculationResult {
    double terminal_leads_resistance_ohms{0.0};
    double internal_element_resistance_ohms{0.0};
    double total_esr_ohms{0.0};
    double esr_at_100khz_ohms{0.0};
};

/**
 * @brief Physical mass, center of gravity, and 3D inertia tensor derived via Steiner's theorem.
 * (Roadmap task PH4-06.03.004)
 */
struct MassPropertiesResult {
    double total_mass_kg{0.0};
    cad3d::Point3D center_of_gravity_mm{0.0, 0.0, 0.0};
    double inertia_tensor_kg_m2[3][3]{{0.0}};
};

/**
 * @brief Environmental and thermal specification for power dissipation derivation.
 * (Roadmap task PH4-06.03.005)
 */
struct ThermalDissipationSpec {
    double ambient_temp_c{25.0};
    double max_operating_temp_c{125.0};
    double convective_htc_w_m2k{15.0}; // Natural convection in air: ~15-25 W/(m^2*K)
    double surface_emissivity{0.85};   // Typical epoxy/ceramic package
};

/**
 * @brief Thermal power rating calculation results.
 * (Roadmap task PH4-06.03.005)
 */
struct ThermalRatingResult {
    double total_surface_area_mm2{0.0};
    double max_continuous_power_watts{0.0};
    double thermal_resistance_deg_c_per_watt{0.0};
    double derated_power_at_70c_watts{0.0};
};

/**
 * @brief Multi-Region Material Physics Engine for ECAD 3D Components.
 */
class ComponentPhysicsEngine {
public:
    // PH4-06.03.001: Multi-region material assignment validation (sigma > 1e5 for terminals)
    [[nodiscard]] static MaterialAssignmentReport validate_material_assignments(
        const ComponentAuthoringData& component,
        const matdb::ElementDatabase* elem_db = nullptr
    );

    // PH4-06.03.002: Automated electrical continuity checks between terminals
    [[nodiscard]] static ContinuityReport check_electrical_continuity(
        const ComponentAuthoringData& component,
        int terminal_a_idx = 0,
        int terminal_b_idx = 1
    );

    // PH4-06.03.003: Equivalent series resistance (ESR) calculation
    [[nodiscard]] static ESRCalculationResult calculate_esr(
        const ComponentAuthoringData& component,
        double internal_element_r = 0.0,
        const std::vector<LeadGeometricSpec>& custom_leads = {}
    );

    // PH4-06.03.004: Total mass, center of gravity, and 3D inertia tensor
    [[nodiscard]] static MassPropertiesResult calculate_mass_properties(
        const ComponentAuthoringData& component
    );

    // PH4-06.03.005: Thermal dissipation rating calculations
    [[nodiscard]] static ThermalRatingResult calculate_thermal_dissipation(
        const ComponentAuthoringData& component,
        const ThermalDissipationSpec& spec = {}
    );
};

/**
 * @brief Automated Component Qualification Inspector.
 * (Roadmap task PH4-06.03.006)
 */
class ComponentQualificationInspector {
public:
    struct QualificationCertificate {
        bool is_qualified{false};
        std::string component_id;
        std::string certification_id;
        std::vector<std::string> critical_failures;
        std::vector<std::string> warnings;
        double evaluated_esr_ohms{0.0};
        double evaluated_max_power_w{0.0};
        double evaluated_mass_grams{0.0};
    };

    [[nodiscard]] static QualificationCertificate inspect_and_qualify(
        const ComponentAuthoringData& component,
        const SchematicSymbol2D& symbol,
        const PinTerminalMapper& mapper,
        const matdb::ElementDatabase* elem_db = nullptr
    );
};

} // namespace shunya::ecad3d
