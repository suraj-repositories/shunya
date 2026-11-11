#include "tests/test_framework.hpp"

#include "shunya/perf/gpu_particle_engine.hpp"
#include "shunya/perf/gpu_spmv.hpp"
#include "shunya/perf/gpu_transfer_ring.hpp"

#include <cmath>
#include <vector>

using namespace shunya::perf;
using shunya::core::domain::SparseMatrixCSR;
using shunya::core::domain::Triplet;

TEST_CASE("PERF-01-001 GpuParticleEngine spatial-hash SPH parity vs brute-force O(N^2)") {
    GpuSphParams params;
    params.smoothing_h_m = 0.08;
    params.rest_density_kg_m3 = 1000.0;
    params.workgroup_size = 256;
    params.hash_table_size = 1024;

    // 4x4x4 = 64 tightly clustered particles around spacing 0.03 m
    std::vector<GpuParticleState> particles;
    for (int iz = 0; iz < 4; ++iz) {
        for (int iy = 0; iy < 4; ++iy) {
            for (int ix = 0; ix < 4; ++ix) {
                GpuParticleState p;
                p.px = ix * 0.03;
                p.py = iy * 0.03;
                p.pz = iz * 0.03;
                p.vx = ix * 0.1;
                p.mass = 0.027;
                particles.push_back(p);
            }
        }
    }

    GpuParticleEngine gpu_engine(params);
    gpu_engine.set_particles(particles);
    auto st = gpu_engine.dispatch_density_and_forces_only();
    REQUIRE(st.is_ok());
    CHECK(st.value().workgroup_count_x == 1); // ceil(64 / 256) == 1

    // Exact O(N^2) poly6 density reference for particle 21 (interior particle)
    const size_t probe_idx = 21;
    const double h = params.smoothing_h_m;
    const double h2 = h * h;
    constexpr double kPi = 3.14159265358979323846;
    const double poly6 = 315.0 / (64.0 * kPi * std::pow(h, 9));
    double ref_rho = 0.0;
    for (const auto& pj : particles) {
        const double dx = particles[probe_idx].px - pj.px;
        const double dy = particles[probe_idx].py - pj.py;
        const double dz = particles[probe_idx].pz - pj.pz;
        const double r2 = dx * dx + dy * dy + dz * dz;
        if (r2 < h2) {
            const double diff = h2 - r2;
            ref_rho += pj.mass * poly6 * diff * diff * diff;
        }
    }

    CHECK(std::abs(gpu_engine.particles()[probe_idx].density - ref_rho) < 1e-10);
    CHECK(GpuParticleEngine::density_shader_glsl().find("local_size_x = 256") != std::string_view::npos);
    CHECK(GpuParticleEngine::forces_shader_glsl().find("local_size_x = 256") != std::string_view::npos);

    auto step_res = gpu_engine.dispatch_step(0.001);
    REQUIRE(step_res.is_ok());
}

TEST_CASE("PERF-01-001 GpuSpMvKernel CSR SpMV and Jacobi Conjugate Gradient vs CPU reference") {
    constexpr size_t N = 200;
    std::vector<Triplet> triplets;
    triplets.reserve(3 * N);
    std::vector<double> rhs(N, 1.0);
    std::vector<double> x_vec(N, 0.0);

    for (size_t i = 0; i < N; ++i) {
        triplets.push_back({i, i, 4.0});
        if (i > 0) triplets.push_back({i, i - 1, -1.0});
        if (i + 1 < N) triplets.push_back({i, i + 1, -1.0});
        x_vec[i] = std::sin(static_cast<double>(i) * 0.1);
    }

    const SparseMatrixCSR A = SparseMatrixCSR::from_triplets(N, N, triplets);
    const std::vector<double> y_cpu = A.multiply(x_vec);
    std::vector<double> y_gpu(N, 0.0);
    REQUIRE(GpuSpMvKernel::multiply(A, x_vec, y_gpu).is_ok());

    for (size_t i = 0; i < N; ++i) {
        CHECK(std::abs(y_cpu[i] - y_gpu[i]) < 1e-12);
    }

    std::vector<double> sol(N, 0.0);
    auto cg = GpuSpMvKernel::solve_jacobi_cg(A, rhs, sol, 1e-10, 1000);
    REQUIRE(cg.is_ok());
    CHECK(cg.value().converged);
    CHECK(cg.value().residual_norm_inf <= 1e-10);

    std::vector<double> check_y(N, 0.0);
    REQUIRE(GpuSpMvKernel::multiply(A, sol, check_y).is_ok());
    for (size_t i = 0; i < N; ++i) {
        CHECK(std::abs(check_y[i] - rhs[i]) < 1e-8);
    }
    CHECK(GpuSpMvKernel::spmv_shader_glsl().find("local_size_x = 256") != std::string_view::npos);
}

TEST_CASE("PERF-01-001 GpuTransferRing triple-buffered timeline semaphore zero-stall DMA pipeline") {
    GpuTransferRing ring(3, 64);
    for (uint64_t frame = 0; frame < 10; ++frame) {
        std::vector<uint8_t> payload(64, static_cast<uint8_t>(frame + 1));
        REQUIRE(ring.begin_async_upload(frame, payload).is_ok());
        // Simulate asynchronous DMA engine retiring in pipelined order
        ring.complete_dma_transfer(frame);
        CHECK(ring.is_frame_device_ready(frame));
        CHECK(ring.device_slot_view(frame)[0] == static_cast<uint8_t>(frame + 1));
    }

    const auto stats = ring.stats();
    CHECK(stats.total_uploads == 10);
    CHECK(stats.cpu_stall_events == 0);
}
