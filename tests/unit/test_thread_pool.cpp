#include "tests/test_framework.hpp"
#include "shunya/core/base/thread_pool.hpp"

#include <chrono>
#include <atomic>
#include <numeric>

using namespace shunya::core::base;

TEST_CASE("FND-08: Basic Future Task Submission") {
    ThreadPool pool(2);
    CHECK(pool.thread_count() == 2);
    CHECK(!pool.is_shutdown());

    auto fut1 = pool.submit([]() { return 42; });
    auto fut2 = pool.submit([](int a, int b) { return a + b; }, 10, 25);

    CHECK(fut1.get() == 42);
    CHECK(fut2.get() == 35);
}

TEST_CASE("FND-08: Parallel Batch Execution and Wait Idle") {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    constexpr int total_jobs = 100;

    for (int i = 0; i < total_jobs; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.wait_idle();
    CHECK(counter.load() == total_jobs);
}

TEST_CASE("FND-08: Cancellation Token Propagation") {
    CancellationToken token;
    CHECK(!token.is_cancelled());

    token.cancel();
    CHECK(token.is_cancelled());

    token.reset();
    CHECK(!token.is_cancelled());
}

TEST_CASE("FND-08: Priority Task Ordering") {
    // 1-thread pool so execution order is strictly deterministic when queue is loaded
    ThreadPool pool(1);
    std::vector<int> executed_order;
    std::mutex order_mutex;

    // Block the single worker thread briefly so we can enqueue multiple items with different priorities
    std::promise<void> gate_promise;
    std::shared_future<void> gate_future = gate_promise.get_future().share();

    pool.submit([gate_future]() {
        gate_future.wait();
    });

    // Enqueue Low, Critical, Normal, High
    pool.submit(JobPriority::Low, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        executed_order.push_back(1); // Low
    });

    pool.submit(JobPriority::Critical, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        executed_order.push_back(4); // Critical
    });

    pool.submit(JobPriority::Normal, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        executed_order.push_back(2); // Normal
    });

    pool.submit(JobPriority::High, [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        executed_order.push_back(3); // High
    });

    // Release the worker
    gate_promise.set_value();
    pool.wait_idle();

    // Expect order: Critical(4), High(3), Normal(2), Low(1)
    std::lock_guard<std::mutex> lock(order_mutex);
    REQUIRE(executed_order.size() == 4);
    CHECK(executed_order[0] == 4);
    CHECK(executed_order[1] == 3);
    CHECK(executed_order[2] == 2);
    CHECK(executed_order[3] == 1);
}
