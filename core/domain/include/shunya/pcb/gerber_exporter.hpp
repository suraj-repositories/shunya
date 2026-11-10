#pragma once

#include "shunya/pcb/board_outline.hpp"
#include "shunya/pcb/drc_engine.hpp"
#include "shunya/pcb/push_shove_engine.hpp"
#include "shunya/pcb/via_manager.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

/**
 * @brief Exports Ucamco Gerber RS-274X rev 2023.08 manufacturing files for copper and outline layers.
 */
class GerberExporter {
public:
    [[nodiscard]] static std::string export_copper_layer(
        std::string_view layer_name,
        int layer_index,
        const std::vector<PcbTrackSegment>& tracks,
        const std::vector<DrcPad>& pads,
        const std::vector<PcbVia>& vias);

    [[nodiscard]] static std::string export_board_outline(const BoardOutline& outline);
};

} // namespace shunya::pcb
