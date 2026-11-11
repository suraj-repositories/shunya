#pragma once

#include "shunya/chem/battery_cell_model.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/component_generator.hpp"
#include "shunya/eng/physical_object.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace shunya::eng {

struct BatteryGeometryConfig {
    double footprint_area_m2{0.02};          // e.g. 200 cm^2 pouch or jellyroll unrolled area
    int stacked_layers{12};
    double anode_thickness_m{80e-6};
    double cathode_thickness_m{90e-6};
    double separator_thickness_m{25e-6};
    double electrolyte_thickness_m{25e-6};
    double anode_collector_thickness_m{12e-6};
    double cathode_collector_thickness_m{15e-6};
    double casing_thickness_m{0.5e-3};
    double terminal_area_m2{1e-4};
    double terminal_length_m{0.02};
    double active_mass_fraction{0.85};       // fraction of cathode coating that is active Li host
    double electrolyte_ionic_conductivity_s_m{1.0}; // ~1.0 S/m typical LiPF6 organic electrolyte
};

struct BatteryRegionMaterials {
    core::base::Uuid anode_material_id;
    core::base::Uuid cathode_material_id;
    core::base::Uuid electrolyte_material_id;
    core::base::Uuid separator_material_id;
    core::base::Uuid anode_collector_material_id;
    core::base::Uuid cathode_collector_material_id;
    core::base::Uuid casing_material_id;
    core::base::Uuid positive_terminal_material_id;
    core::base::Uuid negative_terminal_material_id;
};

struct BatteryDerivedParameters {
    bool is_compatible{false};
    double total_volume_m3{0.0};
    double total_mass_kg{0.0};
    double active_cathode_mass_kg{0.0};
    double nominal_capacity_ah{0.0};
    double nominal_voltage_v{3.7};
    double max_charge_voltage_v{4.2};
    double min_cutoff_voltage_v{2.8};
    double internal_r0_25c_ohm{0.05};
    double thermal_mass_j_k{40.0};
    double cooling_h_area_w_k{0.35};
    double specific_energy_wh_kg{0.0};
    double max_safe_temp_k{353.15};
    double thermal_runaway_onset_k{393.15};
    std::string fidelity_disclosure{
        "L2 Engineering Approximation: Faraday active-mass capacity + Dual-RC Thevenin ECM + lumped thermal balance (not full Newman P2D DFT)."
    };
    std::vector<std::string> compatibility_errors;
    std::vector<std::string> warnings;
};

struct BatteryDesignSpec {
    core::base::Uuid id{core::base::Uuid::generate_random()};
    std::string name{"Lithium_Battery_Cell"};
    std::string chemistry_label{"Li-Ion (Intercalation)"};
    double ambient_temp_k{298.15};
    double initial_soc{0.8};
    double convective_h_w_m2k{12.0};
    BatteryGeometryConfig geometry;
    BatteryRegionMaterials materials;
    BatteryDerivedParameters derived;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static core::base::Result<BatteryDesignSpec> from_json(std::string_view json_str);
};

struct BatterySimSample {
    double time_s{0.0};
    double soc{0.0};
    double voltage_v{0.0};
    double ocv_v{0.0};
    double current_a{0.0}; // >0 discharging, <0 charging
    double power_w{0.0};
    double capacity_ah{0.0}; // cumulative |I*dt| / 3600
    double temperature_k{298.15};
    double heat_generation_w{0.0};
};

struct BatterySimulationResult {
    bool completed{false};
    bool had_limit_warnings{false};
    std::vector<BatterySimSample> samples;
    std::vector<std::string> warnings;
    double peak_temperature_k{298.15};
    double min_voltage_v{0.0};
    double max_voltage_v{0.0};
    double total_energy_wh{0.0};
};

/**
 * @brief Validates material compatibility, computes electrochemical/thermal parameters from geometry
 *        and materials, simulates charging/discharging, and exports a Battery component.
 */
class BatteryEngineer {
public:
    [[nodiscard]] static BatteryDerivedParameters evaluate_and_validate(
        const BatteryDesignSpec& spec,
        const core::domain::MaterialRegistry& reg);

    [[nodiscard]] static core::base::Result<BatterySimulationResult> simulate_discharge_and_charge(
        const BatteryDesignSpec& spec,
        const core::domain::MaterialRegistry& reg,
        double discharge_c_rate,
        double discharge_duration_s,
        double charge_c_rate,
        double charge_duration_s,
        double dt_s = 1.0);

    [[nodiscard]] static core::base::Result<GeneratedComponentDefinition> generate_component(
        BatteryDesignSpec& spec,
        const core::domain::MaterialRegistry& reg,
        PhysicalObjectManager* obj_mgr = nullptr);
};

} // namespace shunya::eng
