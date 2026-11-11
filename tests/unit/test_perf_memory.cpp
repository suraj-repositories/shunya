#include "tests/test_framework.hpp"

#include "shunya/perf/fixed_size_pool.hpp"
#include "shunya/perf/linear_arena.hpp"
#include "shunya/perf/pmr_arena_adapter.hpp"

#include <cstdint>
#include <vector>

using namespace shunya::perf;

TEST_CASE("PERF-02-002 LinearArenaAllocator O(1) reset, alignment, and PMR container backing") {
    LinearArenaAllocator arena(1024 * 1024); // 1 MB arena
    PmrArenaResource pmr_res(arena);

    for (int step = 0; step < 50; ++step) {
        std::pmr::vector<double> scratch(&pmr_res);
        scratch.reserve(1024);
        for (int i = 0; i < 1024; ++i) {
            scratch.push_back(static_cast<double>(i) * 0.5);
        }
        CHECK(arena.bytes_used() >= 1024 * sizeof(double));
        void* aligned64 = arena.allocate(256, 64);
        REQUIRE(aligned64 != nullptr);
        CHECK((reinterpret_cast<uintptr_t>(aligned64) % 64) == 0);

        arena.reset();
        CHECK(arena.bytes_used() == 0);
    }

    // Peak stays bounded cleanly across 50 reset cycles (~8-12 KB) without memory growth
    CHECK(arena.peak_bytes_used() < 32 * 1024);
}

TEST_CASE("PERF-02-002 FixedSizePool 50,000 alloc/dealloc cycles with zero growth & 64-byte alignment") {
    struct ContactNode {
        double nx{0}, ny{1}, nz{0};
        double depth{0.001};
        uint64_t body_a{0}, body_b{0};
    };

    FixedSizePool<ContactNode, 64> pool(1024);
    CHECK(pool.chunk_count() == 1);
    CHECK(pool.total_capacity() == 1024);

    // Churn 50,000 allocations/deallocations in batches of 512 inside 1 initial chunk
    std::vector<ContactNode*> active;
    active.reserve(512);

    for (int batch = 0; batch < 100; ++batch) {
        for (int i = 0; i < 512; ++i) {
            ContactNode* c = pool.construct();
            REQUIRE(c != nullptr);
            CHECK((reinterpret_cast<uintptr_t>(c) % 64) == 0);
            c->body_a = static_cast<uint64_t>(batch * 512 + i);
            active.push_back(c);
        }
        CHECK(pool.active_count() == 512);
        for (ContactNode* c : active) {
            pool.destroy(c);
        }
        active.clear();
        CHECK(pool.active_count() == 0);
    }

    // Zero additional chunk allocations / zero memory fragmentation growth after 51,200 alloc/free ops
    CHECK(pool.chunk_count() == 1);
    CHECK(pool.total_capacity() == 1024);
}
