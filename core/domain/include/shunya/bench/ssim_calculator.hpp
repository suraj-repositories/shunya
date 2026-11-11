#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace shunya::bench {

struct ImageBufferGray {
    size_t width{0};
    size_t height{0};
    std::vector<uint8_t> pixels;
};

/**
 * @brief Sliding-window Wang et al. (2004) Structural Similarity Index (SSIM) image comparator.
 */
class SsimCalculator {
public:
    [[nodiscard]] static double compute_ssim(
        const ImageBufferGray& ref_img,
        const ImageBufferGray& test_img,
        size_t window_size = 8) noexcept {
        if (ref_img.width == 0 || ref_img.height == 0 ||
            ref_img.width != test_img.width || ref_img.height != test_img.height ||
            ref_img.pixels.size() != ref_img.width * ref_img.height ||
            test_img.pixels.size() != ref_img.pixels.size()) {
            return 0.0;
        }

        constexpr double kC1 = (0.01 * 255.0) * (0.01 * 255.0);
        constexpr double kC2 = (0.03 * 255.0) * (0.03 * 255.0);

        const size_t win = std::min({window_size, ref_img.width, ref_img.height});
        double ssim_sum = 0.0;
        size_t win_count = 0;

        for (size_t y = 0; y + win <= ref_img.height; y += std::max<size_t>(1, win / 2)) {
            for (size_t x = 0; x + win <= ref_img.width; x += std::max<size_t>(1, win / 2)) {
                double mu_x = 0.0, mu_y = 0.0;
                const double n_pix = static_cast<double>(win * win);
                for (size_t dy = 0; dy < win; ++dy) {
                    for (size_t dx = 0; dx < win; ++dx) {
                        const size_t idx = (y + dy) * ref_img.width + (x + dx);
                        mu_x += static_cast<double>(ref_img.pixels[idx]);
                        mu_y += static_cast<double>(test_img.pixels[idx]);
                    }
                }
                mu_x /= n_pix;
                mu_y /= n_pix;

                double var_x = 0.0, var_y = 0.0, cov = 0.0;
                for (size_t dy = 0; dy < win; ++dy) {
                    for (size_t dx = 0; dx < win; ++dx) {
                        const size_t idx = (y + dy) * ref_img.width + (x + dx);
                        const double dxv = static_cast<double>(ref_img.pixels[idx]) - mu_x;
                        const double dyv = static_cast<double>(test_img.pixels[idx]) - mu_y;
                        var_x += dxv * dxv;
                        var_y += dyv * dyv;
                        cov += dxv * dyv;
                    }
                }
                var_x /= n_pix;
                var_y /= n_pix;
                cov /= n_pix;

                const double num = (2.0 * mu_x * mu_y + kC1) * (2.0 * cov + kC2);
                const double den = (mu_x * mu_x + mu_y * mu_y + kC1) * (var_x + var_y + kC2);
                ssim_sum += num / std::max(1e-12, den);
                ++win_count;
            }
        }
        return (win_count > 0) ? (ssim_sum / static_cast<double>(win_count)) : 0.0;
    }
};

} // namespace shunya::bench
