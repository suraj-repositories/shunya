#pragma once

#include "shunya/pcb/erc_matrix.hpp"
#include "shunya/pcb/schematic_sheet.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

struct ErcViolation {
    ErcSeverity severity{ErcSeverity::Warning};
    std::string rule_id;
    std::string net_or_component;
    std::string message;
};

struct ErcReport {
    std::vector<ErcViolation> violations;

    [[nodiscard]] size_t error_count() const noexcept;
    [[nodiscard]] size_t warning_count() const noexcept;
    [[nodiscard]] bool passed() const noexcept { return error_count() == 0; }
};

/**
 * @brief Validates compiled schematic netlists for driver conflicts, floating inputs, and undriven power rails.
 */
class ErcEngine {
public:
    [[nodiscard]] static ErcReport run_checks(const SchematicDocument& document);
    [[nodiscard]] static ErcReport run_checks_on_netlist(const CompiledNetlist& netlist);
};

} // namespace shunya::pcb
