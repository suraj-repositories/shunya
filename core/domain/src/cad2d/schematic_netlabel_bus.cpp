#include "shunya/cad2d/schematic_netlabel_bus.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

bool BusDefinition::contains_net(std::string_view net_name) const noexcept {
    for (const auto& m : member_nets) {
        if (m == net_name) return true;
    }
    return false;
}

std::optional<BusDefinition> SchematicNetlabelBusManager::parse_bus_spec(std::string_view spec) {
    auto b_open = spec.find('[');
    auto b_close = spec.find(']');
    if (b_open == std::string_view::npos || b_close == std::string_view::npos || b_close <= b_open) {
        return std::nullopt;
    }

    std::string prefix = std::string(spec.substr(0, b_open));
    std::string_view inside = spec.substr(b_open + 1, b_close - b_open - 1);
    auto dotdot = inside.find("..");
    if (dotdot == std::string_view::npos) {
        return std::nullopt;
    }

    try {
        int s = std::stoi(std::string(inside.substr(0, dotdot)));
        int e = std::stoi(std::string(inside.substr(dotdot + 2)));

        BusDefinition def;
        def.prefix = prefix;
        def.start_index = s;
        def.end_index = e;
        def.raw_spec = std::string(spec);

        int low = std::min(s, e);
        int high = std::max(s, e);
        for (int i = low; i <= high; ++i) {
            def.member_nets.push_back(prefix + std::to_string(i));
        }
        return def;
    } catch (...) {
        return std::nullopt;
    }
}

std::string SchematicNetlabelBusManager::place_netlabel(
    SketchDocument2D& doc,
    Point2D anchor,
    std::string net_name,
    NetLabelType type
) {
    if (net_name.empty()) return {};

    NetLabel lbl;
    lbl.label_id = "sch_lbl_" + std::to_string(next_label_idx_++);
    lbl.name = std::move(net_name);
    lbl.position = anchor;
    lbl.type = type;

    // Synthesize visual callout geometry: small triangular flag or anchor marker
    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_ANNOTATIONS);
    const double sz = (type == NetLabelType::Global) ? 2.0 : 1.5;

    // Triangular flag indicator: anchor -> (anchor.x + sz, anchor.y + sz*0.5) -> (anchor.x + sz, anchor.y - sz*0.5) -> anchor
    std::vector<Point2D> flag_pts = {
        anchor,
        {anchor.x + sz, anchor.y + sz * 0.5},
        {anchor.x + sz, anchor.y - sz * 0.5}
    };
    auto poly = std::make_shared<Polyline2D>(std::move(flag_pts), true);
    std::string eid = doc.add_entity(poly, true, layer);
    lbl.graphic_entity_ids.push_back(std::move(eid));

    const std::string id = lbl.label_id;
    labels_.push_back(std::move(lbl));
    return id;
}

bool SchematicNetlabelBusManager::remove_netlabel(SketchDocument2D& doc, std::string_view label_id) {
    auto it = std::find_if(labels_.begin(), labels_.end(), [&](const NetLabel& l) {
        return l.label_id == label_id;
    });
    if (it == labels_.end()) return false;

    for (const auto& eid : it->graphic_entity_ids) {
        doc.remove_entity(eid);
    }
    labels_.erase(it);
    return true;
}

std::string SchematicNetlabelBusManager::place_bus(
    SketchDocument2D& doc,
    const std::vector<Point2D>& waypoints,
    std::string bus_spec
) {
    if (waypoints.size() < 2) return {};
    auto parsed = parse_bus_spec(bus_spec);
    if (!parsed.has_value()) return {};

    BusDefinition bus = std::move(*parsed);
    bus.bus_id = "sch_bus_" + std::to_string(next_bus_idx_++);
    bus.waypoints = waypoints;

    // Thicker bus wire path in document
    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_WIRES);
    for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
        auto seg = std::make_shared<LineSegment2D>(waypoints[i], waypoints[i + 1]);
        std::string eid = doc.add_entity(seg, true, layer);
        bus.graphic_entity_ids.push_back(std::move(eid));
    }

    const std::string id = bus.bus_id;
    buses_.push_back(std::move(bus));
    return id;
}

bool SchematicNetlabelBusManager::remove_bus(SketchDocument2D& doc, std::string_view bus_id) {
    auto it = std::find_if(buses_.begin(), buses_.end(), [&](const BusDefinition& b) {
        return b.bus_id == bus_id;
    });
    if (it == buses_.end()) return false;

    for (const auto& eid : it->graphic_entity_ids) {
        doc.remove_entity(eid);
    }
    // Remove attached taps
    std::vector<std::string> taps_to_remove;
    for (const auto& t : bus_taps_) {
        if (t.bus_id == bus_id) taps_to_remove.push_back(t.tap_id);
    }
    for (const auto& tid : taps_to_remove) {
        remove_bus_tap(doc, tid);
    }

    buses_.erase(it);
    return true;
}

std::string SchematicNetlabelBusManager::place_bus_tap(
    SketchDocument2D& doc,
    std::string_view bus_id,
    Point2D bus_pos,
    std::string member_net,
    double tap_length_mm,
    bool angle_45_deg
) {
    const auto* bus = find_bus(bus_id);
    if (!bus || !bus->contains_net(member_net)) {
        return {};
    }

    BusTap tap;
    tap.tap_id = "sch_tap_" + std::to_string(next_tap_idx_++);
    tap.bus_id = std::string(bus_id);
    tap.member_net = std::move(member_net);
    tap.bus_contact_pos = bus_pos;

    if (angle_45_deg) {
        tap.tap_end_pos = {bus_pos.x + tap_length_mm * 0.70710678, bus_pos.y + tap_length_mm * 0.70710678};
    } else {
        tap.tap_end_pos = {bus_pos.x + tap_length_mm, bus_pos.y};
    }

    // 45-deg entry tap line
    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_WIRES);
    auto tap_seg = std::make_shared<LineSegment2D>(tap.bus_contact_pos, tap.tap_end_pos);
    std::string eid = doc.add_entity(tap_seg, true, layer);
    tap.graphic_entity_ids.push_back(std::move(eid));

    const std::string id = tap.tap_id;
    bus_taps_.push_back(std::move(tap));
    return id;
}

bool SchematicNetlabelBusManager::remove_bus_tap(SketchDocument2D& doc, std::string_view tap_id) {
    auto it = std::find_if(bus_taps_.begin(), bus_taps_.end(), [&](const BusTap& t) {
        return t.tap_id == tap_id;
    });
    if (it == bus_taps_.end()) return false;

    for (const auto& eid : it->graphic_entity_ids) {
        doc.remove_entity(eid);
    }
    bus_taps_.erase(it);
    return true;
}

const NetLabel* SchematicNetlabelBusManager::find_netlabel(std::string_view label_id) const noexcept {
    for (const auto& l : labels_) {
        if (l.label_id == label_id) return &l;
    }
    return nullptr;
}

const BusDefinition* SchematicNetlabelBusManager::find_bus(std::string_view bus_id) const noexcept {
    for (const auto& b : buses_) {
        if (b.bus_id == bus_id) return &b;
    }
    return nullptr;
}

const BusTap* SchematicNetlabelBusManager::find_bus_tap(std::string_view tap_id) const noexcept {
    for (const auto& t : bus_taps_) {
        if (t.tap_id == tap_id) return &t;
    }
    return nullptr;
}

std::vector<Point2D> SchematicNetlabelBusManager::find_all_positions_for_net(std::string_view net_name) const {
    std::vector<Point2D> pts;
    for (const auto& l : labels_) {
        if (l.name == net_name) {
            pts.push_back(l.position);
        }
    }
    for (const auto& t : bus_taps_) {
        if (t.member_net == net_name) {
            pts.push_back(t.tap_end_pos);
        }
    }
    return pts;
}

void SchematicNetlabelBusManager::clear(SketchDocument2D& doc) {
    for (const auto& l : labels_) {
        for (const auto& eid : l.graphic_entity_ids) doc.remove_entity(eid);
    }
    labels_.clear();
    for (const auto& b : buses_) {
        for (const auto& eid : b.graphic_entity_ids) doc.remove_entity(eid);
    }
    buses_.clear();
    for (const auto& t : bus_taps_) {
        for (const auto& eid : t.graphic_entity_ids) doc.remove_entity(eid);
    }
    bus_taps_.clear();
}

} // namespace shunya::cad2d
