#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/pcb/impedance_calculator.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace shunya::pcb {

enum class StackupLayerKind : uint8_t {
    Signal,
    PowerPlane,
    GroundPlane,
    Mixed,
    DielectricPrepreg,
    DielectricCore,
    SolderMask
};

struct StackupLayer {
    std::string name;
    StackupLayerKind kind{StackupLayerKind::Signal};
    double thickness_mm{0.035};
    double copper_oz{1.0};
    double epsilon_r{4.4};
    double loss_tangent{0.02};
};

/**
 * @brief Multi-layer PCB substrate stackup manager (1 to 32 copper layers) with Z_0 analysis.
 */
class BoardStackupManager {
public:
    core::base::Result<void> add_layer(StackupLayer layer);
    core::base::Result<void> validate() const;

    [[nodiscard]] const std::vector<StackupLayer>& layers() const noexcept { return layers_; }
    [[nodiscard]] int copper_layer_count() const noexcept;
    [[nodiscard]] double total_thickness_mm() const noexcept;

    /**
     * @brief Computes surface microstrip Z_0 (Ohms) over the adjacent dielectric layer below Top copper.
     */
    [[nodiscard]] core::base::Result<double> compute_top_microstrip_z0(double trace_width_mm) const;

    [[nodiscard]] static BoardStackupManager standard_2layer_fr4();
    [[nodiscard]] static BoardStackupManager standard_4layer_fr4();

private:
    std::vector<StackupLayer> layers_;
};

} // namespace shunya::pcb
