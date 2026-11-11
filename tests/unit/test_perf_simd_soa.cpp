#include "tests/test_framework.hpp"

#include "shunya/perf/simd_math.hpp"
#include "shunya/perf/soa_container.hpp"

#include <cmath>
#include <vector>

using namespace shunya::perf;

TEST_CASE("PERF-01-002 SimdMath fmadd, axpy, dot, distances, and 1D thermal stencil equivalence") {
    constexpr size_t N = 1029; // Non-multiple of 4 and 8 to exercise SIMD + scalar tails
    std::vector<float> a(N), b(N), c(N), out(N), y(N);
    for (size_t i = 0; i < N; ++i) {
        a[i] = std::sin(static_cast<float>(i) * 0.03f);
        b[i] = std::cos(static_cast<float>(i) * 0.05f);
        c[i] = static_cast<float>(i % 11) * 0.25f;
        y[i] = c[i];
    }

    SimdMath::fmadd_f32(a, b, c, out);
    for (size_t i = 0; i < N; ++i) {
        CHECK(std::abs(out[i] - (a[i] * b[i] + c[i])) < 1e-6f);
    }

    SimdMath::axpy_f32(2.5f, a, y);
    for (size_t i = 0; i < N; ++i) {
        CHECK(std::abs(y[i] - (c[i] + 2.5f * a[i])) < 1e-5f);
    }

    const float simd_dot = SimdMath::dot_f32(a, b);
    double ref_dot = 0.0;
    for (size_t i = 0; i < N; ++i) ref_dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
    CHECK(std::abs(simd_dot - static_cast<float>(ref_dot)) < 1e-3f);

    std::vector<float> t_in(N, 300.0f), t_out(N, 0.0f);
    t_in[0] = 400.0f;
    t_in[N - 1] = 350.0f;
    t_in[N / 2] = 500.0f;
    SimdMath::thermal_stencil_1d_f32(0.1f, t_in, t_out);
    for (size_t i = 1; i + 1 < N; ++i) {
        const float expected = t_in[i] + 0.1f * (t_in[i - 1] - 2.0f * t_in[i] + t_in[i + 1]);
        CHECK(std::abs(t_out[i] - expected) < 1e-5f);
    }
}

TEST_CASE("PERF-01-002 SoAParticleContainer 64-byte cache alignment and SIMD pairwise distances") {
    SoAParticleContainer soa;
    soa.reserve(100);
    for (int i = 0; i < 37; ++i) {
        soa.push_back(static_cast<float>(i) * 0.1f, 0.5f, -0.25f, 1.0f, 0.0f, 0.0f);
    }

    REQUIRE(soa.size() == 37);
    CHECK(soa.all_planes_64byte_aligned());

    AlignedBuffer<float, 64> r2(soa.size(), 0.0f);
    CHECK(r2.is_aligned());
    SimdMath::pairwise_squared_distances_f32(
        0.5f, 0.5f, -0.25f,
        soa.px.span(), soa.py.span(), soa.pz.span(), r2.span());

    for (size_t i = 0; i < soa.size(); ++i) {
        const float dx = soa.px[i] - 0.5f;
        CHECK(std::abs(r2[i] - (dx * dx)) < 1e-6f);
    }

    // Exercise SoA velocity update plane via SIMD axpy
    SimdMath::axpy_f32(0.01f, soa.vx.span(), soa.px.span());
    CHECK(std::abs(soa.px[0] - 0.01f) < 1e-6f);
}
