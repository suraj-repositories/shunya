#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace shunya::perf {

/**
 * @brief Structure-of-Arrays (SoA) layout for high-throughput SIMD bounding box queries.
 */
struct BBox3DSoA {
    std::span<const float> min_x;
    std::span<const float> min_y;
    std::span<const float> min_z;
    std::span<const float> max_x;
    std::span<const float> max_y;
    std::span<const float> max_z;

    [[nodiscard]] size_t size() const noexcept {
        return min_x.size();
    }
};

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

    // --- Vectorized Constitutive Material Loops (PH4-02.02.003) ---

    /**
     * @brief Vectorized Temperature Coefficient of Resistance (TCR):
     * rho(T) = rho0 * (1 + alpha * (T - T0))
     */
    static void constitutive_resistivity_tcr_f32(
        float rho0,
        float alpha,
        float t0,
        std::span<const float> temp,
        std::span<float> out_rho) noexcept;

    /**
     * @brief Vectorized Joule Heating volumetric power density: P = J^2 * rho
     */
    static void constitutive_joule_heating_f32(
        std::span<const float> current_density_sq,
        std::span<const float> resistivity,
        std::span<float> out_heat_power) noexcept;

    /**
     * @brief Vectorized linear thermal expansion: dL = L0 * alpha * dT
     */
    static void constitutive_thermal_expansion_f32(
        float linear_alpha,
        std::span<const float> delta_t,
        std::span<const float> orig_lengths,
        std::span<float> out_expansions) noexcept;

    /**
     * @brief Vectorized 1D Hookean elastic constitutive stress: sigma = E * epsilon
     */
    static void constitutive_elastic_stress_f32(
        float youngs_modulus,
        std::span<const float> strains,
        std::span<float> out_stresses) noexcept;

    // --- Vectorized Geometric Bounding Box Queries (PH4-02.02.003) ---

    /**
     * @brief SIMD 8-wide / 4-wide batch AABB-AABB spatial overlap query against SoA bounding boxes.
     * @return Number of matching box indices written to out_matching_indices.
     */
    static size_t batch_aabb_overlap_query(
        const BBox3DSoA& boxes,
        float qmin_x, float qmin_y, float qmin_z,
        float qmax_x, float qmax_y, float qmax_z,
        std::span<uint32_t> out_matching_indices) noexcept;

    /**
     * @brief SIMD 8-wide / 4-wide batch Ray-AABB slab intersection query against SoA bounding boxes.
     * @return Number of intersected box indices written to out_matching_indices.
     */
    static size_t batch_aabb_ray_intersect_f32(
        const BBox3DSoA& boxes,
        float ray_ox, float ray_oy, float ray_oz,
        float inv_dx, float inv_dy, float inv_dz,
        float max_dist,
        std::span<uint32_t> out_matching_indices) noexcept;
};

} // namespace shunya::perf
