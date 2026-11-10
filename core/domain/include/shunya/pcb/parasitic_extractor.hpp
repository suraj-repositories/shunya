#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/pcb/board_stackup.hpp"
#include "shunya/pcb/push_shove_engine.hpp"
#include "shunya/pcb/via_manager.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::pcb {

struct TraceParasitics {
    std::string net_name;
    double length_mm{0.0};
    double width_mm{0.25};
    double thickness_mm{0.035};
    double resistance_ohm{0.0};
    double inductance_nh{0.0};
    double capacitance_pf{0.0};
    double impedance_ohm{50.0};
};

struct ViaParasitics {
    std::string net_name;
    double drill_diameter_mm{0.3};
    double board_thickness_mm{1.6};
    double resistance_ohm{0.0};
    double inductance_nh{0.0};
    double capacitance_pf{0.0};
};

struct NetParasiticSummary {
    std::string net_name;
    double total_length_mm{0.0};
    double total_resistance_ohm{0.0};
    double total_inductance_nh{0.0};
    double total_capacitance_pf{0.0};
    int segment_count{0};
    int via_count{0};
};

/**
 * @brief Extracts distributed R, L, C parasitics from PCB copper traces and vias using geometric & stackup parameters.
 */
class ParasiticExtractor {
public:
    static constexpr double kCopperRho20C = 1.68e-8;     // Ohm * m at 20 C
    static constexpr double kCopperTempCoeff = 0.00393;  // 1 / C
    static constexpr double kSpeedOfLightMmS = 2.99792458e11;

    [[nodiscard]] static double copper_thickness_mm_from_oz(double oz) noexcept;

    [[nodiscard]] static double copper_resistivity_ohm_m(double temp_c = 20.0) noexcept;

    /**
     * @brief Extracts lumped RLC parameters for a single microstrip copper trace segment.
     */
    [[nodiscard]] static core::base::Result<TraceParasitics> extract_track(
        const PcbTrackSegment& segment,
        double copper_thickness_mm = 0.035,
        double dielectric_height_mm = 0.20,
        double epsilon_r = 4.4,
        double temp_c = 20.0);

    /**
     * @brief Extracts copper barrel resistance, self-inductance, and pad capacitance for a via.
     */
    [[nodiscard]] static core::base::Result<ViaParasitics> extract_via(
        const PcbVia& via,
        double board_thickness_mm = 1.6,
        double plating_thickness_mm = 0.025,
        double epsilon_r = 4.4,
        double temp_c = 20.0);

    /**
     * @brief Aggregates R, L, C parasitics per net across all routed track segments and vias using board stackup.
     */
    [[nodiscard]] static std::unordered_map<std::string, NetParasiticSummary> extract_board_nets(
        const std::vector<PcbTrackSegment>& tracks,
        const std::vector<PcbVia>& vias,
        const BoardStackupManager& stackup,
        double temp_c = 20.0);
};

} // namespace shunya::pcb
