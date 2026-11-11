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

} // namespace shunya::perf
