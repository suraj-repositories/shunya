#pragma once

#include <cstddef>
#include <span>

namespace shunya::perf {

/**
 * @brief Cross-platform SIMD-accelerated float32 math & physics kernel helpers (AVX2 / NEON / 8-wide unrolled scalar fallback).
 */
class SimdMath {
public:
    [[nodiscard]] static const char* backend_name() noexcept;
    [[nodiscard]] static size_t vector_width_floats() noexcept;

    static void fmadd_f32(
        std::span<const float> a,
        std::span<const float> b,
        std::span<const float> c,
        std::span<float> out) noexcept;

    static void axpy_f32(
        float alpha,
        std::span<const float> x,
        std::span<float> y) noexcept;

    [[nodiscard]] static float dot_f32(
        std::span<const float> a,
        std::span<const float> b) noexcept;

    static void pairwise_squared_distances_f32(
        float ref_x, float ref_y, float ref_z,
        std::span<const float> px,
        std::span<const float> py,
        std::span<const float> pz,
        std::span<float> out_r2) noexcept;

    static void thermal_stencil_1d_f32(
        float coeff,
        std::span<const float> t_in,
        std::span<float> t_out) noexcept;
};

} // namespace shunya::perf
