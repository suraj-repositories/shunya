#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/material_mixing_lab.hpp"

#include <string>
#include <vector>

namespace shunya::mat {

struct EnvironmentConditions {
    std::string name{"Standard_Room"};
    double temperature_k{298.15};
    double pressure_pa{101325.0};
    double gravity_m_s2{9.80665};
    double relative_humidity_pct{50.0};
    double oxygen_pct{20.95};
    double co2_ppm{420.0};
    double salinity_ppt{0.0};          // Parts per thousand NaCl
    double electric_field_v_m{0.0};
    double magnetic_field_t{0.0};
    double radiation_dose_dpa{0.0};
};

struct TestBenchmarkResult {
    std::string domain_name;     // "Thermal", "Mechanical/Gravity", "Vacuum/Space", "Electrical", "Caloric Heat", "Rust & Oxidation", "Extreme Environments"
    bool passed{true};
    double score{100.0};         // 0 - 100%
    std::string metric_summary;  // Key numerical indicators
    std::string physical_model;  // Governing physical law
    std::string provenance;      // Reference benchmark / analytical formulation
    bool is_approximate{false};
    std::vector<std::string> details;
    std::vector<std::string> failure_reasons;
};

struct VirtualTestBenchReport {
    std::string material_name;
    EnvironmentConditions conditions;
    bool overall_qualification_passed{true};
    double aggregate_score{100.0};
    std::vector<TestBenchmarkResult> domain_results;
    std::vector<std::string> critical_failures;
    std::vector<std::string> engineering_recommendations;
};

class VirtualTestBenchEngine {
public:
    // 1. Pre-configured environmental presets
    [[nodiscard]] static EnvironmentConditions preset_spacecraft_leo();
    [[nodiscard]] static EnvironmentConditions preset_deep_sea();
    [[nodiscard]] static EnvironmentConditions preset_industrial_furnace();
    [[nodiscard]] static EnvironmentConditions preset_underground_cable();
    [[nodiscard]] static EnvironmentConditions preset_outdoor_coastal();

    // 2. Comprehensive Multiphysics Qualification
    [[nodiscard]] static VirtualTestBenchReport qualify_material(
        const MixtureEvaluationResult& mixture,
        const EnvironmentConditions& env);

    // 3. Seven Specific Physics Domain Models
    [[nodiscard]] static TestBenchmarkResult evaluate_thermal(
        const MixtureEvaluationResult& mix, const EnvironmentConditions& env);

    [[nodiscard]] static TestBenchmarkResult evaluate_mechanical_gravity(
        const MixtureEvaluationResult& mix, const EnvironmentConditions& env);

    [[nodiscard]] static TestBenchmarkResult evaluate_vacuum_space(
        const MixtureEvaluationResult& mix, const EnvironmentConditions& env);

    [[nodiscard]] static TestBenchmarkResult evaluate_electrical(
        const MixtureEvaluationResult& mix, const EnvironmentConditions& env);

    [[nodiscard]] static TestBenchmarkResult evaluate_caloric_heat(
        const MixtureEvaluationResult& mix, const EnvironmentConditions& env);

    [[nodiscard]] static TestBenchmarkResult evaluate_oxidation_rust(
        const MixtureEvaluationResult& mix, const EnvironmentConditions& env);

    [[nodiscard]] static TestBenchmarkResult evaluate_extreme_environments(
        const MixtureEvaluationResult& mix, const EnvironmentConditions& env);
};

} // namespace shunya::mat
