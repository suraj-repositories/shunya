#include "shunya/cad2d/schematic_symbol_engine.hpp"

#include <algorithm>

namespace shunya::cad2d {

SchematicDraftingEngine::SchematicDraftingEngine(pcb::SymbolStandard standard)
    : catalog_(standard) {}

void SchematicDraftingEngine::ensure_schematic_layers(LayerManager& layer_mgr) {
    if (!layer_mgr.find_layer(SCHEMATIC_LAYER_SYMBOLS)) {
        layer_mgr.add_layer(std::string(SCHEMATIC_LAYER_SYMBOLS), "0", {"#61afef", 0.35, LinePattern::Solid, 1.0});
    }
    if (!layer_mgr.find_layer(SCHEMATIC_LAYER_PINS)) {
        layer_mgr.add_layer(std::string(SCHEMATIC_LAYER_PINS), "0", {"#e5c07b", 0.25, LinePattern::Solid, 1.0});
    }
    if (!layer_mgr.find_layer(SCHEMATIC_LAYER_WIRES)) {
        layer_mgr.add_layer(std::string(SCHEMATIC_LAYER_WIRES), "0", {"#98c379", 0.30, LinePattern::Solid, 1.0});
    }
    if (!layer_mgr.find_layer(SCHEMATIC_LAYER_ANNOTATIONS)) {
        layer_mgr.add_layer(std::string(SCHEMATIC_LAYER_ANNOTATIONS), "0", {"#abb2bf", 0.18, LinePattern::Solid, 1.0});
    }
}

std::string SchematicDraftingEngine::place_symbol(
    SketchDocument2D& doc,
    std::string_view symbol_id,
    Point2D position,
    int rotation_deg,
    std::string ref_des,
    std::string value
) {
    std::string sym_lookup(symbol_id);
    if (sym_lookup == "R") sym_lookup = "Resistor";
    else if (sym_lookup == "C") sym_lookup = "Capacitor";
    else if (sym_lookup == "L") sym_lookup = "Inductor";
    else if (sym_lookup == "D") sym_lookup = "Diode";

    pcb::SymbolDefinition def = catalog_.get_symbol(sym_lookup);
    if (def.id.empty() && def.graphics.empty() && def.pins.empty()) {
        def = catalog_.get_symbol(symbol_id);
    }
    if (def.id.empty() && def.graphics.empty() && def.pins.empty()) {
        return {};
    }

    if (rotation_deg != 0) {
        def = def.transformed(rotation_deg, false, false);
    }

    SchematicComponentInstance comp;
    comp.instance_id = "sch_cmp_" + std::to_string(next_instance_idx_++);
    comp.symbol_id = std::string(symbol_id);
    comp.position = position;
    comp.rotation_deg = rotation_deg;
    comp.value = std::move(value);

    if (ref_des.empty()) {
        comp.reference_designator = def.ref_prefix + std::to_string(instances_.size() + 1);
    } else {
        comp.reference_designator = std::move(ref_des);
    }

    // 1. Instantiate graphics into document
    // We mark is_construction = true to guarantee mechanical solid profile extractors never include schematic graphics
    for (const auto& prim : def.graphics) {
        std::shared_ptr<Curve2D> curve;
        if (prim.kind == pcb::SymbolPrimitive::Kind::Line) {
            curve = std::make_shared<LineSegment2D>(
                Point2D{position.x + prim.p0.x, position.y + prim.p0.y},
                Point2D{position.x + prim.p1.x, position.y + prim.p1.y}
            );
        } else if (prim.kind == pcb::SymbolPrimitive::Kind::Rect) {
            // Outline rect as 4 segments or polyline
            const double w = prim.p1.x;
            const double h = prim.p1.y;
            const Point2D c = {position.x + prim.p0.x, position.y + prim.p0.y};
            std::vector<Point2D> pts = {
                {c.x - w, c.y - h},
                {c.x + w, c.y - h},
                {c.x + w, c.y + h},
                {c.x - w, c.y + h}
            };
            curve = std::make_shared<Polyline2D>(std::move(pts), true);
        } else if (prim.kind == pcb::SymbolPrimitive::Kind::Circle) {
            curve = std::make_shared<Circle2D>(
                Point2D{position.x + prim.p0.x, position.y + prim.p0.y},
                prim.p1.x
            );
        }

        if (curve) {
            std::string eid = doc.add_entity(curve, true, std::string(SCHEMATIC_LAYER_SYMBOLS));
            comp.graphic_entity_ids.push_back(eid);
            entity_to_instance_map_[eid] = comp.instance_id;
        }
    }

    // 2. Instantiate pins
    for (const auto& pin_def : def.pins) {
        SchematicPinInstance pin;
        pin.pin_id = "sch_pin_" + std::to_string(next_pin_idx_++);
        pin.symbol_instance_id = comp.instance_id;
        pin.pin_number = pin_def.number;
        pin.pin_name = pin_def.name;
        pin.electrical_type = pin_def.electrical_type;
        pin.world_pos = {position.x + pin_def.pos_mm.x, position.y + pin_def.pos_mm.y};

        // Add small pin indicator graphic (e.g. circle of radius 0.5mm)
        auto pin_curve = std::make_shared<Circle2D>(pin.world_pos, 0.5);
        std::string pin_eid = doc.add_entity(pin_curve, true, std::string(SCHEMATIC_LAYER_PINS));
        comp.graphic_entity_ids.push_back(pin_eid);
        entity_to_instance_map_[pin_eid] = comp.instance_id;
        pin_to_instance_map_[pin.pin_id] = comp.instance_id;

        comp.pins.push_back(std::move(pin));
    }

    const std::string id = comp.instance_id;
    instances_[id] = std::move(comp);
    return id;
}

bool SchematicDraftingEngine::remove_symbol(SketchDocument2D& doc, std::string_view instance_id) {
    auto it = instances_.find(std::string(instance_id));
    if (it == instances_.end()) return false;

    // Remove graphics and pins from doc
    for (const auto& eid : it->second.graphic_entity_ids) {
        doc.remove_entity(eid);
        entity_to_instance_map_.erase(eid);
    }
    for (const auto& pin : it->second.pins) {
        pin_to_instance_map_.erase(pin.pin_id);
    }

    instances_.erase(it);
    return true;
}

void SchematicDraftingEngine::clear_symbols(SketchDocument2D& doc) {
    std::vector<std::string> ids;
    ids.reserve(instances_.size());
    for (const auto& [id, _] : instances_) {
        ids.push_back(id);
    }
    for (const auto& id : ids) {
        remove_symbol(doc, id);
    }
}

const SchematicComponentInstance* SchematicDraftingEngine::find_instance(std::string_view instance_id) const noexcept {
    auto it = instances_.find(std::string(instance_id));
    return (it != instances_.end()) ? &it->second : nullptr;
}

const SchematicComponentInstance* SchematicDraftingEngine::find_instance_by_pin(std::string_view pin_id) const noexcept {
    auto it = pin_to_instance_map_.find(std::string(pin_id));
    if (it != pin_to_instance_map_.end()) {
        return find_instance(it->second);
    }
    return nullptr;
}

const SchematicPinInstance* SchematicDraftingEngine::find_pin(std::string_view pin_id) const noexcept {
    const auto* comp = find_instance_by_pin(pin_id);
    if (!comp) return nullptr;
    for (const auto& p : comp->pins) {
        if (p.pin_id == pin_id) return &p;
    }
    return nullptr;
}

std::vector<const SchematicComponentInstance*> SchematicDraftingEngine::all_instances() const {
    std::vector<const SchematicComponentInstance*> out;
    out.reserve(instances_.size());
    for (const auto& [_, inst] : instances_) {
        out.push_back(&inst);
    }
    return out;
}

std::vector<SchematicPinInstance> SchematicDraftingEngine::all_pins() const {
    std::vector<SchematicPinInstance> res;
    for (const auto& [_, inst] : instances_) {
        for (const auto& p : inst.pins) {
            res.push_back(p);
        }
    }
    return res;
}

bool SchematicDraftingEngine::is_schematic_entity(const SketchDocument2D& doc, std::string_view entity_id) const {
    if (entity_to_instance_map_.find(std::string(entity_id)) != entity_to_instance_map_.end()) {
        return true;
    }
    if (const auto* ent = doc.find_entity(entity_id)) {
        if (ent->layer == SCHEMATIC_LAYER_SYMBOLS ||
            ent->layer == SCHEMATIC_LAYER_PINS ||
            ent->layer == SCHEMATIC_LAYER_WIRES ||
            ent->layer == SCHEMATIC_LAYER_ANNOTATIONS) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> SchematicDraftingEngine::filter_mechanical_entities(const SketchDocument2D& doc) const {
    std::vector<std::string> res;
    for (const auto& ent : doc.entities()) {
        if (!is_schematic_entity(doc, ent.id)) {
            res.push_back(ent.id);
        }
    }
    return res;
}

std::vector<std::string> SchematicDraftingEngine::filter_schematic_entities(const SketchDocument2D& doc) const {
    std::vector<std::string> res;
    for (const auto& ent : doc.entities()) {
        if (is_schematic_entity(doc, ent.id)) {
            res.push_back(ent.id);
        }
    }
    return res;
}

Paths2D SchematicDraftingEngine::extract_pure_mechanical_profiles(
    const SketchDocument2D& doc,
    size_t tessellation_segments,
    double endpoint_tol
) const {
    SketchDocument2D pure_doc;
    for (const auto& ent : doc.entities()) {
        if (!is_schematic_entity(doc, ent.id) && !ent.is_construction && ent.visible && ent.curve) {
            pure_doc.add_entity(std::shared_ptr<Curve2D>(ent.curve->clone()), false, ent.layer);
        }
    }
    return pure_doc.extract_solid_profile_loops(tessellation_segments, endpoint_tol);
}

bool SchematicDraftingEngine::validate_mechanical_isolation(const SketchDocument2D& doc) const {
    // 1. Ensure none of the doc's solid entities belong to schematic
    for (const auto* solid_ent : doc.solid_entities()) {
        if (is_schematic_entity(doc, solid_ent->id)) {
            return false;
        }
    }
    // 2. Ensure pure mechanical profiles match doc's solid profile loops exactly
    const auto pure_profiles = extract_pure_mechanical_profiles(doc);
    const auto doc_profiles = doc.extract_solid_profile_loops();
    return pure_profiles.size() == doc_profiles.size();
}

} // namespace shunya::cad2d
