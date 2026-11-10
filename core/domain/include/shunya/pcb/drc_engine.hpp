#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/pcb/board_outline.hpp"
#include "shunya/pcb/push_shove_engine.hpp"
#include "shunya/pcb/via_manager.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace shunya::pcb {

enum class DrcViolationKind : uint8_t {
    ClearanceViolation,
    TrackWidthViolation,
    ViaDrillViolation,
    AnnularRingViolation,
    UnconnectedNet
};

struct DrcRules {
    double min_clearance_mm{0.15};
    double min_track_width_mm{0.15};
    double min_via_drill_mm{0.20};
    double min_annular_ring_mm{0.125};
};

struct DrcPad {
    std::string ref_des;
    std::string pin;
    std::string net_name;
    int layer_index{0};
    core::base::math::Vector2<double> center_mm;
    double width_mm{0.60};
    double height_mm{1.55};
};

struct DrcViolation {
    DrcViolationKind kind{DrcViolationKind::ClearanceViolation};
    std::string net_or_item;
    core::base::math::Vector2<double> location_mm;
    double measured_mm{0.0};
    double required_mm{0.15};
    std::string message;
};

struct DrcReport {
    std::vector<DrcViolation> violations;
    [[nodiscard]] bool passed() const noexcept { return violations.empty(); }
};

/**
 * @brief Geometric Design Rule Checker validating clearances, trace widths, drills, annular rings, and connectivity.
 */
class DrcEngine {
public:
    [[nodiscard]] static DrcReport check_board(
        const DrcRules& rules,
        const std::vector<PcbTrackSegment>& tracks,
        const std::vector<DrcPad>& pads,
        const ViaManager& via_mgr,
        const std::vector<std::string>& required_nets = {});
};

} // namespace shunya::pcb
