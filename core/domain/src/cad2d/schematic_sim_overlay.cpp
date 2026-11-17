#include "shunya/cad2d/schematic_sim_overlay.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::cad2d {

namespace {

std::string format_float(double val, int precision = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << val;
    return ss.str();
}

} // namespace

std::string SchematicSimOverlay::format_voltage(double volts) {
    const double a = std::abs(volts);
    if (a < 1e-9) return "0.00 V";
    if (a >= 1e3) return format_float(volts * 1e-3) + " kV";
    if (a >= 1.0) return format_float(volts) + " V";
    if (a >= 1e-3) return format_float(volts * 1e3) + " mV";
    return format_float(volts * 1e6) + " uV";
}

std::string SchematicSimOverlay::format_current(double amperes) {
    const double a = std::abs(amperes);
    if (a < 1e-12) return "0.00 A";
    if (a >= 1.0) return format_float(amperes) + " A";
    if (a >= 1e-3) return format_float(amperes * 1e3) + " mA";
    if (a >= 1e-6) return format_float(amperes * 1e6) + " uA";
    return format_float(amperes * 1e9) + " nA";
}

std::string SchematicSimOverlay::format_power(double watts) {
    const double a = std::abs(watts);
    if (a < 1e-12) return "0.00 W";
    if (a >= 1.0) return format_float(watts) + " W";
    if (a >= 1e-3) return format_float(watts * 1e3) + " mW";
    return format_float(watts * 1e6) + " uW";
}

std::string SchematicSimOverlay::voltage_to_color(double volts) {
    if (volts > 12.0) return "#ff5555";     // Red
    if (volts >= 4.0) return "#ffb86c";     // Orange
    if (volts >= 1.5) return "#50fa7b";     // Green
    if (volts >= 0.0) return "#8be9fd";     // Cyan
    return "#bd93f9";                       // Purple (negative)
}

void SchematicSimOverlay::clear_overlay(SketchDocument2D& doc) {
    for (const auto& vb : voltage_badges_) {
        if (!vb.graphic_entity_id.empty()) doc.remove_entity(vb.graphic_entity_id);
    }
    voltage_badges_.clear();

    for (const auto& cp : current_probes_) {
        if (!cp.graphic_entity_id.empty()) doc.remove_entity(cp.graphic_entity_id);
    }
    current_probes_.clear();

    for (const auto& pb : power_badges_) {
        if (!pb.graphic_entity_id.empty()) doc.remove_entity(pb.graphic_entity_id);
    }
    power_badges_.clear();
}

void SchematicSimOverlay::render_overlay(
    SketchDocument2D& doc,
    const core::domain::DCOperatingPointResult& sim_result,
    const SchematicDraftingEngine& symbol_engine,
    const SchematicWireRouter& wire_router,
    const SchematicPinPlacementTool* pin_tool
) {
    clear_overlay(doc);
    const std::string layer = std::string(SchematicDraftingEngine::SCHEMATIC_LAYER_ANNOTATIONS);

    // 1. Render Live Node Voltages
    if (show_voltages_) {
        for (const auto& [node, v] : sim_result.node_voltages) {
            Point2D anchor{0.0, 0.0};
            bool found = false;

            // Check wire network
            for (const auto& w : wire_router.wires()) {
                if (w.net_name == node && !w.points.empty()) {
                    anchor = w.points.front();
                    found = true;
                    break;
                }
            }

            // Check symbol pins
            if (!found) {
                for (const auto* inst : symbol_engine.all_instances()) {
                    for (const auto& p : inst->pins) {
                        if (p.pin_name == node || (inst->reference_designator + "." + p.pin_name) == node) {
                            anchor = p.world_pos;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }

            // Check standalone pin tool
            if (!found && pin_tool) {
                for (const auto& p : pin_tool->all_pins()) {
                    if (p.pin_name == node) {
                        anchor = pin_tool->terminal_anchor(p);
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                VoltageBadgeItem item;
                item.node_name = node;
                item.position = anchor;
                item.voltage_v = v;
                item.formatted_text = format_voltage(v);
                item.color_hex = voltage_to_color(v);

                // Add small voltage badge pill on canvas
                auto pill = std::make_shared<Circle2D>(anchor, 1.0);
                item.graphic_entity_id = doc.add_entity(pill, true, layer);

                voltage_badges_.push_back(std::move(item));
            }
        }
    }

    // 2. Render Live Branch Currents
    if (show_currents_) {
        for (const auto& [comp_id, i_val] : sim_result.branch_currents) {
            Point2D pos{0.0, 0.0};
            bool found = false;

            if (const auto* inst = symbol_engine.find_instance(comp_id)) {
                pos = inst->position;
                found = true;
            } else {
                for (const auto* inst : symbol_engine.all_instances()) {
                    if (inst->reference_designator == comp_id) {
                        pos = inst->position;
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                CurrentProbeItem item;
                item.component_or_wire_id = comp_id;
                item.position = pos;
                item.direction_arrow_end = {pos.x + 3.0, pos.y};
                item.current_a = i_val;
                item.formatted_text = format_current(i_val);

                // Small directional arrow line segment
                auto arrow = std::make_shared<LineSegment2D>(pos, item.direction_arrow_end);
                item.graphic_entity_id = doc.add_entity(arrow, true, layer);

                current_probes_.push_back(std::move(item));
            }
        }
    }

    // 3. Render Live Component Power Dissipation
    if (show_power_) {
        for (const auto& [comp_id, p_val] : sim_result.component_power) {
            Point2D pos{0.0, 0.0};
            bool found = false;

            for (const auto* inst : symbol_engine.all_instances()) {
                if (inst->reference_designator == comp_id || inst->instance_id == comp_id) {
                    pos = {inst->position.x, inst->position.y + 4.0};
                    found = true;
                    break;
                }
            }

            if (found) {
                PowerBadgeItem item;
                item.component_id = comp_id;
                item.position = pos;
                item.power_w = p_val;
                item.formatted_text = format_power(p_val);

                auto power_circ = std::make_shared<Circle2D>(pos, 0.8);
                item.graphic_entity_id = doc.add_entity(power_circ, true, layer);

                power_badges_.push_back(std::move(item));
            }
        }
    }
}

ProbeMeasurement SchematicSimOverlay::probe_at(
    Point2D pos,
    const core::domain::DCOperatingPointResult& sim_result,
    const SchematicDraftingEngine& symbol_engine,
    const SchematicWireRouter& wire_router,
    double tol
) const {
    const double tol_sq = tol * tol;

    // 1. Check components
    for (const auto* inst : symbol_engine.all_instances()) {
        const double d2 = (pos.x - inst->position.x) * (pos.x - inst->position.x) + (pos.y - inst->position.y) * (pos.y - inst->position.y);
        if (d2 <= tol_sq) {
            ProbeMeasurement m;
            m.has_data = true;
            m.target_label = inst->reference_designator;
            m.current_a = sim_result.get_current(inst->reference_designator);
            m.power_w = sim_result.get_power(inst->reference_designator);
            m.formatted_summary = inst->reference_designator + ": I=" + format_current(m.current_a) + ", P=" + format_power(m.power_w);
            return m;
        }
    }

    // 2. Check wires
    for (const auto& w : wire_router.wires()) {
        for (size_t i = 0; i + 1 < w.points.size(); ++i) {
            const Point2D& p0 = w.points[i];
            const Point2D& p1 = w.points[i + 1];
            const double l2 = (p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y);
            double t = (l2 < 1e-12) ? 0.0 : std::clamp(((pos.x - p0.x) * (p1.x - p0.x) + (pos.y - p0.y) * (p1.y - p0.y)) / l2, 0.0, 1.0);
            const Point2D proj = {p0.x + t * (p1.x - p0.x), p0.y + t * (p1.y - p0.y)};
            const double d2 = (pos.x - proj.x) * (pos.x - proj.x) + (pos.y - proj.y) * (pos.y - proj.y);
            if (d2 <= tol_sq) {
                ProbeMeasurement m;
                m.has_data = true;
                m.target_label = "Net: " + w.net_name;
                m.voltage_v = sim_result.get_voltage(w.net_name);
                m.formatted_summary = w.net_name + ": V=" + format_voltage(m.voltage_v);
                return m;
            }
        }
    }

    return ProbeMeasurement{};
}

} // namespace shunya::cad2d
