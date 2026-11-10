#pragma once

#include "shunya/pcb/board_outline.hpp"
#include "shunya/pcb/via_manager.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

/**
 * @brief Exports Excellon NC drill files (METRIC TZ format) for vias and mounting holes.
 */
class ExcellonExporter {
public:
    [[nodiscard]] static std::string export_drill_file(
        const std::vector<PcbVia>& vias,
        const std::vector<MountingHole>& mounting_holes = {});
};

} // namespace shunya::pcb
