#include "shunya/perf/simd_math.hpp"

#include <algorithm>

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace shunya::perf {

const char* SimdMath::backend_name() noexcept {
#if defined(__AVX2__)
    return "AVX2";
#elif defined(__ARM_NEON)
    return "ARM_NEON";
#elif defined(__SSE2__)
    return "SSE2";
#else
    return "Scalar8Unrolled";
#endif
}

size_t SimdMath::vector_width_floats() noexcept {
#if defined(__AVX2__)
    return 8;
#elif defined(__ARM_NEON) || defined(__SSE2__)
    return 4;
#else
    return 1;
#endif
}

void SimdMath::fmadd_f32(
    std::span<const float> a,
    std::span<const float> b,
    std::span<const float> c,
    std::span<float> out) noexcept {
    const size_t n = std::min({a.size(), b.size(), c.size(), out.size()});
    size_t i = 0;

#if defined(__AVX2__) && defined(__FMA__)
    for (; i + 8 <= n; i += 8) {
        const __m256 va = _mm256_loadu_ps(a.data() + i);
        const __m256 vb = _mm256_loadu_ps(b.data() + i);
        const __m256 vc = _mm256_loadu_ps(c.data() + i);
        _mm256_storeu_ps(out.data() + i, _mm256_fmadd_ps(va, vb, vc));
    }
#elif defined(__AVX2__)
    for (; i + 8 <= n; i += 8) {
        const __m256 va = _mm256_loadu_ps(a.data() + i);
        const __m256 vb = _mm256_loadu_ps(b.data() + i);
        const __m256 vc = _mm256_loadu_ps(c.data() + i);
        _mm256_storeu_ps(out.data() + i, _mm256_add_ps(_mm256_mul_ps(va, vb), vc));
    }
#elif defined(__SSE2__)
    for (; i + 4 <= n; i += 4) {
        const __m128 va = _mm_loadu_ps(a.data() + i);
        const __m128 vb = _mm_loadu_ps(b.data() + i);
        const __m128 vc = _mm_loadu_ps(c.data() + i);
        _mm_storeu_ps(out.data() + i, _mm_add_ps(_mm_mul_ps(va, vb), vc));
    }
#elif defined(__ARM_NEON)
    for (; i + 4 <= n; i += 4) {
        const float32x4_t va = vld1q_f32(a.data() + i);
        const float32x4_t vb = vld1q_f32(b.data() + i);
        const float32x4_t vc = vld1q_f32(c.data() + i);
        vst1q_f32(out.data() + i, vmlaq_f32(vc, va, vb));
    }
#endif

    for (; i < n; ++i) {
        out[i] = a[i] * b[i] + c[i];
    }
}

void SimdMath::axpy_f32(
    float alpha,
    std::span<const float> x,
    std::span<float> y) noexcept {
    const size_t n = std::min(x.size(), y.size());
    size_t i = 0;

#if defined(__AVX2__)
    const __m256 va = _mm256_set1_ps(alpha);
    for (; i + 8 <= n; i += 8) {
        const __m256 vx = _mm256_loadu_ps(x.data() + i);
        const __m256 vy = _mm256_loadu_ps(y.data() + i);
        _mm256_storeu_ps(y.data() + i, _mm256_add_ps(_mm256_mul_ps(va, vx), vy));
    }
#elif defined(__SSE2__)
    const __m128 va = _mm_set1_ps(alpha);
    for (; i + 4 <= n; i += 4) {
        const __m128 vx = _mm_loadu_ps(x.data() + i);
        const __m128 vy = _mm_loadu_ps(y.data() + i);
        _mm_storeu_ps(y.data() + i, _mm_add_ps(_mm_mul_ps(va, vx), vy));
    }
#endif

    for (; i < n; ++i) {
        y[i] += alpha * x[i];
    }
}

float SimdMath::dot_f32(
    std::span<const float> a,
    std::span<const float> b) noexcept {
    const size_t n = std::min(a.size(), b.size());
    size_t i = 0;
    float sum = 0.0f;

#if defined(__AVX2__)
    __m256 acc = _mm256_setzero_ps();
    for (; i + 8 <= n; i += 8) {
        const __m256 va = _mm256_loadu_ps(a.data() + i);
        const __m256 vb = _mm256_loadu_ps(b.data() + i);
        acc = _mm256_add_ps(acc, _mm256_mul_ps(va, vb));
    }
    alignas(32) float lanes[8]{};
    _mm256_store_ps(lanes, acc);
    for (float l : lanes) sum += l;
#elif defined(__SSE2__)
    __m128 acc = _mm_setzero_ps();
    for (; i + 4 <= n; i += 4) {
        const __m128 va = _mm_loadu_ps(a.data() + i);
        const __m128 vb = _mm_loadu_ps(b.data() + i);
        acc = _mm_add_ps(acc, _mm_mul_ps(va, vb));
    }
    alignas(16) float lanes[4]{};
    _mm_store_ps(lanes, acc);
    for (float l : lanes) sum += l;
#endif

    for (; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

void SimdMath::pairwise_squared_distances_f32(
    float ref_x, float ref_y, float ref_z,
    std::span<const float> px,
    std::span<const float> py,
    std::span<const float> pz,
    std::span<float> out_r2) noexcept {
    const size_t n = std::min({px.size(), py.size(), pz.size(), out_r2.size()});
    size_t i = 0;

#if defined(__AVX2__)
    const __m256 rx = _mm256_set1_ps(ref_x);
    const __m256 ry = _mm256_set1_ps(ref_y);
    const __m256 rz = _mm256_set1_ps(ref_z);
    for (; i + 8 <= n; i += 8) {
        const __m256 dx = _mm256_sub_ps(_mm256_loadu_ps(px.data() + i), rx);
        const __m256 dy = _mm256_sub_ps(_mm256_loadu_ps(py.data() + i), ry);
        const __m256 dz = _mm256_sub_ps(_mm256_loadu_ps(pz.data() + i), rz);
        const __m256 r2 = _mm256_add_ps(_mm256_mul_ps(dx, dx),
                          _mm256_add_ps(_mm256_mul_ps(dy, dy), _mm256_mul_ps(dz, dz)));
        _mm256_storeu_ps(out_r2.data() + i, r2);
    }
#elif defined(__SSE2__)
    const __m128 rx = _mm_set1_ps(ref_x);
    const __m128 ry = _mm_set1_ps(ref_y);
    const __m128 rz = _mm_set1_ps(ref_z);
    for (; i + 4 <= n; i += 4) {
        const __m128 dx = _mm_sub_ps(_mm_loadu_ps(px.data() + i), rx);
        const __m128 dy = _mm_sub_ps(_mm_loadu_ps(py.data() + i), ry);
        const __m128 dz = _mm_sub_ps(_mm_loadu_ps(pz.data() + i), rz);
        const __m128 r2 = _mm_add_ps(_mm_mul_ps(dx, dx),
                          _mm_add_ps(_mm_mul_ps(dy, dy), _mm_mul_ps(dz, dz)));
        _mm_storeu_ps(out_r2.data() + i, r2);
    }
#endif

    for (; i < n; ++i) {
        const float dx = px[i] - ref_x;
        const float dy = py[i] - ref_y;
        const float dz = pz[i] - ref_z;
        out_r2[i] = dx * dx + dy * dy + dz * dz;
    }
}

void SimdMath::thermal_stencil_1d_f32(
    float coeff,
    std::span<const float> t_in,
    std::span<float> t_out) noexcept {
    const size_t n = std::min(t_in.size(), t_out.size());
    if (n < 3) return;
    t_out[0] = t_in[0];
    t_out[n - 1] = t_in[n - 1];

    size_t i = 1;
#if defined(__AVX2__)
    const __m256 vc = _mm256_set1_ps(coeff);
    const __m256 v2 = _mm256_set1_ps(2.0f);
    for (; i + 8 <= n - 1; i += 8) {
        const __m256 tl = _mm256_loadu_ps(t_in.data() + i - 1);
        const __m256 tc = _mm256_loadu_ps(t_in.data() + i);
        const __m256 tr = _mm256_loadu_ps(t_in.data() + i + 1);
        const __m256 lap = _mm256_sub_ps(_mm256_add_ps(tl, tr), _mm256_mul_ps(v2, tc));
        _mm256_storeu_ps(t_out.data() + i, _mm256_add_ps(tc, _mm256_mul_ps(vc, lap)));
    }
#elif defined(__SSE2__)
    const __m128 vc = _mm_set1_ps(coeff);
    const __m128 v2 = _mm_set1_ps(2.0f);
    for (; i + 4 <= n - 1; i += 4) {
        const __m128 tl = _mm_loadu_ps(t_in.data() + i - 1);
        const __m128 tc = _mm_loadu_ps(t_in.data() + i);
        const __m128 tr = _mm_loadu_ps(t_in.data() + i + 1);
        const __m128 lap = _mm_sub_ps(_mm_add_ps(tl, tr), _mm_mul_ps(v2, tc));
        _mm_storeu_ps(t_out.data() + i, _mm_add_ps(tc, _mm_mul_ps(vc, lap)));
    }
#endif

    for (; i + 1 < n; ++i) {
        t_out[i] = t_in[i] + coeff * (t_in[i - 1] - 2.0f * t_in[i] + t_in[i + 1]);
    }
}

void SimdMath::constitutive_resistivity_tcr_f32(
    float rho0,
    float alpha,
    float t0,
    std::span<const float> temp,
    std::span<float> out_rho) noexcept {
    const size_t n = std::min(temp.size(), out_rho.size());
    size_t i = 0;

#if defined(__AVX2__)
    const __m256 v_rho0 = _mm256_set1_ps(rho0);
    const __m256 v_alpha = _mm256_set1_ps(alpha);
    const __m256 v_t0 = _mm256_set1_ps(t0);
    const __m256 v_one = _mm256_set1_ps(1.0f);
    for (; i + 8 <= n; i += 8) {
        const __m256 vt = _mm256_loadu_ps(temp.data() + i);
        const __m256 dt = _mm256_sub_ps(vt, v_t0);
        const __m256 factor = _mm256_add_ps(v_one, _mm256_mul_ps(v_alpha, dt));
        _mm256_storeu_ps(out_rho.data() + i, _mm256_mul_ps(v_rho0, factor));
    }
#elif defined(__SSE2__)
    const __m128 v_rho0 = _mm_set1_ps(rho0);
    const __m128 v_alpha = _mm_set1_ps(alpha);
    const __m128 v_t0 = _mm_set1_ps(t0);
    const __m128 v_one = _mm_set1_ps(1.0f);
    for (; i + 4 <= n; i += 4) {
        const __m128 vt = _mm_loadu_ps(temp.data() + i);
        const __m128 dt = _mm_sub_ps(vt, v_t0);
        const __m128 factor = _mm_add_ps(v_one, _mm_mul_ps(v_alpha, dt));
        _mm_storeu_ps(out_rho.data() + i, _mm_mul_ps(v_rho0, factor));
    }
#elif defined(__ARM_NEON)
    const float32x4_t v_rho0 = vdupq_n_f32(rho0);
    const float32x4_t v_alpha = vdupq_n_f32(alpha);
    const float32x4_t v_t0 = vdupq_n_f32(t0);
    const float32x4_t v_one = vdupq_n_f32(1.0f);
    for (; i + 4 <= n; i += 4) {
        const float32x4_t vt = vld1q_f32(temp.data() + i);
        const float32x4_t dt = vsubq_f32(vt, v_t0);
        const float32x4_t factor = vaddq_f32(v_one, vmulq_f32(v_alpha, dt));
        vst1q_f32(out_rho.data() + i, vmulq_f32(v_rho0, factor));
    }
#endif

    for (; i < n; ++i) {
        out_rho[i] = rho0 * (1.0f + alpha * (temp[i] - t0));
    }
}

void SimdMath::constitutive_joule_heating_f32(
    std::span<const float> current_density_sq,
    std::span<const float> resistivity,
    std::span<float> out_heat_power) noexcept {
    const size_t n = std::min({current_density_sq.size(), resistivity.size(), out_heat_power.size()});
    size_t i = 0;

#if defined(__AVX2__)
    for (; i + 8 <= n; i += 8) {
        const __m256 vj2 = _mm256_loadu_ps(current_density_sq.data() + i);
        const __m256 vrho = _mm256_loadu_ps(resistivity.data() + i);
        _mm256_storeu_ps(out_heat_power.data() + i, _mm256_mul_ps(vj2, vrho));
    }
#elif defined(__SSE2__)
    for (; i + 4 <= n; i += 4) {
        const __m128 vj2 = _mm_loadu_ps(current_density_sq.data() + i);
        const __m128 vrho = _mm_loadu_ps(resistivity.data() + i);
        _mm_storeu_ps(out_heat_power.data() + i, _mm_mul_ps(vj2, vrho));
    }
#elif defined(__ARM_NEON)
    for (; i + 4 <= n; i += 4) {
        const float32x4_t vj2 = vld1q_f32(current_density_sq.data() + i);
        const float32x4_t vrho = vld1q_f32(resistivity.data() + i);
        vst1q_f32(out_heat_power.data() + i, vmulq_f32(vj2, vrho));
    }
#endif

    for (; i < n; ++i) {
        out_heat_power[i] = current_density_sq[i] * resistivity[i];
    }
}

void SimdMath::constitutive_thermal_expansion_f32(
    float linear_alpha,
    std::span<const float> delta_t,
    std::span<const float> orig_lengths,
    std::span<float> out_expansions) noexcept {
    const size_t n = std::min({delta_t.size(), orig_lengths.size(), out_expansions.size()});
    size_t i = 0;

#if defined(__AVX2__)
    const __m256 valpha = _mm256_set1_ps(linear_alpha);
    for (; i + 8 <= n; i += 8) {
        const __m256 vdt = _mm256_loadu_ps(delta_t.data() + i);
        const __m256 vl0 = _mm256_loadu_ps(orig_lengths.data() + i);
        _mm256_storeu_ps(out_expansions.data() + i, _mm256_mul_ps(_mm256_mul_ps(vl0, valpha), vdt));
    }
#elif defined(__SSE2__)
    const __m128 valpha = _mm_set1_ps(linear_alpha);
    for (; i + 4 <= n; i += 4) {
        const __m128 vdt = _mm_loadu_ps(delta_t.data() + i);
        const __m128 vl0 = _mm_loadu_ps(orig_lengths.data() + i);
        _mm_storeu_ps(out_expansions.data() + i, _mm_mul_ps(_mm_mul_ps(vl0, valpha), vdt));
    }
#elif defined(__ARM_NEON)
    const float32x4_t valpha = vdupq_n_f32(linear_alpha);
    for (; i + 4 <= n; i += 4) {
        const float32x4_t vdt = vld1q_f32(delta_t.data() + i);
        const float32x4_t vl0 = vld1q_f32(orig_lengths.data() + i);
        vst1q_f32(out_expansions.data() + i, vmulq_f32(vmulq_f32(vl0, valpha), vdt));
    }
#endif

    for (; i < n; ++i) {
        out_expansions[i] = orig_lengths[i] * linear_alpha * delta_t[i];
    }
}

void SimdMath::constitutive_elastic_stress_f32(
    float youngs_modulus,
    std::span<const float> strains,
    std::span<float> out_stresses) noexcept {
    const size_t n = std::min(strains.size(), out_stresses.size());
    size_t i = 0;

#if defined(__AVX2__)
    const __m256 ve = _mm256_set1_ps(youngs_modulus);
    for (; i + 8 <= n; i += 8) {
        const __m256 veps = _mm256_loadu_ps(strains.data() + i);
        _mm256_storeu_ps(out_stresses.data() + i, _mm256_mul_ps(ve, veps));
    }
#elif defined(__SSE2__)
    const __m128 ve = _mm_set1_ps(youngs_modulus);
    for (; i + 4 <= n; i += 4) {
        const __m128 veps = _mm_loadu_ps(strains.data() + i);
        _mm_storeu_ps(out_stresses.data() + i, _mm_mul_ps(ve, veps));
    }
#elif defined(__ARM_NEON)
    const float32x4_t ve = vdupq_n_f32(youngs_modulus);
    for (; i + 4 <= n; i += 4) {
        const float32x4_t veps = vld1q_f32(strains.data() + i);
        vst1q_f32(out_stresses.data() + i, vmulq_f32(ve, veps));
    }
#endif

    for (; i < n; ++i) {
        out_stresses[i] = youngs_modulus * strains[i];
    }
}

size_t SimdMath::batch_aabb_overlap_query(
    const BBox3DSoA& boxes,
    float qmin_x, float qmin_y, float qmin_z,
    float qmax_x, float qmax_y, float qmax_z,
    std::span<uint32_t> out_matching_indices) noexcept {
    const size_t n = std::min({boxes.min_x.size(), boxes.min_y.size(), boxes.min_z.size(),
                               boxes.max_x.size(), boxes.max_y.size(), boxes.max_z.size()});
    const size_t max_out = out_matching_indices.size();
    size_t match_count = 0;
    size_t i = 0;

#if defined(__AVX2__)
    const __m256 v_qmin_x = _mm256_set1_ps(qmin_x);
    const __m256 v_qmin_y = _mm256_set1_ps(qmin_y);
    const __m256 v_qmin_z = _mm256_set1_ps(qmin_z);
    const __m256 v_qmax_x = _mm256_set1_ps(qmax_x);
    const __m256 v_qmax_y = _mm256_set1_ps(qmax_y);
    const __m256 v_qmax_z = _mm256_set1_ps(qmax_z);

    for (; i + 8 <= n && match_count < max_out; i += 8) {
        const __m256 b_min_x = _mm256_loadu_ps(boxes.min_x.data() + i);
        const __m256 b_min_y = _mm256_loadu_ps(boxes.min_y.data() + i);
        const __m256 b_min_z = _mm256_loadu_ps(boxes.min_z.data() + i);
        const __m256 b_max_x = _mm256_loadu_ps(boxes.max_x.data() + i);
        const __m256 b_max_y = _mm256_loadu_ps(boxes.max_y.data() + i);
        const __m256 b_max_z = _mm256_loadu_ps(boxes.max_z.data() + i);

        const __m256 c0 = _mm256_cmp_ps(b_max_x, v_qmin_x, _CMP_GE_OQ);
        const __m256 c1 = _mm256_cmp_ps(b_min_x, v_qmax_x, _CMP_LE_OQ);
        const __m256 c2 = _mm256_cmp_ps(b_max_y, v_qmin_y, _CMP_GE_OQ);
        const __m256 c3 = _mm256_cmp_ps(b_min_y, v_qmax_y, _CMP_LE_OQ);
        const __m256 c4 = _mm256_cmp_ps(b_max_z, v_qmin_z, _CMP_GE_OQ);
        const __m256 c5 = _mm256_cmp_ps(b_min_z, v_qmax_z, _CMP_LE_OQ);

        const __m256 m_xy = _mm256_and_ps(_mm256_and_ps(c0, c1), _mm256_and_ps(c2, c3));
        const __m256 m_all = _mm256_and_ps(m_xy, _mm256_and_ps(c4, c5));

        int mask = _mm256_movemask_ps(m_all);
        while (mask != 0 && match_count < max_out) {
            int bit = __builtin_ctz(mask);
            out_matching_indices[match_count++] = static_cast<uint32_t>(i + bit);
            mask &= mask - 1;
        }
    }
#elif defined(__SSE2__)
    const __m128 v_qmin_x = _mm_set1_ps(qmin_x);
    const __m128 v_qmin_y = _mm_set1_ps(qmin_y);
    const __m128 v_qmin_z = _mm_set1_ps(qmin_z);
    const __m128 v_qmax_x = _mm_set1_ps(qmax_x);
    const __m128 v_qmax_y = _mm_set1_ps(qmax_y);
    const __m128 v_qmax_z = _mm_set1_ps(qmax_z);

    for (; i + 4 <= n && match_count < max_out; i += 4) {
        const __m128 b_min_x = _mm_loadu_ps(boxes.min_x.data() + i);
        const __m128 b_min_y = _mm_loadu_ps(boxes.min_y.data() + i);
        const __m128 b_min_z = _mm_loadu_ps(boxes.min_z.data() + i);
        const __m128 b_max_x = _mm_loadu_ps(boxes.max_x.data() + i);
        const __m128 b_max_y = _mm_loadu_ps(boxes.max_y.data() + i);
        const __m128 b_max_z = _mm_loadu_ps(boxes.max_z.data() + i);

        const __m128 c0 = _mm_cmpge_ps(b_max_x, v_qmin_x);
        const __m128 c1 = _mm_cmple_ps(b_min_x, v_qmax_x);
        const __m128 c2 = _mm_cmpge_ps(b_max_y, v_qmin_y);
        const __m128 c3 = _mm_cmple_ps(b_min_y, v_qmax_y);
        const __m128 c4 = _mm_cmpge_ps(b_max_z, v_qmin_z);
        const __m128 c5 = _mm_cmple_ps(b_min_z, v_qmax_z);

        const __m128 m_all = _mm_and_ps(_mm_and_ps(_mm_and_ps(c0, c1), _mm_and_ps(c2, c3)), _mm_and_ps(c4, c5));
        int mask = _mm_movemask_ps(m_all);
        while (mask != 0 && match_count < max_out) {
            int bit = __builtin_ctz(mask);
            out_matching_indices[match_count++] = static_cast<uint32_t>(i + bit);
            mask &= mask - 1;
        }
    }
#endif

    for (; i < n && match_count < max_out; ++i) {
        if (boxes.max_x[i] >= qmin_x && boxes.min_x[i] <= qmax_x &&
            boxes.max_y[i] >= qmin_y && boxes.min_y[i] <= qmax_y &&
            boxes.max_z[i] >= qmin_z && boxes.min_z[i] <= qmax_z) {
            out_matching_indices[match_count++] = static_cast<uint32_t>(i);
        }
    }
    return match_count;
}

size_t SimdMath::batch_aabb_ray_intersect_f32(
    const BBox3DSoA& boxes,
    float ray_ox, float ray_oy, float ray_oz,
    float inv_dx, float inv_dy, float inv_dz,
    float max_dist,
    std::span<uint32_t> out_matching_indices) noexcept {
    const size_t n = std::min({boxes.min_x.size(), boxes.min_y.size(), boxes.min_z.size(),
                               boxes.max_x.size(), boxes.max_y.size(), boxes.max_z.size()});
    const size_t max_out = out_matching_indices.size();
    size_t match_count = 0;
    size_t i = 0;

#if defined(__AVX2__)
    const __m256 v_ox = _mm256_set1_ps(ray_ox);
    const __m256 v_oy = _mm256_set1_ps(ray_oy);
    const __m256 v_oz = _mm256_set1_ps(ray_oz);
    const __m256 v_idx = _mm256_set1_ps(inv_dx);
    const __m256 v_idy = _mm256_set1_ps(inv_dy);
    const __m256 v_idz = _mm256_set1_ps(inv_dz);
    const __m256 v_zero = _mm256_setzero_ps();
    const __m256 v_maxd = _mm256_set1_ps(max_dist);

    for (; i + 8 <= n && match_count < max_out; i += 8) {
        const __m256 b_min_x = _mm256_loadu_ps(boxes.min_x.data() + i);
        const __m256 b_min_y = _mm256_loadu_ps(boxes.min_y.data() + i);
        const __m256 b_min_z = _mm256_loadu_ps(boxes.min_z.data() + i);
        const __m256 b_max_x = _mm256_loadu_ps(boxes.max_x.data() + i);
        const __m256 b_max_y = _mm256_loadu_ps(boxes.max_y.data() + i);
        const __m256 b_max_z = _mm256_loadu_ps(boxes.max_z.data() + i);

        const __m256 tx1 = _mm256_mul_ps(_mm256_sub_ps(b_min_x, v_ox), v_idx);
        const __m256 tx2 = _mm256_mul_ps(_mm256_sub_ps(b_max_x, v_ox), v_idx);
        const __m256 ty1 = _mm256_mul_ps(_mm256_sub_ps(b_min_y, v_oy), v_idy);
        const __m256 ty2 = _mm256_mul_ps(_mm256_sub_ps(b_max_y, v_oy), v_idy);
        const __m256 tz1 = _mm256_mul_ps(_mm256_sub_ps(b_min_z, v_oz), v_idz);
        const __m256 tz2 = _mm256_mul_ps(_mm256_sub_ps(b_max_z, v_oz), v_idz);

        const __m256 tmin_x = _mm256_min_ps(tx1, tx2);
        const __m256 tmax_x = _mm256_max_ps(tx1, tx2);
        const __m256 tmin_y = _mm256_min_ps(ty1, ty2);
        const __m256 tmax_y = _mm256_max_ps(ty1, ty2);
        const __m256 tmin_z = _mm256_min_ps(tz1, tz2);
        const __m256 tmax_z = _mm256_max_ps(tz1, tz2);

        const __m256 t_near = _mm256_max_ps(_mm256_max_ps(tmin_x, tmin_y), tmin_z);
        const __m256 t_far = _mm256_min_ps(_mm256_min_ps(tmax_x, tmax_y), tmax_z);

        const __m256 valid_near = _mm256_max_ps(v_zero, t_near);
        const __m256 hit_mask = _mm256_and_ps(
            _mm256_cmp_ps(t_far, valid_near, _CMP_GE_OQ),
            _mm256_cmp_ps(t_near, v_maxd, _CMP_LE_OQ)
        );

        int mask = _mm256_movemask_ps(hit_mask);
        while (mask != 0 && match_count < max_out) {
            int bit = __builtin_ctz(mask);
            out_matching_indices[match_count++] = static_cast<uint32_t>(i + bit);
            mask &= mask - 1;
        }
    }
#elif defined(__SSE2__)
    const __m128 v_ox = _mm_set1_ps(ray_ox);
    const __m128 v_oy = _mm_set1_ps(ray_oy);
    const __m128 v_oz = _mm_set1_ps(ray_oz);
    const __m128 v_idx = _mm_set1_ps(inv_dx);
    const __m128 v_idy = _mm_set1_ps(inv_dy);
    const __m128 v_idz = _mm_set1_ps(inv_dz);
    const __m128 v_zero = _mm_setzero_ps();
    const __m128 v_maxd = _mm_set1_ps(max_dist);

    for (; i + 4 <= n && match_count < max_out; i += 4) {
        const __m128 b_min_x = _mm_loadu_ps(boxes.min_x.data() + i);
        const __m128 b_min_y = _mm_loadu_ps(boxes.min_y.data() + i);
        const __m128 b_min_z = _mm_loadu_ps(boxes.min_z.data() + i);
        const __m128 b_max_x = _mm_loadu_ps(boxes.max_x.data() + i);
        const __m128 b_max_y = _mm_loadu_ps(boxes.max_y.data() + i);
        const __m128 b_max_z = _mm_loadu_ps(boxes.max_z.data() + i);

        const __m128 tx1 = _mm_mul_ps(_mm_sub_ps(b_min_x, v_ox), v_idx);
        const __m128 tx2 = _mm_mul_ps(_mm_sub_ps(b_max_x, v_ox), v_idx);
        const __m128 ty1 = _mm_mul_ps(_mm_sub_ps(b_min_y, v_oy), v_idy);
        const __m128 ty2 = _mm_mul_ps(_mm_sub_ps(b_max_y, v_oy), v_idy);
        const __m128 tz1 = _mm_mul_ps(_mm_sub_ps(b_min_z, v_oz), v_idz);
        const __m128 tz2 = _mm_mul_ps(_mm_sub_ps(b_max_z, v_oz), v_idz);

        const __m128 tmin_x = _mm_min_ps(tx1, tx2);
        const __m128 tmax_x = _mm_max_ps(tx1, tx2);
        const __m128 tmin_y = _mm_min_ps(ty1, ty2);
        const __m128 tmax_y = _mm_max_ps(ty1, ty2);
        const __m128 tmin_z = _mm_min_ps(tz1, tz2);
        const __m128 tmax_z = _mm_max_ps(tz1, tz2);

        const __m128 t_near = _mm_max_ps(_mm_max_ps(tmin_x, tmin_y), tmin_z);
        const __m128 t_far = _mm_min_ps(_mm_min_ps(tmax_x, tmax_y), tmax_z);

        const __m128 valid_near = _mm_max_ps(v_zero, t_near);
        const __m128 hit_mask = _mm_and_ps(
            _mm_cmpge_ps(t_far, valid_near),
            _mm_cmple_ps(t_near, v_maxd)
        );

        int mask = _mm_movemask_ps(hit_mask);
        while (mask != 0 && match_count < max_out) {
            int bit = __builtin_ctz(mask);
            out_matching_indices[match_count++] = static_cast<uint32_t>(i + bit);
            mask &= mask - 1;
        }
    }
#endif

    for (; i < n && match_count < max_out; ++i) {
        float tx1 = (boxes.min_x[i] - ray_ox) * inv_dx;
        float tx2 = (boxes.max_x[i] - ray_ox) * inv_dx;
        float ty1 = (boxes.min_y[i] - ray_oy) * inv_dy;
        float ty2 = (boxes.max_y[i] - ray_oy) * inv_dy;
        float tz1 = (boxes.min_z[i] - ray_oz) * inv_dz;
        float tz2 = (boxes.max_z[i] - ray_oz) * inv_dz;

        float tmin_x = std::min(tx1, tx2);
        float tmax_x = std::max(tx1, tx2);
        float tmin_y = std::min(ty1, ty2);
        float tmax_y = std::max(ty1, ty2);
        float tmin_z = std::min(tz1, tz2);
        float tmax_z = std::max(tz1, tz2);

        float t_near = std::max({tmin_x, tmin_y, tmin_z});
        float t_far = std::min({tmax_x, tmax_y, tmax_z});

        if (t_far >= std::max(0.0f, t_near) && t_near <= max_dist) {
            out_matching_indices[match_count++] = static_cast<uint32_t>(i);
        }
    }
    return match_count;
}

} // namespace shunya::perf
