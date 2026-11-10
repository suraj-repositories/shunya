#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/pcb/board_stackup.hpp"
#include "shunya/pcb/parasitic_extractor.hpp"
#include "shunya/pcb/push_shove_engine.hpp"
#include "shunya/pcb/schematic_sheet.hpp"
#include "shunya/pcb/via_manager.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

struct ComponentSpiceSpec {
    std::string ref_des;
    core::domain::ElectricalComponentType type{core::domain::ElectricalComponentType::Resistor};
    double value_primary{1000.0};
    double value_secondary{0.0};
    double value_tertiary{0.0};
    core::domain::WaveformParams waveform{};
    // Pin order mapping from schematic pin_number to electrical terminal order (p, q, or d/g/s)
    std::vector<std::string> pin_order{"1", "2"};
    double is_saturation_current{1e-14};
    double ideality_factor{1.0};
    double mos_vth{1.0};
    double mos_kp{1e-3};
};

struct CompiledSpiceBoardResult {
    core::domain::CircuitNetlist circuit;
    std::string spice_deck;
    std::unordered_map<std::string, NetParasiticSummary> net_parasitics;
};

/**
 * @brief Compiles multi-sheet schematics and optional PCB layout trace/via parasitics into a SPICE CircuitNetlist.
 */
class NetlistCompiler {
public:
    /**
     * @brief Compiles schematic connectivity + component models + optional routed PCB parasitics.
     *
     * When include_trace_parasitics is true and a non-ground net has routed copper parasitics above threshold,
     * a lumped R_trace - L_trace series branch and C_trace shunt capacitor are inserted automatically.
     */
    [[nodiscard]] static core::base::Result<CompiledSpiceBoardResult> compile(
        const SchematicDocument& schematic,
        const std::vector<ComponentSpiceSpec>& specs,
        const std::vector<PcbTrackSegment>& tracks = {},
        const std::vector<PcbVia>& vias = {},
        const BoardStackupManager* stackup = nullptr,
        bool include_trace_parasitics = true);
};

} // namespace shunya::pcb
