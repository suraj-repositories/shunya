#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/periodic_table.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::matdb {

enum class PropertyProvenance : uint8_t {
    CsvPrimary,
    CsvNormalized,
    NistReferenceFallback,
    MissingUnsupported
};

[[nodiscard]] constexpr std::string_view provenance_to_string(PropertyProvenance p) noexcept {
    switch (p) {
    case PropertyProvenance::CsvPrimary:            return "CSV Primary";
    case PropertyProvenance::CsvNormalized:         return "CSV Normalized";
    case PropertyProvenance::NistReferenceFallback: return "IUPAC/NIST Reference";
    case PropertyProvenance::MissingUnsupported:    return "Missing / Unsupported";
    }
    return "CSV Primary";
}

struct ElementRecord {
    // Identity & Classification
    int atomic_number{0};
    std::string symbol;
    std::string name;
    std::string iupac_symbol;
    std::string iupac_name;
    std::string alternate_names;
    std::string block;
    std::optional<int> group;
    int period{0};
    int graph_period{0};
    int graph_group{0};
    std::string phase;
    std::string color;
    std::string gas_phase;

    // Atomic & Quantum
    std::optional<double> atomic_weight;        // amu (g/mol)
    std::optional<int> valence;
    std::optional<double> electronegativity;    // Pauling
    std::optional<double> electron_affinity;    // kJ/mol
    std::string electron_configuration;         // Raw CSV
    std::string electron_configuration_iupac;   // Verified IUPAC ground-state config
    std::string quantum_numbers;
    std::optional<double> atomic_radius;        // m
    std::optional<double> covalent_radius;      // m
    std::optional<double> van_der_waals_radius; // m

    // Physical & Thermodynamic
    std::optional<double> density;                // kg/m^3
    std::optional<double> density_liquid;         // kg/m^3
    std::optional<double> molar_volume;           // m^3/mol
    std::optional<double> melting_point;          // K
    std::optional<double> boiling_point;          // K
    std::optional<double> absolute_melting_point; // K
    std::optional<double> absolute_boiling_point; // K
    std::optional<double> critical_pressure;      // Pa
    std::optional<double> critical_temperature;   // K
    std::optional<double> heat_of_fusion;         // kJ/mol
    std::optional<double> heat_of_vaporization;   // kJ/mol
    std::optional<double> heat_of_combustion;     // kJ/mol
    std::optional<double> specific_heat;          // J/(kg*K)
    std::optional<double> adiabatic_index;        // dimensionless gamma
    std::optional<double> thermal_conductivity;   // W/(m*K)
    std::optional<double> thermal_expansion;      // 1/K

    // Mechanical & Acoustic & Optical
    std::optional<double> brinell_hardness;       // Pa
    std::optional<double> mohs_hardness;          // scaled value from CSV
    std::optional<double> vickers_hardness;       // Pa
    std::optional<double> bulk_modulus;           // Pa
    std::optional<double> shear_modulus;          // Pa
    std::optional<double> young_modulus;          // Pa
    std::optional<double> poisson_ratio;          // dimensionless
    std::optional<double> refractive_index;       // dimensionless
    std::optional<double> speed_of_sound;         // m/s

    // Electrical & Magnetic
    std::string electrical_type;
    std::optional<double> electrical_conductivity; // S/m
    std::optional<double> resistivity;             // Ohm*m
    std::optional<double> superconducting_point;   // K
    std::string magnetic_type;
    std::optional<double> curie_point;             // K
    std::optional<double> neel_point;              // K
    std::optional<double> mass_magnetic_susceptibility;
    std::optional<double> molar_magnetic_susceptibility;
    std::optional<double> volume_magnetic_susceptibility;

    // Chemical Hazards & Registry Identifiers
    std::optional<double> autoignition_point;      // K
    std::optional<double> flashpoint;              // K
    std::string dot_hazard_class;
    std::string dot_numbers;
    std::string eu_number;
    std::optional<int> nfpa_fire_rating;
    std::optional<int> nfpa_health_rating;
    std::optional<int> nfpa_reactivity_rating;
    std::string rtecs_number;
    std::string cas_number;
    std::string cid_number;
    std::string gmelin_number;
    std::string nsc_number;

    // Abundances (%)
    std::optional<double> percent_in_universe;
    std::optional<double> percent_in_sun;
    std::optional<double> percent_in_meteorites;
    std::optional<double> percent_in_earths_crust;
    std::optional<double> percent_in_oceans;
    std::optional<double> percent_in_humans;

    // Crystallography & Nuclear
    std::string space_group_name;
    std::optional<int> space_group_number;
    std::string half_life_raw;
    std::optional<double> half_life_seconds; // nullopt if Inf or unparseable; is_stable flag tracks Inf
    bool is_radioactive_in_csv{false};
    std::string lifetime_raw;
    std::string decay_mode;
    std::optional<double> neutron_cross_section;
    std::optional<double> neutron_mass_absorption;

    // Full raw CSV fidelity & provenance
    std::unordered_map<std::string, std::string> raw_csv_fields;
    std::unordered_map<std::string, PropertyProvenance> field_provenance;
    std::vector<std::string> normalization_notes;

    [[nodiscard]] std::optional<double> effective_resistivity() const noexcept;
    [[nodiscard]] std::optional<double> effective_conductivity() const noexcept;
    [[nodiscard]] PropertyProvenance provenance_of(std::string_view column_name) const noexcept;
};

enum class ElementSortField : uint8_t {
    AtomicNumber,
    Name,
    Symbol,
    AtomicWeight,
    Density,
    MeltingPoint,
    BoilingPoint,
    SpecificHeat,
    ThermalConductivity,
    ElectricalConductivity,
    YoungModulus,
    Electronegativity
};

struct ElementQuery {
    std::string search_text;
    std::string phase_filter;            // e.g. "Solid", "Liquid", "Gas", or empty for all
    std::string block_filter;            // "s", "p", "d", "f", or empty
    std::string electrical_type_filter;  // "Conductor", "Semiconductor", "Insulator", or empty
    std::string magnetic_type_filter;
    std::optional<int> group_filter;
    std::optional<int> period_filter;
    std::string require_property;        // CSV column name that must be non-empty
    ElementSortField sort_by{ElementSortField::AtomicNumber};
    bool descending{false};
};

/**
 * @brief Centralized, RFC-4180 CSV-backed chemical element database preserving all 82 properties
 *        from C:\Users\Shubham\Desktop\test\chemistry\elements.csv with validation & provenance.
 */
class ElementDatabase {
public:
    ElementDatabase() = default;

    static ElementDatabase& global();

    [[nodiscard]] core::base::Result<void> load_from_csv(const std::filesystem::path& csv_path);
    [[nodiscard]] core::base::Result<void> load_from_csv_text(std::string_view csv_text,
                                                              std::string source_label = "memory");
    [[nodiscard]] core::base::Result<void> load_default();

    [[nodiscard]] bool is_loaded() const noexcept { return !elements_.empty(); }
    [[nodiscard]] size_t count() const noexcept { return elements_.size(); }
    [[nodiscard]] const std::filesystem::path& source_path() const noexcept { return source_path_; }
    [[nodiscard]] const std::vector<std::string>& csv_columns() const noexcept { return columns_; }
    [[nodiscard]] const std::vector<ElementRecord>& all_elements() const noexcept { return elements_; }

    [[nodiscard]] const ElementRecord* find_by_z(int atomic_number) const noexcept;
    [[nodiscard]] const ElementRecord* find_by_symbol(std::string_view symbol) const noexcept;
    [[nodiscard]] const ElementRecord* find_by_name(std::string_view name) const noexcept;

    [[nodiscard]] std::vector<const ElementRecord*> query(const ElementQuery& q) const;

    // Cross-module bridge to Phase 1/2 PeriodicTableDatabase
    void populate_periodic_database(core::domain::PeriodicTableDatabase& db) const;

    // Audit summary of normalization/discrepancy notes found during CSV ingestion
    [[nodiscard]] core::base::ValidationReport audit_dataset() const;

    // RFC 4180 parser exposed for testing
    [[nodiscard]] static std::vector<std::vector<std::string>> parse_rfc4180_csv(std::string_view text);

private:
    std::filesystem::path source_path_;
    std::vector<std::string> columns_;
    std::vector<ElementRecord> elements_;
    std::unordered_map<int, size_t> index_by_z_;
    std::unordered_map<std::string, size_t> index_by_symbol_lower_;
};

} // namespace shunya::matdb
