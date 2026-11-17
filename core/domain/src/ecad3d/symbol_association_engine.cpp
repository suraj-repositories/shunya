#include "shunya/ecad3d/symbol_association_engine.hpp"

#include <algorithm>

namespace shunya::ecad3d {

namespace {

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
// ComponentSymbolCouplingEngine Implementation
// ============================================================================

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

    // 4. Invariant: PinTerminalMapper must be 100% complete and verified
    if (!mapper.is_complete()) {
        res.rejection_reasons.push_back("Pin-to-terminal mapping is incomplete or unverified");
        res.is_coupled = false;
    }

    return res;
}

} // namespace shunya::ecad3d
