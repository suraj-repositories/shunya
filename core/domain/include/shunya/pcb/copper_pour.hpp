#pragma once

#include "shunya/pcb/board_outline.hpp"
#include "shunya/pcb/push_shove_engine.hpp"
#include "shunya/pcb/thermal_relief.hpp"
#include "shunya/pcb/via_manager.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

struct CopperPourZone {
    std::string net_name{"GND"};
    int layer_index{0};
    double clearance_mm{0.20};
    double spoke_width_mm{0.30};
    double min_island_area_mm2{0.50};
};

struct PourPadObstacle {
    std::string net_name;
    core::base::math::Vector2<double> center_mm;
    double radius_mm{0.80};
};

struct CopperPourResult {
    std::vector<core::base::math::Vector2<double>> outer_boundary_mm;
    std::vector<std::vector<core::base::math::Vector2<double>>> clearance_voids_mm;
    std::vector<ThermalReliefPad> thermal_relief_pads;
    double net_copper_area_mm2{0.0};
    bool has_acute_acid_traps{false};
};

/**
 * @brief Flood-fills copper pour zones with clearance cutouts around signal nets and 4-spoke thermal reliefs.
 */
class CopperPourEngine {
public:
    [[nodiscard]] static CopperPourResult generate_pour(
        const CopperPourZone& zone,
        const BoardOutline& board,
        const std::vector<PcbTrackSegment>& tracks,
        const std::vector<PourPadObstacle>& pads,
        const std::vector<PcbVia>& vias);

    /**
     * @brief Generates 4 orthogonal thermal relief spokes bridging a same-net pad across its antipad gap.
     */
    [[nodiscard]] static ThermalReliefPad build_thermal_relief(
        std::string net_name,
        const core::base::math::Vector2<double>& center_mm,
        double pad_radius_mm,
        double isolation_gap_mm,
        double spoke_width_mm);

    [[nodiscard]] static bool polygon_has_acute_acid_trap(
        const std::vector<core::base::math::Vector2<double>>& poly,
        double min_angle_deg = 60.0) noexcept;
};

} // namespace shunya::pcb
