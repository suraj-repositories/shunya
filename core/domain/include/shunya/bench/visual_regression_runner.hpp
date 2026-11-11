#pragma once

#include "shunya/bench/ssim_calculator.hpp"

#include <string>

namespace shunya::bench {

struct VisualRegressionVerdict {
    std::string scene_name;
    double ssim_score{0.0};
    double required_threshold{0.995};
    bool passed{false};
};

/**
 * @brief Offscreen reference PBR & 2D schematic framebuffer comparator enforcing SSIM >= 0.995.
 */
class VisualRegressionRunner {
public:
    [[nodiscard]] static ImageBufferGray render_pbr_sphere_reference(size_t width = 64, size_t height = 64, double light_az = 0.6);
    [[nodiscard]] static ImageBufferGray render_2d_schematic_reference(size_t width = 64, size_t height = 64);

    [[nodiscard]] static VisualRegressionVerdict evaluate_frame(
        std::string scene_name,
        const ImageBufferGray& golden,
        const ImageBufferGray& candidate,
        double threshold = 0.995);
};

} // namespace shunya::bench
