#include "shunya/ecad3d/symbol_association_engine.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace shunya::ecad3d {

namespace {

constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;

inline core::base::Result<void> make_fail(std::string msg) {
    core::base::Error err;
    err.message = std::move(msg);
    err.domain = core::base::ErrorDomain::Validation;
    return core::base::Result<void>(std::move(err));
}

inline core::base::Result<void> make_ok() {
    return core::base::Result<void>();
}

} // namespace

// ============================================================================
// SchematicSymbolSynthesizer Implementation
// ============================================================================

SchematicSymbol2D SchematicSymbolSynthesizer::synthesize_symbol(
    eng::ComponentKind kind,
    std::string_view standard,
    int pin_count
) {
    SchematicSymbol2D sym;
    sym.standard = std::string(standard);
    sym.component_kind = kind;

    switch (kind) {
        case eng::ComponentKind::Resistor: {
            sym.symbol_id = "SYM_RESISTOR_" + sym.standard;
            sym.primitive_shape = (sym.standard == "IEC") ? "rectangle" : "zigzag";
            sym.bounding_box_size = {30.0, 10.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "1", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-15.0, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "2", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {15.0, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::Capacitor: {
            sym.symbol_id = "SYM_CAPACITOR_" + sym.standard;
            sym.primitive_shape = "parallel_plates";
            sym.bounding_box_size = {20.0, 15.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "+", core::domain::ElectricalPinPolarity::Positive, {-10.0, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "-", core::domain::ElectricalPinPolarity::Negative, {10.0, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::Inductor: {
            sym.symbol_id = "SYM_INDUCTOR_" + sym.standard;
            sym.primitive_shape = (sym.standard == "IEC") ? "filled_rectangle" : "inductor_loops";
            sym.bounding_box_size = {30.0, 12.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "1", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-15.0, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "2", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {15.0, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::Battery: {
            sym.symbol_id = "SYM_BATTERY_" + sym.standard;
            sym.primitive_shape = "battery_cells";
            sym.bounding_box_size = {25.0, 20.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "+", core::domain::ElectricalPinPolarity::Positive, {-12.5, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "-", core::domain::ElectricalPinPolarity::Negative, {12.5, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::Switch: {
            sym.symbol_id = "SYM_SWITCH_" + sym.standard;
            sym.primitive_shape = "spst_lever";
            sym.bounding_box_size = {25.0, 10.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "COM", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-12.5, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "NO", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {12.5, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::Diode:
        case eng::ComponentKind::LED:
        case eng::ComponentKind::ZenerDiode:
        case eng::ComponentKind::SchottkyDiode: {
            std::string prefix = (kind == eng::ComponentKind::LED) ? "SYM_LED_" :
                                 (kind == eng::ComponentKind::ZenerDiode) ? "SYM_ZENER_" :
                                 (kind == eng::ComponentKind::SchottkyDiode) ? "SYM_SCHOTTKY_" : "SYM_DIODE_";
            sym.symbol_id = prefix + sym.standard;
            sym.primitive_shape = (kind == eng::ComponentKind::LED) ? "led_diode_arrows" :
                                  (kind == eng::ComponentKind::ZenerDiode) ? "zener_diode_z" :
                                  (kind == eng::ComponentKind::SchottkyDiode) ? "schottky_diode_s" : "diode_triangle";
            sym.bounding_box_size = {25.0, 15.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "A", core::domain::ElectricalPinPolarity::Positive, {-12.5, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "K", core::domain::ElectricalPinPolarity::Negative, {12.5, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::BJT_NPN:
        case eng::ComponentKind::BJT_PNP: {
            sym.symbol_id = (kind == eng::ComponentKind::BJT_NPN) ? "SYM_BJT_NPN" : "SYM_BJT_PNP";
            sym.primitive_shape = (kind == eng::ComponentKind::BJT_NPN) ? "bjt_npn_circle" : "bjt_pnp_circle";
            sym.bounding_box_size = {30.0, 30.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "B", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-15.0, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "C", core::domain::ElectricalPinPolarity::Positive, {10.0, 15.0}, {0.0, 1.0}},
                SchematicSymbolPin{"3", 3, "E", core::domain::ElectricalPinPolarity::Negative, {10.0, -15.0}, {0.0, -1.0}}
            };
            break;
        }
        case eng::ComponentKind::MOSFET_N:
        case eng::ComponentKind::MOSFET_P: {
            sym.symbol_id = (kind == eng::ComponentKind::MOSFET_N) ? "SYM_MOSFET_N" : "SYM_MOSFET_P";
            sym.primitive_shape = (kind == eng::ComponentKind::MOSFET_N) ? "mosfet_n_channel" : "mosfet_p_channel";
            sym.bounding_box_size = {30.0, 30.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "G", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-15.0, -5.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "D", core::domain::ElectricalPinPolarity::Positive, {10.0, 15.0}, {0.0, 1.0}},
                SchematicSymbolPin{"3", 3, "S", core::domain::ElectricalPinPolarity::Negative, {10.0, -15.0}, {0.0, -1.0}}
            };
            break;
        }
        case eng::ComponentKind::JFET_N:
        case eng::ComponentKind::JFET_P: {
            sym.symbol_id = (kind == eng::ComponentKind::JFET_N) ? "SYM_JFET_N" : "SYM_JFET_P";
            sym.primitive_shape = (kind == eng::ComponentKind::JFET_N) ? "jfet_n_channel" : "jfet_p_channel";
            sym.bounding_box_size = {30.0, 30.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "G", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-15.0, -5.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "D", core::domain::ElectricalPinPolarity::Positive, {10.0, 15.0}, {0.0, 1.0}},
                SchematicSymbolPin{"3", 3, "S", core::domain::ElectricalPinPolarity::Negative, {10.0, -15.0}, {0.0, -1.0}}
            };
            break;
        }
        case eng::ComponentKind::PushButton: {
            sym.symbol_id = "SYM_PUSHBUTTON_" + sym.standard;
            sym.primitive_shape = "push_button_no";
            sym.bounding_box_size = {25.0, 10.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "COM", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-12.5, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "NO", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {12.5, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::Relay: {
            sym.symbol_id = "SYM_RELAY_SPDT";
            sym.primitive_shape = "relay_coil_spdt";
            sym.bounding_box_size = {35.0, 25.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "COIL+", core::domain::ElectricalPinPolarity::Positive, {-15.0, 10.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "COIL-", core::domain::ElectricalPinPolarity::Negative, {-15.0, -10.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"3", 3, "COM", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {15.0, 0.0}, {1.0, 0.0}},
                SchematicSymbolPin{"4", 4, "NO", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {15.0, 10.0}, {1.0, 0.0}},
                SchematicSymbolPin{"5", 5, "NC", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {15.0, -10.0}, {1.0, 0.0}}
            };
            break;
        }
        case eng::ComponentKind::DCVoltageSource: {
            sym.symbol_id = "SYM_DC_SOURCE";
            sym.primitive_shape = "dc_source_circle";
            sym.bounding_box_size = {25.0, 25.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "+", core::domain::ElectricalPinPolarity::Positive, {0.0, 12.5}, {0.0, 1.0}},
                SchematicSymbolPin{"2", 2, "-", core::domain::ElectricalPinPolarity::Negative, {0.0, -12.5}, {0.0, -1.0}}
            };
            break;
        }
        case eng::ComponentKind::ACVoltageSource: {
            sym.symbol_id = "SYM_AC_SOURCE";
            sym.primitive_shape = "ac_source_sine";
            sym.bounding_box_size = {25.0, 25.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "L", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {0.0, 12.5}, {0.0, 1.0}},
                SchematicSymbolPin{"2", 2, "N", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {0.0, -12.5}, {0.0, -1.0}}
            };
            break;
        }
        case eng::ComponentKind::GroundReference: {
            sym.symbol_id = "SYM_GROUND";
            sym.primitive_shape = "ground_triangle";
            sym.bounding_box_size = {20.0, 20.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "GND", core::domain::ElectricalPinPolarity::Negative, {0.0, 10.0}, {0.0, 1.0}}
            };
            break;
        }
        case eng::ComponentKind::Fuse: {
            sym.symbol_id = "SYM_FUSE_" + sym.standard;
            sym.primitive_shape = "fuse_wire";
            sym.bounding_box_size = {25.0, 10.0};
            sym.pins = {
                SchematicSymbolPin{"1", 1, "1", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-12.5, 0.0}, {-1.0, 0.0}},
                SchematicSymbolPin{"2", 2, "2", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {12.5, 0.0}, {1.0, 0.0}}
            };
            break;
        }
        default: {
            sym.symbol_id = "SYM_GENERIC_" + std::to_string(pin_count);
            sym.primitive_shape = "ic_box";
            double height = std::max(20.0, static_cast<double>(pin_count) * 10.0);
            sym.bounding_box_size = {40.0, height};
            sym.pins.clear();
            const int half = pin_count / 2;
            for (int i = 0; i < pin_count; ++i) {
                SchematicSymbolPin p;
                p.pin_number = i + 1;
                p.pin_id = std::to_string(p.pin_number);
                p.pin_label = "P" + p.pin_id;
                p.polarity = core::domain::ElectricalPinPolarity::PassiveBiDirectional;
                if (i < half) {
                    p.position = {-20.0, -height * 0.5 + 10.0 * (i + 1)};
                    p.orientation = {-1.0, 0.0};
                } else {
                    p.position = {20.0, -height * 0.5 + 10.0 * (pin_count - i)};
                    p.orientation = {1.0, 0.0};
                }
                sym.pins.push_back(p);
            }
            break;
        }
    }

    return sym;
}

// ============================================================================
// PinTerminalMapper Implementation
// ============================================================================

void PinTerminalMapper::initialize(
    const std::vector<PhysicalTerminal3D>& terminals,
    const SchematicSymbol2D& symbol
) {
    terminals_ = terminals;
    symbol_ = symbol;
    mappings_.clear();
    mappings_.reserve(terminals.size());

    for (const auto& t : terminals) {
        PinTerminalMappingEntry entry;
        entry.terminal_3d_id = t.terminal_id;
        entry.pin_number = t.pin_number;
        entry.verified = false;
        mappings_.push_back(entry);
    }
}

bool PinTerminalMapper::is_complete() const noexcept {
    if (mappings_.empty() || mappings_.size() != symbol_.pins.size()) {
        return false;
    }
    for (const auto& m : mappings_) {
        if (!m.verified || m.symbol_pin_id.empty()) {
            return false;
        }
    }
    return true;
}

core::base::Result<void> PinTerminalMapper::map_pin(
    const std::string& terminal_3d_id,
    const std::string& symbol_pin_id
) {
    auto term_it = std::find_if(mappings_.begin(), mappings_.end(), [&](const PinTerminalMappingEntry& e) {
        return e.terminal_3d_id == terminal_3d_id;
    });
    if (term_it == mappings_.end()) {
        return make_fail("Terminal 3D ID '" + terminal_3d_id + "' not found");
    }

    auto sym_it = std::find_if(symbol_.pins.begin(), symbol_.pins.end(), [&](const SchematicSymbolPin& p) {
        return p.pin_id == symbol_pin_id;
    });
    if (sym_it == symbol_.pins.end()) {
        return make_fail("Symbol Pin ID '" + symbol_pin_id + "' not found");
    }

    term_it->symbol_pin_id = symbol_pin_id;
    term_it->verified = true;
    return make_ok();
}

core::base::Result<void> PinTerminalMapper::auto_map_by_number_and_polarity() {
    if (terminals_.empty() || symbol_.pins.empty()) {
        return make_fail("Empty terminals or pins list");
    }

    for (auto& m : mappings_) {
        // Find 3D terminal
        auto term_it = std::find_if(terminals_.begin(), terminals_.end(), [&](const PhysicalTerminal3D& t) {
            return t.terminal_id == m.terminal_3d_id;
        });
        if (term_it == terminals_.end()) continue;

        // First attempt match by exact pin number
        auto sym_it = std::find_if(symbol_.pins.begin(), symbol_.pins.end(), [&](const SchematicSymbolPin& p) {
            return p.pin_number == term_it->pin_number;
        });

        // Second attempt match by polarity
        if (sym_it == symbol_.pins.end()) {
            sym_it = std::find_if(symbol_.pins.begin(), symbol_.pins.end(), [&](const SchematicSymbolPin& p) {
                return p.polarity == term_it->polarity;
            });
        }

        if (sym_it != symbol_.pins.end()) {
            m.symbol_pin_id = sym_it->pin_id;
            m.verified = true;
        }
    }

    return make_ok();
}

std::optional<std::string> PinTerminalMapper::get_symbol_pin_for_terminal(const std::string& terminal_3d_id) const {
    for (const auto& m : mappings_) {
        if (m.terminal_3d_id == terminal_3d_id && m.verified) {
            return m.symbol_pin_id;
        }
    }
    return std::nullopt;
}

std::optional<std::string> PinTerminalMapper::get_terminal_for_symbol_pin(const std::string& symbol_pin_id) const {
    for (const auto& m : mappings_) {
        if (m.symbol_pin_id == symbol_pin_id && m.verified) {
            return m.terminal_3d_id;
        }
    }
    return std::nullopt;
}

// ============================================================================
// SpatialMountingOrientation Implementation
// ============================================================================

cad3d::Point3D SpatialMountingOrientation::transform_terminal_to_pcb(
    const cad3d::Point3D& local_pos,
    double yaw_deg,
    bool mirror_bottom
) const {
    double total_yaw = (default_rotation_deg + yaw_deg) * DEG_TO_RAD;
    double cos_theta = std::cos(total_yaw);
    double sin_theta = std::sin(total_yaw);

    // Apply local center offset
    double x = local_pos.x - center_offset.x;
    double y = local_pos.y - center_offset.y;
    double z = local_pos.z - center_offset.z;

    // Apply mounting plane transformation
    cad3d::Point3D pcb_pos;
    switch (mounting_plane) {
        case MountingPlane::XY_TopSurface:
            pcb_pos.x = x * cos_theta - y * sin_theta;
            pcb_pos.y = x * sin_theta + y * cos_theta;
            pcb_pos.z = z + seating_height_mm;
            break;
        case MountingPlane::XY_BottomSurface:
            // Mirrored across X for bottom mount
            pcb_pos.x = -(x * cos_theta - y * sin_theta);
            pcb_pos.y = x * sin_theta + y * cos_theta;
            pcb_pos.z = -(z + seating_height_mm);
            break;
        case MountingPlane::XZ_Vertical:
            // Component standing upright along Z, footprint along X
            pcb_pos.x = x * cos_theta - z * sin_theta;
            pcb_pos.y = y;
            pcb_pos.z = x * sin_theta + z * cos_theta + seating_height_mm;
            break;
        case MountingPlane::YZ_Vertical:
            pcb_pos.x = x;
            pcb_pos.y = y * cos_theta - z * sin_theta;
            pcb_pos.z = y * sin_theta + z * cos_theta + seating_height_mm;
            break;
        case MountingPlane::ThroughHole_Z:
            pcb_pos.x = x * cos_theta - y * sin_theta;
            pcb_pos.y = x * sin_theta + y * cos_theta;
            pcb_pos.z = z - std::abs(seating_height_mm); // Penetrating through PCB
            break;
    }

    if (mirror_bottom && mounting_plane != MountingPlane::XY_BottomSurface) {
        pcb_pos.x = -pcb_pos.x;
    }

    return pcb_pos;
}

std::pair<cad3d::Point3D, cad3d::Point3D> SpatialMountingOrientation::compute_mounting_footprint_bounds(
    const std::vector<PhysicalTerminal3D>& terminals,
    double clearance_mm
) const {
    if (terminals.empty()) {
        return {cad3d::Point3D{-clearance_mm, -clearance_mm, 0.0},
                cad3d::Point3D{clearance_mm, clearance_mm, 0.0}};
    }

    double min_x = 1e9, min_y = 1e9, min_z = 1e9;
    double max_x = -1e9, max_y = -1e9, max_z = -1e9;

    for (const auto& term : terminals) {
        auto pcb_p = transform_terminal_to_pcb(term.contact_point);
        min_x = std::min(min_x, pcb_p.x);
        min_y = std::min(min_y, pcb_p.y);
        min_z = std::min(min_z, pcb_p.z);
        max_x = std::max(max_x, pcb_p.x);
        max_y = std::max(max_y, pcb_p.y);
        max_z = std::max(max_z, pcb_p.z);
    }

    return {
        cad3d::Point3D{min_x - clearance_mm, min_y - clearance_mm, min_z},
        cad3d::Point3D{max_x + clearance_mm, max_y + clearance_mm, max_z}
    };
}

// ============================================================================
// SelectionHighlightCoordinator Implementation
// ============================================================================

void SelectionHighlightCoordinator::select_from_3d(
    const std::string& terminal_3d_id,
    const PinTerminalMapper& mapper
) {
    current_.source = SelectionSource::Viewport3D;
    current_.terminal_3d_id = terminal_3d_id;

    auto sym_pin_opt = mapper.get_symbol_pin_for_terminal(terminal_3d_id);
    if (sym_pin_opt.has_value()) {
        current_.symbol_pin_id = sym_pin_opt.value();
    } else {
        current_.symbol_pin_id.clear();
    }

    // Lookup pin number if mapped
    for (const auto& m : mapper.mappings()) {
        if (m.terminal_3d_id == terminal_3d_id) {
            current_.pin_number = m.pin_number;
            break;
        }
    }

    if (listener_) {
        listener_(current_);
    }
}

void SelectionHighlightCoordinator::select_from_2d(
    const std::string& symbol_pin_id,
    const PinTerminalMapper& mapper
) {
    current_.source = SelectionSource::Schematic2D;
    current_.symbol_pin_id = symbol_pin_id;

    auto term_opt = mapper.get_terminal_for_symbol_pin(symbol_pin_id);
    if (term_opt.has_value()) {
        current_.terminal_3d_id = term_opt.value();
    } else {
        current_.terminal_3d_id.clear();
    }

    for (const auto& m : mapper.mappings()) {
        if (m.symbol_pin_id == symbol_pin_id) {
            current_.pin_number = m.pin_number;
            break;
        }
    }

    if (listener_) {
        listener_(current_);
    }
}

void SelectionHighlightCoordinator::select_by_pin_number(
    int pin_number,
    const PinTerminalMapper& mapper
) {
    current_.source = SelectionSource::None;
    current_.pin_number = pin_number;
    current_.terminal_3d_id.clear();
    current_.symbol_pin_id.clear();

    for (const auto& m : mapper.mappings()) {
        if (m.pin_number == pin_number) {
            current_.terminal_3d_id = m.terminal_3d_id;
            current_.symbol_pin_id = m.symbol_pin_id;
            break;
        }
    }

    if (listener_) {
        listener_(current_);
    }
}

void SelectionHighlightCoordinator::clear_selection() {
    current_ = SelectionHighlightEvent{};
    if (listener_) {
        listener_(current_);
    }
}

// ============================================================================
// ComponentSymbolCouplingEngine Implementation
// ============================================================================

ComponentSymbolCouplingEngine::PinParityReport ComponentSymbolCouplingEngine::verify_pin_parity(
    const ComponentAuthoringData& component_3d,
    const SchematicSymbol2D& symbol_2d
) {
    PinParityReport report;
    report.is_valid = true;

    // 1. Total terminal vs pin count parity
    if (component_3d.terminals.size() != symbol_2d.pins.size()) {
        report.is_valid = false;
        report.parity_violations.push_back(
            "Pin count parity mismatch: 3D model defines " +
            std::to_string(component_3d.terminals.size()) +
            " terminals, but 2D symbol defines " +
            std::to_string(symbol_2d.pins.size()) + " pins"
        );
    }

    // 2. Check duplicate pin numbers in 3D terminals
    std::set<int> term_pin_numbers;
    for (const auto& t : component_3d.terminals) {
        if (term_pin_numbers.count(t.pin_number) > 0) {
            report.is_valid = false;
            report.parity_violations.push_back(
                "Duplicate 3D terminal pin number: " + std::to_string(t.pin_number)
            );
        }
        term_pin_numbers.insert(t.pin_number);
    }

    // 3. Check duplicate pin numbers in 2D symbol pins
    std::set<int> sym_pin_numbers;
    for (const auto& p : symbol_2d.pins) {
        if (sym_pin_numbers.count(p.pin_number) > 0) {
            report.is_valid = false;
            report.parity_violations.push_back(
                "Duplicate 2D schematic symbol pin number: " + std::to_string(p.pin_number)
            );
        }
        sym_pin_numbers.insert(p.pin_number);
    }

    // 4. Exact pin numbering set parity
    for (int num : term_pin_numbers) {
        if (sym_pin_numbers.count(num) == 0) {
            report.is_valid = false;
            report.parity_violations.push_back(
                "Pin number parity failure: 3D terminal pin " + std::to_string(num) +
                " does not exist in 2D symbol"
            );
        }
    }
    for (int num : sym_pin_numbers) {
        if (term_pin_numbers.count(num) == 0) {
            report.is_valid = false;
            report.parity_violations.push_back(
                "Pin number parity failure: 2D symbol pin " + std::to_string(num) +
                " does not exist in 3D model"
            );
        }
    }

    // 5. Polarity parity
    for (const auto& t : component_3d.terminals) {
        auto it = std::find_if(symbol_2d.pins.begin(), symbol_2d.pins.end(), [&](const SchematicSymbolPin& p) {
            return p.pin_number == t.pin_number;
        });
        if (it != symbol_2d.pins.end()) {
            if (t.polarity != core::domain::ElectricalPinPolarity::PassiveBiDirectional &&
                it->polarity != core::domain::ElectricalPinPolarity::PassiveBiDirectional &&
                t.polarity != it->polarity) {
                report.is_valid = false;
                report.parity_violations.push_back(
                    "Pin polarity conflict on pin " + std::to_string(t.pin_number) +
                    ": 3D terminal polarity differs from 2D symbol pin polarity"
                );
            }
        }
    }

    return report;
}

ComponentSymbolCouplingEngine::VerificationResult ComponentSymbolCouplingEngine::verify_coupling(
    const ComponentAuthoringData& component_3d,
    const SchematicSymbol2D& symbol_2d,
    const PinTerminalMapper& mapper
) {
    VerificationResult res;
    res.is_coupled = true;

    // 1. Invariant: 3D model geometry must be non-empty
    if (component_3d.mesh_geometry.vertices.empty() || component_3d.mesh_geometry.triangles.empty()) {
        res.rejection_reasons.push_back("3D component lacks valid geometric mesh representation");
        res.is_coupled = false;
    }

    // 2. Invariant: 2D symbol must be non-empty
    if (symbol_2d.pins.empty()) {
        res.rejection_reasons.push_back("Coupled 2D schematic symbol contains zero pins");
        res.is_coupled = false;
    }

    // 3. Invariant: Terminal count must equal symbol pin count
    if (component_3d.terminals.size() != symbol_2d.pins.size()) {
        res.rejection_reasons.push_back(
            "Terminal count mismatch: 3D model has " + std::to_string(component_3d.terminals.size()) +
            " terminals, while 2D symbol has " + std::to_string(symbol_2d.pins.size()) + " pins"
        );
        res.is_coupled = false;
    }

    // 4. Invariant: Pin parity verification
    auto parity = verify_pin_parity(component_3d, symbol_2d);
    if (!parity.is_valid) {
        for (auto& err : parity.parity_violations) {
            res.rejection_reasons.push_back(std::move(err));
        }
        res.is_coupled = false;
    }

    // 5. Invariant: PinTerminalMapper must be 100% complete and verified
    if (!mapper.is_complete()) {
        res.rejection_reasons.push_back("Pin-to-terminal mapping is incomplete or unverified");
        res.is_coupled = false;
    }

    return res;
}

} // namespace shunya::ecad3d
