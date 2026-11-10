#pragma once

#include "shunya/cad3d/rhi_renderer_engine.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace shunya::cad3d {

/**
 * @brief McGuire & Bavoil Weighted Blended Order-Independent Transparency (WBOIT) pass.
 */
class WboitPass {
public:
    void resize(uint32_t width, uint32_t height);
    void clear();

    void accumulate(
        const core::domain::ViewportCamera& camera,
        const std::vector<RhiDrawItem>& transparent_items,
        const std::vector<double>& opaque_depth
    );

    void composite(RhiRenderTarget& target_opaque_rt) const;

    [[nodiscard]] const std::vector<std::array<double, 4>>& accum_buffer() const noexcept { return accum_; }
    [[nodiscard]] const std::vector<double>& revealage_buffer() const noexcept { return revealage_; }

private:
    uint32_t width_{0};
    uint32_t height_{0};
    std::vector<std::array<double, 4>> accum_;
    std::vector<double> revealage_;
};

} // namespace shunya::cad3d
