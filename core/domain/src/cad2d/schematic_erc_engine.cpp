#include "shunya/cad2d/schematic_erc_engine.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace shunya::cad2d {

namespace {

bool is_near_pt(const Point2D& a, const Point2D& b, double tol = 0.5) noexcept {
    return std::abs(a.x - b.x) <= tol && std::abs(a.y - b.y) <= tol;
}

double dist_sq_pt(const Point2D& a, const Point2D& b) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

size_t ErcResult2D::error_count() const noexcept {
    size_t count = 0;
    for (const auto& b : badges) {
        if (b.severity == pcb::ErcSeverity::Error) ++count;
    }
    return count;
}

size_t ErcResult2D::warning_count() const noexcept {
    size_t count = 0;
    for (const auto& b : badges) {
        if (b.severity == pcb::ErcSeverity::Warning) ++count;
    }
    return count;
}

ErcResult2D SchematicErcEngine2D::analyze(
    const SketchDocument2D& doc,
    const SchematicDraftingEngine& symbol_engine,
    const SchematicWireRouter& wire_router,
    const SchematicNetlabelBusManager& netlabel_mgr,
    const SchematicPinPlacementTool* pin_tool
) const {
    (void)doc;
    ErcResult2D result;

    struct UnifiedPin {
        std::string id;
        std::string name;
        pcb::PinElectricalType type{pcb::PinElectricalType::Passive};
        Point2D pos;
    };
    std::vector<UnifiedPin> all_pins;

    // 1. Gather pins from symbol instances
    for (const auto* inst : symbol_engine.all_instances()) {
        for (const auto& p : inst->pins) {
            all_pins.push_back({p.pin_id, inst->reference_designator + "." + p.pin_name, p.electrical_type, p.world_pos});
        }
    }

    // 2. Gather pins from standalone pin tool
    if (pin_tool) {
        for (const auto& p : pin_tool->all_pins()) {
            all_pins.push_back({p.pin_id, p.pin_name, p.electrical_type, pin_tool->terminal_anchor(p)});
        }
    }

    const auto& wires = wire_router.wires();
    const auto& labels = netlabel_mgr.all_netlabels();

    // Map net name to connected pins
    std::unordered_map<std::string, std::vector<UnifiedPin>> net_to_pins;
    std::unordered_set<std::string> connected_pin_ids;

    for (const auto& pin : all_pins) {
        bool connected = false;
        std::string resolved_net;

        // Check if wire touches pin
        for (const auto& w : wires) {
            for (const auto& wpt : w.points) {
                if (is_near_pt(pin.pos, wpt, 0.5)) {
                    connected = true;
                    connected_pin_ids.insert(pin.id);
                    resolved_net = w.net_name;
                    net_to_pins[resolved_net].push_back(pin);
                    break;
                }
            }
            if (connected) break;
        }

        // Check if netlabel touches pin directly
        if (!connected) {
            for (const auto& l : labels) {
                if (is_near_pt(pin.pos, l.position, 0.5)) {
                    connected = true;
                    connected_pin_ids.insert(pin.id);
                    resolved_net = l.name;
                    net_to_pins[resolved_net].push_back(pin);
                    break;
                }
            }
        }

        // Check 1: Floating Pins
        if (!connected) {
            if (pin.type == pcb::PinElectricalType::Input || pin.type == pcb::PinElectricalType::PowerInput) {
                result.badges.push_back(ErcBadge{
                    "badge_flt_" + pin.id,
                    pin.pos,
                    pcb::ErcSeverity::Error,
                    ErcRuleKind::FloatingPin,
                    pin.name,
                    "Floating input/power pin: " + pin.name + " has no connection",
                    ""
                });
            } else if (pin.type != pcb::PinElectricalType::Unconnected) {
                result.badges.push_back(ErcBadge{
                    "badge_flt_" + pin.id,
                    pin.pos,
                    pcb::ErcSeverity::Warning,
                    ErcRuleKind::FloatingPin,
                    pin.name,
                    "Unconnected pin: " + pin.name,
                    ""
                });
            }
        }
    }

    // Check 2: Conflicting Net Names (two distinct labels on the same wire path)
    for (const auto& w : wires) {
        std::vector<std::string> touched_labels;
        for (const auto& l : labels) {
            for (const auto& wpt : w.points) {
                if (is_near_pt(l.position, wpt, 0.5)) {
                    if (std::find(touched_labels.begin(), touched_labels.end(), l.name) == touched_labels.end()) {
                        touched_labels.push_back(l.name);
                    }
                    break;
                }
            }
        }
        if (touched_labels.size() > 1) {
            result.badges.push_back(ErcBadge{
                "badge_conf_" + w.wire_id,
                w.points.front(),
                pcb::ErcSeverity::Error,
                ErcRuleKind::NetNameConflict,
                w.wire_id,
                "Net conflict on wire: " + touched_labels[0] + " shorted to " + touched_labels[1],
                ""
            });
        }
    }

    // Check 3: Driver Conflicts & Open Circuits per Net
    for (const auto& [net_name, pins_in_net] : net_to_pins) {
        // Driver conflict check between all pin pairs
        for (size_t i = 0; i < pins_in_net.size(); ++i) {
            for (size_t j = i + 1; j < pins_in_net.size(); ++j) {
                pcb::ErcSeverity sev = pcb::ErcMatrix::check_pin_pair(pins_in_net[i].type, pins_in_net[j].type);
                if (sev == pcb::ErcSeverity::Error) {
                    result.badges.push_back(ErcBadge{
                        "badge_drv_" + pins_in_net[i].id,
                        pins_in_net[i].pos,
                        pcb::ErcSeverity::Error,
                        ErcRuleKind::DriverConflict,
                        net_name,
                        "Driver contention on net " + net_name + ": " + pins_in_net[i].name + " vs " + pins_in_net[j].name,
                        ""
                    });
                }
            }
        }

        // Open Circuit (single pin on net and no netlabels)
        if (pins_in_net.size() == 1) {
            bool has_label = false;
            for (const auto& l : labels) {
                if (l.name == net_name) { has_label = true; break; }
            }
            if (!has_label) {
                result.badges.push_back(ErcBadge{
                    "badge_open_" + pins_in_net[0].id,
                    pins_in_net[0].pos,
                    pcb::ErcSeverity::Warning,
                    ErcRuleKind::OpenCircuit,
                    net_name,
                    "Open circuit: Net " + net_name + " only has 1 pin connected (" + pins_in_net[0].name + ")",
                    ""
                });
            }
        }
    }

    return result;
}

ErcResult2D SchematicErcEngine2D::update_badges(
    SketchDocument2D& doc,
    const SchematicDraftingEngine& symbol_engine,
    const SchematicWireRouter& wire_router,
    const SchematicNetlabelBusManager& netlabel_mgr,
    const SchematicPinPlacementTool* pin_tool
) {
    clear_badges(doc);
    ErcResult2D res = analyze(doc, symbol_engine, wire_router, netlabel_mgr, pin_tool);

    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_ANNOTATIONS);
    for (auto& b : res.badges) {
        b.badge_id = "erc_badge_" + std::to_string(next_badge_idx_++);
        // Visual badge: diamond or circle badge of radius 1.2mm
        auto badge_circ = std::make_shared<Circle2D>(b.position, 1.2);
        b.visual_entity_id = doc.add_entity(badge_circ, true, layer);
    }

    active_badges_ = res.badges;
    return res;
}

void SchematicErcEngine2D::clear_badges(SketchDocument2D& doc) {
    for (const auto& b : active_badges_) {
        if (!b.visual_entity_id.empty()) {
            doc.remove_entity(b.visual_entity_id);
        }
    }
    active_badges_.clear();
}

const ErcBadge* SchematicErcEngine2D::find_badge_at(Point2D pos, double tol) const noexcept {
    const double tol_sq = tol * tol;
    for (const auto& b : active_badges_) {
        if (dist_sq_pt(pos, b.position) <= tol_sq) {
            return &b;
        }
    }
    return nullptr;
}

} // namespace shunya::cad2d
