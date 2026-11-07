#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain::adr {

// =============================================================================
// ADR-001 to ADR-010: Architecture Decision Record Schema & Status
// =============================================================================

enum class ADRStatus {
    Proposed,
    Accepted,
    Rejected,
    Deprecated,
    Superseded
};

[[nodiscard]] constexpr std::string_view adr_status_to_string(ADRStatus status) noexcept {
    switch (status) {
        case ADRStatus::Proposed: return "Proposed";
        case ADRStatus::Accepted: return "Accepted";
        case ADRStatus::Rejected: return "Rejected";
        case ADRStatus::Deprecated: return "Deprecated";
        case ADRStatus::Superseded: return "Superseded";
    }
    return "Unknown";
}

struct ArchitectureDecisionRecord {
    std::string id;                  // e.g. "ADR-001"
    std::string title;               // e.g. "Desktop UI & Core Decoupling"
    ADRStatus status{ADRStatus::Accepted};
    std::string context;             // Architectural context & problem statement
    std::string decision;            // Chosen decision
    std::vector<std::string> positive_consequences;
    std::vector<std::string> negative_consequences;
    std::vector<std::string> compliance_rules; // Enforceable architectural invariants
};

class ADRRegistry {
public:
    ADRRegistry();

    [[nodiscard]] static ADRRegistry create_standard_registry();

    void register_adr(ArchitectureDecisionRecord record);
    [[nodiscard]] std::optional<ArchitectureDecisionRecord> find_adr(const std::string& id) const;
    [[nodiscard]] const std::vector<ArchitectureDecisionRecord>& all_adrs() const noexcept { return records_; }

    // Evaluates architectural compliance against platform invariants
    [[nodiscard]] base::Result<void> verify_system_compliance() const;

    // Formats standard Markdown for documentation export
    [[nodiscard]] static std::string export_markdown(const ArchitectureDecisionRecord& adr);

private:
    std::vector<ArchitectureDecisionRecord> records_;
    std::unordered_map<std::string, size_t> id_map_;
};

} // namespace shunya::core::domain::adr
