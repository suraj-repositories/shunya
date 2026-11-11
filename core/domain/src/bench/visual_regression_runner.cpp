#include "shunya/bench/visual_regression_runner.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::bench {

ImageBufferGray VisualRegressionRunner::render_pbr_sphere_reference(
    size_t width,
    size_t height,
    double light_az) {
    ImageBufferGray img{width, height, std::vector<uint8_t>(width * height, 18)};
    const double cx = 0.5 * static_cast<double>(width);
    const double cy = 0.5 * static_cast<double>(height);
    const double rad = 0.36 * static_cast<double>(std::min(width, height));

    const double lx = std::cos(light_az);
    const double ly = -0.5;
    const double lz = std::sin(light_az);
    const double l_len = std::sqrt(lx * lx + ly * ly + lz * lz);
    const double nx_l = lx / l_len, ny_l = ly / l_len, nz_l = lz / l_len;

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const double dx = (static_cast<double>(x) + 0.5 - cx) / rad;
            const double dy = (static_cast<double>(y) + 0.5 - cy) / rad;
            const double r2 = dx * dx + dy * dy;
            if (r2 <= 1.0) {
                const double dz = std::sqrt(1.0 - r2);
                const double ndotl = std::max(0.0, dx * nx_l + dy * ny_l + dz * nz_l);
                // Simplified PBR GGX/Lambert highlight
                const double hx = nx_l, hy = ny_l, hz = nz_l + 1.0;
                const double h_len = std::sqrt(hx * hx + hy * hy + hz * hz);
                const double ndoth = std::max(0.0, (dx * hx + dy * hy + dz * hz) / h_len);
                const double spec = std::pow(ndoth, 32.0);
                const double lum = std::clamp(0.08 + 0.72 * ndotl + 0.20 * spec, 0.0, 1.0);
                img.pixels[y * width + x] = static_cast<uint8_t>(lum * 255.0);
            }
        }
    }
    return img;
}

ImageBufferGray VisualRegressionRunner::render_2d_schematic_reference(size_t width, size_t height) {
    ImageBufferGray img{width, height, std::vector<uint8_t>(width * height, 20)};
    // Horizontal schematic bus & resistor symbol outline
    const size_t mid_y = height / 2;
    for (size_t x = 4; x + 4 < width; ++x) {
        if (x < width / 3 || x > (2 * width) / 3) {
            img.pixels[mid_y * width + x] = 220;
        }
    }
    const size_t x0 = width / 3, x1 = (2 * width) / 3;
    const size_t y0 = mid_y - height / 8, y1 = mid_y + height / 8;
    for (size_t x = x0; x <= x1; ++x) {
        img.pixels[y0 * width + x] = 240;
        img.pixels[y1 * width + x] = 240;
    }
    for (size_t y = y0; y <= y1; ++y) {
        img.pixels[y * width + x0] = 240;
        img.pixels[y * width + x1] = 240;
    }
    return img;
}

VisualRegressionVerdict VisualRegressionRunner::evaluate_frame(
    std::string scene_name,
    const ImageBufferGray& golden,
    const ImageBufferGray& candidate,
    double threshold) {
    const double ssim = SsimCalculator::compute_ssim(golden, candidate, 8);
    return VisualRegressionVerdict{
        std::move(scene_name),
        ssim,
        threshold,
        ssim >= threshold
    };
}

} // namespace shunya::bench
