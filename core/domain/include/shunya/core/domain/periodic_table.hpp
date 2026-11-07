#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>
#include <shunya/core/base/uuid.hpp>
#include <shunya/core/domain/material_curve.hpp>
#include <shunya/core/domain/material_dictionary.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// MATDB-05: Element Isotope Definition
struct ElementIsotope {
    int mass_number{1};             // A
    double atomic_mass{1.0};        // amu / Da
    double abundance_pct{100.0};    // 0.0 to 100.0 %
    bool is_stable{true};
    std::optional<double> half_life_seconds;
    std::string spin_parity;

    bool operator==(const ElementIsotope& o) const noexcept {
        return mass_number == o.mass_number &&
               atomic_mass == o.atomic_mass &&
               abundance_pct == o.abundance_pct &&
               is_stable == o.is_stable &&
               half_life_seconds == o.half_life_seconds &&
               spin_parity == o.spin_parity;
    }
};

// MATDB-01, MATDB-02, MATDB-05: Periodic Element Representation
struct PeriodicElement {
    int atomic_number{1};           // Z (1 to 118)
    std::string symbol{"H"};
    std::string name{"Hydrogen"};
    int period{1};                  // 1 to 7
    int group{1};                   // 1 to 18
    char block{'s'};                // 's', 'p', 'd', 'f'
    double standard_atomic_weight{1.008}; // amu
    std::string electron_configuration{"1s1"};
    std::optional<double> electronegativity;       // Pauling scale
    std::optional<double> density_at_stp;          // kg/m^3
    std::optional<double> melting_point_k;         // K
    std::optional<double> boiling_point_k;         // K
    std::optional<double> specific_heat_capacity;  // J/(kg*K)
    std::optional<double> thermal_conductivity;    // W/(m*K)
    std::optional<double> electrical_conductivity; // S/m

    std::vector<ElementIsotope> isotopes;
    std::unordered_map<std::string, AuditedValue> audited_fields;
    std::unordered_map<std::string, MaterialCurve> curves;

    [[nodiscard]] bool has_isotope(int mass_number) const noexcept;
    [[nodiscard]] const ElementIsotope* get_isotope(int mass_number) const noexcept;
};

// MATDB-06: Review Queue for Conflicting Sources
enum class ConflictResolutionState : uint8_t {
    PendingReview,
    AcceptedSource,
    ManualOverride,
    Rejected
};

[[nodiscard]] constexpr std::string_view conflict_resolution_to_string(ConflictResolutionState s) noexcept {
    switch (s) {
    case ConflictResolutionState::PendingReview:  return "PendingReview";
    case ConflictResolutionState::AcceptedSource: return "AcceptedSource";
    case ConflictResolutionState::ManualOverride: return "ManualOverride";
    case ConflictResolutionState::Rejected:       return "Rejected";
    }
    return "PendingReview";
}

[[nodiscard]] base::Result<ConflictResolutionState> conflict_resolution_from_string(std::string_view s);

struct ConflictRecord {
    base::Uuid conflict_id;
    std::string element_symbol;
    std::string property_name;
    std::vector<AuditedValue> candidate_values;
    ConflictResolutionState resolution_state{ConflictResolutionState::PendingReview};
    std::optional<AuditedValue> resolved_value;
    std::string reviewer_notes;

    bool operator==(const ConflictRecord& o) const noexcept {
        return conflict_id == o.conflict_id &&
               element_symbol == o.element_symbol &&
               property_name == o.property_name &&
               candidate_values == o.candidate_values &&
               resolution_state == o.resolution_state &&
               resolved_value == o.resolved_value &&
               reviewer_notes == o.reviewer_notes;
    }
};

// MATDB-02, MATDB-05, MATDB-06, MATDB-10: Periodic Table Database
class PeriodicTableDatabase {
public:
    PeriodicTableDatabase();

    // Element Management
    base::Result<void> add_element(PeriodicElement elem);
    [[nodiscard]] bool has_element(int atomic_number) const noexcept;
    [[nodiscard]] bool has_element(std::string_view symbol) const noexcept;
    [[nodiscard]] const PeriodicElement* get_element(int atomic_number) const noexcept;
    [[nodiscard]] const PeriodicElement* get_element(std::string_view symbol) const noexcept;
    [[nodiscard]] size_t element_count() const noexcept { return elements_by_z_.size(); }
    [[nodiscard]] const std::unordered_map<int, PeriodicElement>& elements() const noexcept { return elements_by_z_; }

    // MATDB-02: Dataset Import Adapter
    base::Result<void> import_dataset_record(std::string_view provider_id,
                                             std::string_view provider_name,
                                             int atomic_number,
                                             std::string_view symbol,
                                             std::string_view name,
                                             const std::vector<AuditedValue>& values);

    // MATDB-05: Consistency Validation
    [[nodiscard]] base::ValidationReport validate_periodic_consistency() const;

    // MATDB-06: Conflicting Sources Review Queue
    base::Result<base::Uuid> register_conflict(std::string element_symbol,
                                               std::string property_name,
                                               std::vector<AuditedValue> candidates);
    base::Result<void> resolve_conflict(const base::Uuid& conflict_id,
                                        ConflictResolutionState state,
                                        std::optional<AuditedValue> resolved_val,
                                        std::string reviewer_notes);
    [[nodiscard]] std::vector<ConflictRecord> get_pending_conflicts() const;
    [[nodiscard]] const ConflictRecord* get_conflict(const base::Uuid& id) const noexcept;
    [[nodiscard]] size_t conflict_count() const noexcept { return conflicts_.size(); }

    // MATDB-10: Standard Regression Fixtures
    void seed_representative_elements();

    // JSON Serialization
    [[nodiscard]] std::string to_json() const;
    static base::Result<PeriodicTableDatabase> from_json(std::string_view json_str);

private:
    std::unordered_map<int, PeriodicElement> elements_by_z_;
    std::unordered_map<std::string, int> symbol_to_z_;
    std::vector<ConflictRecord> conflicts_;
};

} // namespace shunya::core::domain
