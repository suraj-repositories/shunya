#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/pcb/push_shove_engine.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::pcb {

enum class ViaType : uint8_t {
    ThroughHole,
    Blind,
    Buried
};

struct PcbVia {
    std::string net_name;
    core::base::math::Vector2<double> center_mm;
    double drill_mm{0.30};
    double pad_diameter_mm{0.60};
    int start_layer{0};
    int end_layer{1};
    ViaType type{ViaType::ThroughHole};

    [[nodiscard]] double annular_ring_mm() const noexcept {
        return 0.5 * (pad_diameter_mm - drill_mm);
    }

    [[nodiscard]] bool spans_layer(int layer) const noexcept {
        return layer >= start_layer && layer <= end_layer;
    }
};

/**
 * @brief Manages through-hole, blind, and buried vias plus multi-layer netlist continuity verification.
 */
class ViaManager {
public:
    explicit ViaManager(int total_copper_layers = 4) : total_layers_(total_copper_layers) {}

    core::base::Result<void> add_via(PcbVia via, double min_annular_ring_mm = 0.125);
    [[nodiscard]] const std::vector<PcbVia>& vias() const noexcept { return vias_; }

    /**
     * @brief Verifies that all segments on `net_name` form a single connected graph across layers via vias.
     */
    [[nodiscard]] bool verify_net_continuity(std::string_view net_name,
                                             const std::vector<PcbTrackSegment>& tracks) const;

private:
    int total_layers_{4};
    std::vector<PcbVia> vias_;
};

} // namespace shunya::pcb
