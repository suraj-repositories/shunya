#pragma once

#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/schematic_netlabel_bus.hpp"
#include "shunya/cad2d/schematic_pin_tool.hpp"
#include "shunya/cad2d/schematic_symbol_engine.hpp"
#include "shunya/cad2d/schematic_wire_router.hpp"
#include "shunya/pcb/erc_matrix.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::cad2d {

enum class ErcRuleKind {
    FloatingPin,
    OpenCircuit,
    NetNameConflict,
    DriverConflict,
    UndrivenPower
};

[[nodiscard]] constexpr std::string_view erc_rule_kind_to_string(ErcRuleKind k) noexcept {
    switch (k) {
        case ErcRuleKind::FloatingPin:       return "FloatingPin";
        case ErcRuleKind::OpenCircuit:       return "OpenCircuit";
        case ErcRuleKind::NetNameConflict:   return "NetNameConflict";
        case ErcRuleKind::DriverConflict:    return "DriverConflict";
        case ErcRuleKind::UndrivenPower:     return "UndrivenPower";
    }
    return "Unknown";
}

struct ErcBadge {
    std::string badge_id;
    Point2D position;
    pcb::ErcSeverity severity{pcb::ErcSeverity::Warning};
    ErcRuleKind rule{ErcRuleKind::FloatingPin};
    std::string target_name;
    std::string message;
    std::string visual_entity_id; // Circle/Badge entity in SketchDocument2D
};

struct ErcResult2D {
    std::vector<ErcBadge> badges;

    [[nodiscard]] size_t error_count() const noexcept;
    [[nodiscard]] size_t warning_count() const noexcept;
    [[nodiscard]] bool passed() const noexcept { return error_count() == 0; }
};

/**
 * @brief Real-time Electrical Rules Check (ERC) engine for the 2D schematic editor.
 *
 * Scans components, pins, wires, and netlabels in real-time, detecting floating inputs,
 * open-circuit nets, conflicting net names, and driver collisions, and renders visual badges.
 */
class SchematicErcEngine2D {
public:
    SchematicErcEngine2D() = default;

    /**
     * @brief Performs full ERC analysis across components, pins, wires, and netlabels.
     */
    [[nodiscard]] ErcResult2D analyze(
        const SketchDocument2D& doc,
        const SchematicDraftingEngine& symbol_engine,
        const SchematicWireRouter& wire_router,
        const SchematicNetlabelBusManager& netlabel_mgr,
        const SchematicPinPlacementTool* pin_tool = nullptr
    ) const;

    /**
     * @brief Runs analysis, clears previous badges, and synthesizes interactive visual badges in doc.
     */
    ErcResult2D update_badges(
        SketchDocument2D& doc,
        const SchematicDraftingEngine& symbol_engine,
        const SchematicWireRouter& wire_router,
        const SchematicNetlabelBusManager& netlabel_mgr,
        const SchematicPinPlacementTool* pin_tool = nullptr
    );

    void clear_badges(SketchDocument2D& doc);

    [[nodiscard]] const std::vector<ErcBadge>& active_badges() const noexcept { return active_badges_; }
    [[nodiscard]] const ErcBadge* find_badge_at(Point2D pos, double tol = 2.0) const noexcept;

private:
    uint64_t next_badge_idx_{1};
    std::vector<ErcBadge> active_badges_;
};

} // namespace shunya::cad2d
