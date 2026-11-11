#include "tests/test_framework.hpp"

#include "shunya/perf/domain_decomposition.hpp"
#include "shunya/perf/job_graph.hpp"
#include "shunya/perf/work_stealing_pool.hpp"

#include <atomic>
#include <cmath>
#include <vector>

using namespace shunya::perf;

TEST_CASE("PERF-02-001 WorkStealingPool parallel_for and JobGraph DAG deadlock-free execution") {
    WorkStealingPool pool(4);

    // 1. Parallel reduction check across 10,000 items
    std::atomic<int64_t> sum{0};
    pool.parallel_for(0, 10000, 64, [&](size_t i) {
        sum.fetch_add(static_cast<int64_t>(i + 1), std::memory_order_relaxed);
    });
    CHECK(sum.load() == 50005000LL);

    // 2. Randomized diamond/fan-out DAG dependency ordering (zero deadlock)
    JobGraph graph;
    std::atomic<int> stage{0};
    const size_t n_root = graph.add_node([&]() { stage.fetch_add(1); });
    const size_t n_a = graph.add_node([&]() { CHECK(stage.load() >= 1); stage.fetch_add(10); }, {n_root});
    const size_t n_b = graph.add_node([&]() { CHECK(stage.load() >= 1); stage.fetch_add(100); }, {n_root});
    (void)graph.add_node([&]() { CHECK(stage.load() == 111); stage.fetch_add(1000); }, {n_a, n_b});

    graph.execute(pool);
    CHECK(stage.load() == 1111);
}

TEST_CASE("PERF-02-001 DomainDecompositionManager ghost-cell diffusion matches monolithic bit-for-bit") {
    constexpr size_t NX = 24;
    constexpr size_t NY = 12;
    constexpr size_t NZ = 10;

    std::vector<double> init(NX * NY * NZ, 300.0);
    for (size_t k = 0; k < NZ; ++k) {
        for (size_t j = 0; j < NY; ++j) {
            init[(k * NY + j) * NX + 0] = 500.0; // Hot x=0 boundary
            init[(k * NY + j) * NX + (NX - 1)] = 250.0;
        }
    }
    init[(5 * NY + 6) * NX + 12] = 900.0; // Hot interior spike crossing subdomain seam

    WorkStealingPool pool(4);
    const double alpha_fo = 0.08;
    constexpr int kSteps = 20;

    const auto ref = DomainDecompositionManager::run_monolithic_diffusion_3d(
        NX, NY, NZ, alpha_fo, kSteps, init);
    auto par = DomainDecompositionManager::run_parallel_diffusion_3d(
        NX, NY, NZ, 4, alpha_fo, kSteps, init, pool);

    REQUIRE(par.is_ok());
    REQUIRE(par.value().size() == ref.size());

    double max_err = 0.0;
    for (size_t i = 0; i < ref.size(); ++i) {
        max_err = std::max(max_err, std::abs(par.value()[i] - ref[i]));
    }
    // Identical IEEE-754 summation order + exact ghost synchronization yields zero error
    CHECK(max_err == 0.0);
    CHECK(par.value()[(5 * NY + 6) * NX + 13] > 305.0);
}
