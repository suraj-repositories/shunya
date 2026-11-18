#pragma once

#include "shunya/matdb/element_database.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shunya::mat {

struct MaterialMetricScores {
    std::string material_name;
    // 8 criteria:
    // 1. Electrical Conductivity (MS/m)
    double electrical_conductivity{0.0};
    // 2. Thermal Conductivity (W/(m*K))
    double thermal_conductivity{0.0};
    // 3. Temperature Tolerance (Melting / softening in K)
    double temperature_tolerance{300.0};
    // 4. Corrosion Resistance (0.0 to 100.0 index)
    double corrosion_resistance{50.0};
    // 5. Mass Density (kg/m^3)
    double density{5000.0};
    // 6. Mechanical Strength (Yield strength in MPa)
    double mechanical_strength{200.0};
    // 7. Economic Abundance Index (0.0 to 100.0 index, higher = cheaper/more abundant)
    double economic_abundance{50.0};
    // 8. EMI Shielding Suitability (0.0 to 100.0 index)
    double emi_shielding{50.0};

    // Normalized scores (0.0 to 1.0) for Radar plot
    std::array<double, 8> normalized_radar{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

struct MaterialRankingResult {
    std::string goal;
    std::string winner_name;
    std::vector<std::pair<std::string, double>> rankings; // {material_name, score 0-100}
    std::string explanation;
};

struct StructuredExplanation {
    std::string summary;
    std::vector<std::string> features_advantages;
    std::vector<std::string> disadvantages_risks;
    std::string physical_mechanism;
    std::vector<std::string> suggested_improvements;
    std::string confidence_tag;
    std::string provenance;
};

class MaterialComparisonEngine {
public:
    [[nodiscard]] static MaterialMetricScores evaluate_element(const matdb::ElementRecord& el);

    [[nodiscard]] static MaterialMetricScores evaluate_mixture(
        const std::string& name,
        const std::vector<const matdb::ElementRecord*>& elements,
        const std::vector<double>& mass_fractions,
        double density,
        double thermal_k,
        double electrical_sigma,
        double melting_k,
        const std::string& scenario);

    static void normalize_radar_scores(std::vector<MaterialMetricScores>& list);

    [[nodiscard]] static MaterialRankingResult rank_materials(
        const std::vector<MaterialMetricScores>& candidates,
        const std::string& goal);

    [[nodiscard]] static StructuredExplanation generate_explanation(
        const MaterialMetricScores& mat,
        const std::vector<std::string>& constituents,
        const std::vector<double>& fractions,
        const std::string& scenario,
        const std::string& miscibility_state);

    [[nodiscard]] static std::string export_markdown_report(
        const StructuredExplanation& expl,
        const MaterialMetricScores& mat);
};

} // namespace shunya::mat
