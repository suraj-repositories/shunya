#include "shunya/cad2d/schematic_pin_tool.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

Point2D SchematicPinPlacementTool::terminal_anchor(const SchematicPinSpec& pin) const noexcept {
    switch (pin.orientation) {
        case PinOrientation::Right:
            return {pin.position.x + pin.length_mm, pin.position.y};
        case PinOrientation::Left:
            return {pin.position.x - pin.length_mm, pin.position.y};
        case PinOrientation::Up:
            return {pin.position.x, pin.position.y + pin.length_mm};
        case PinOrientation::Down:
            return {pin.position.x, pin.position.y - pin.length_mm};
    }
    return {pin.position.x + pin.length_mm, pin.position.y};
}

std::string SchematicPinPlacementTool::place_pin(
    SketchDocument2D& doc,
    Point2D position,
    std::optional<std::string> pin_number,
    std::optional<std::string> pin_name,
    std::optional<pcb::PinElectricalType> electrical_type,
    std::optional<PinOrientation> orientation
) {
    SchematicPinSpec spec;
    spec.pin_id = "sch_pin_" + std::to_string(next_pin_idx_++);
    spec.position = position;
    spec.electrical_type = electrical_type.value_or(default_type_);
    spec.orientation = orientation.value_or(default_orientation_);
    spec.length_mm = default_length_mm_;

    if (pin_number.has_value()) {
        spec.pin_number = *pin_number;
    } else {
        spec.pin_number = std::to_string(next_number_);
        if (auto_increment_) {
            ++next_number_;
        }
    }

    if (pin_name.has_value()) {
        spec.pin_name = *pin_name;
    } else {
        spec.pin_name = next_name_ + spec.pin_number;
    }

    // Synthesize visual graphics into document on SCHEMATIC_LAYER_PINS
    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_PINS);
    const Point2D anchor = terminal_anchor(spec);

    // 1. Terminal stalk line
    auto stalk = std::make_shared<LineSegment2D>(spec.position, anchor);
    std::string stalk_eid = doc.add_entity(stalk, true, layer);
    spec.graphic_entity_ids.push_back(stalk_eid);

    // 2. Terminal anchor dot for wire connection
    auto dot = std::make_shared<Circle2D>(anchor, 0.4);
    std::string dot_eid = doc.add_entity(dot, true, layer);
    spec.graphic_entity_ids.push_back(dot_eid);

    const std::string id = spec.pin_id;
    pins_.push_back(std::move(spec));
    return id;
}

bool SchematicPinPlacementTool::remove_pin(SketchDocument2D& doc, std::string_view pin_id) {
    auto it = std::find_if(pins_.begin(), pins_.end(), [&](const SchematicPinSpec& p) {
        return p.pin_id == pin_id;
    });
    if (it == pins_.end()) return false;

    for (const auto& eid : it->graphic_entity_ids) {
        doc.remove_entity(eid);
    }
    pins_.erase(it);
    return true;
}

void SchematicPinPlacementTool::clear(SketchDocument2D& doc) {
    for (const auto& p : pins_) {
        for (const auto& eid : p.graphic_entity_ids) {
            doc.remove_entity(eid);
        }
    }
    pins_.clear();
}

const SchematicPinSpec* SchematicPinPlacementTool::find_pin(std::string_view pin_id) const noexcept {
    for (const auto& p : pins_) {
        if (p.pin_id == pin_id) return &p;
    }
    return nullptr;
}

std::string SchematicPinPlacementTool::hit_test_pin(Point2D pos, double tol) const {
    const double tol_sq = tol * tol;
    for (const auto& p : pins_) {
        const Point2D anchor = terminal_anchor(p);
        const double d_base = (pos.x - p.position.x) * (pos.x - p.position.x) + (pos.y - p.position.y) * (pos.y - p.position.y);
        const double d_anchor = (pos.x - anchor.x) * (pos.x - anchor.x) + (pos.y - anchor.y) * (pos.y - anchor.y);
        if (d_base <= tol_sq || d_anchor <= tol_sq) {
            return p.pin_id;
        }
    }
    return {};
}

std::vector<pcb::SymbolPinDef> SchematicPinPlacementTool::export_symbol_pin_defs() const {
    std::vector<pcb::SymbolPinDef> defs;
    defs.reserve(pins_.size());
    for (const auto& p : pins_) {
        defs.push_back(pcb::SymbolPinDef{
            p.pin_number,
            p.pin_name,
            p.electrical_type,
            {p.position.x, p.position.y}
        });
    }
    return defs;
}

} // namespace shunya::cad2d
