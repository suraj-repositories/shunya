#pragma once

#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/cavity_physics.hpp"
#include "shunya/mat/virtual_test_bench.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/base/result.hpp"

#include <string>
#include <vector>
#include <optional>

namespace shunya::eng {

/**
 * @brief Environmental stimuli and operating parameters for qualification testing.
 */
struct ObjectTestBenchStimuli {
    // Electrical Stimulus
    double applied_voltage_v{120.0};
    double applied_current_a{0.5};
    std::string terminal_a{"+V"};
    std::string terminal_b{"GND"};

    // Thermal Stimulus
    double ambient_temperature_k{298.15};
    double external_heat_flux_w{0.0};

    // Mechanical Stimulus
    double gravity_m_s2{9.80665}; // Earth 9.81, Moon 1.62, Space 0.0
    double applied_force_n{0.0};   // Tensile or cantilever load

    // Environmental Stimulus
    double ambient_pressure_pa{101325.0};
    double relative_humidity_pct{50.0};
    double oxygen_pct{20.95};
    double salinity_ppt{0.0};
};

/**
 * @brief Individual physics domain qualification outcome.
 */
struct DomainQualificationResult {
    std::string domain_name;     // "Thermal", "Electrical", "Mechanical", "Pressure/Vacuum", "Environmental Degradation"
    bool passed{true};
    double score{100.0};         // 0 - 100%
    std::string metric_summary;  // Key numerical indicators
    std::string physical_model;  // Governing physical equation
    std::string provenance;      // Reference source / standard
    std::vector<std::string> details;
    std::vector<std::string> warnings;
    std::vector<std::string> failure_reasons;
};

/**
 * @brief Point on a response curve for 2D plotting.
 */
struct ResponseCurvePoint {
    double x{0.0};
    double y{0.0};
};

/**
 * @brief Complete multiphysics qualification dossier for a PhysicalObject.
 */
struct ObjectTestBenchReport {
    std::string object_name;
    core::base::Uuid object_id;
    bool overall_passed{true};
    double composite_score{100.0};
    std::vector<DomainQualificationResult> domains;
    std::vector<std::string> critical_failures;
    std::vector<std::string> engineering_recommendations;

    // Response Curves for 2D Plotting
    std::vector<ResponseCurvePoint> curve_temperature_vs_time; // T(K) vs t(s)
    std::vector<ResponseCurvePoint> curve_voltage_vs_current;   // V(V) vs I(A)
    std::vector<ResponseCurvePoint> curve_deflection_vs_load;   // Deflection(mm) vs Load(N)

    [[nodiscard]] std::string to_markdown() const;
    [[nodiscard]] std::string to_json() const;
};

/**
 * @brief First-principles qualification test bench for 3D physical objects with
 *        internal cavities, regions, and electrical terminals.
 */
class ObjectTestBenchEngine {
public:
    // Standard Stimuli Presets
    [[nodiscard]] static ObjectTestBenchStimuli preset_standard_room();
    [[nodiscard]] static ObjectTestBenchStimuli preset_spacecraft_leo();
    [[nodiscard]] static ObjectTestBenchStimuli preset_high_voltage_step();
    [[nodiscard]] static ObjectTestBenchStimuli preset_lunar_surface();

    // Comprehensive Multiphysics Qualification
    [[nodiscard]] static ObjectTestBenchReport qualify_object(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& reg,
        const ObjectTestBenchStimuli& stimuli);

    // Individual Domain Solvers
    [[nodiscard]] static DomainQualificationResult evaluate_thermal(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& reg,
        const ObjectTestBenchStimuli& stimuli);

    [[nodiscard]] static DomainQualificationResult evaluate_electrical(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& reg,
        const ObjectTestBenchStimuli& stimuli);

    [[nodiscard]] static DomainQualificationResult evaluate_mechanical(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& reg,
        const ObjectTestBenchStimuli& stimuli);

    [[nodiscard]] static DomainQualificationResult evaluate_pressure_vacuum(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& reg,
        const ObjectTestBenchStimuli& stimuli);

    [[nodiscard]] static DomainQualificationResult evaluate_environmental_degradation(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& reg,
        const ObjectTestBenchStimuli& stimuli);
};

} // namespace shunya::eng
