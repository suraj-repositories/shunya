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


TEST_CASE("PH4-01.02.003: Cooperative Cancellation with CancellationToken") {
    ThreadPool pool(2);

    CancellationToken cancelled_token;
    cancelled_token.cancel();

    CancellationToken valid_token;

    // Test non-void task with cancelled token
    auto fut_skipped = pool.submit_cancellable(JobPriority::Normal, cancelled_token, []() {
        return 999;
    });

    // Test non-void task with valid token
    auto fut_executed = pool.submit_cancellable(JobPriority::Normal, valid_token, []() {
        return 1234;
    });

    // Test void task with cancelled token
    std::atomic<bool> executed_void{false};
    auto fut_void_skipped = pool.submit_cancellable(cancelled_token, [&]() {
        executed_void.store(true);
    });

    // Test void task with valid token
    std::atomic<bool> executed_void_valid{false};
    auto fut_void_executed = pool.submit_cancellable(valid_token, [&]() {
        executed_void_valid.store(true);
    });

    pool.wait_idle();

    auto res_skipped = fut_skipped.get();
    CHECK_FALSE(res_skipped.has_value());

    auto res_executed = fut_executed.get();
    REQUIRE(res_executed.has_value());
    CHECK(res_executed.value() == 1234);

    CHECK_FALSE(executed_void.load());
    CHECK_FALSE(fut_void_skipped.get());

    CHECK(executed_void_valid.load());
    CHECK(fut_void_executed.get());
}

TEST_CASE("PH4-01.02.003: Work Stealing Across Worker Queues") {
    ThreadPool pool(2);
    REQUIRE(pool.thread_count() == 2);

    std::promise<void> worker0_hold_promise;
    std::shared_future<void> worker0_hold_future = worker0_hold_promise.get_future().share();

    // 1. Submit a blocking job targeted directly to Worker 0
    pool.submit_to_worker(0, JobPriority::Critical, [worker0_hold_future]() {
        worker0_hold_future.wait();
    });

    // Give worker 0 a moment to pick up the blocking task
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 2. Submit 8 tasks specifically to worker 0's local queue
    std::atomic<int> completed_stolen_jobs{0};
    for (int i = 0; i < 8; ++i) {
        pool.submit_to_worker(0, JobPriority::Normal, [&completed_stolen_jobs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            completed_stolen_jobs.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // Worker 1 should steal tasks from worker 0 while worker 0 is blocked
    // Wait until worker 1 steals and executes tasks
    for (int retry = 0; retry < 50; ++retry) {
        if (completed_stolen_jobs.load() > 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Release worker 0
    worker0_hold_promise.set_value();

    pool.wait_idle();

    CHECK(completed_stolen_jobs.load() == 8);
    CHECK(pool.stolen_tasks_count() > 0);
}
