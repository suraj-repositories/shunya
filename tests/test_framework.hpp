#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <string_view>

namespace shunya::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

struct AutoRegister {
    AutoRegister(std::string name, std::function<void()> func) {
        get_registry().push_back({std::move(name), std::move(func)});
    }
};

inline int g_failed_checks = 0;
inline int g_total_checks = 0;

inline void check_impl(bool cond, std::string_view expr, std::string_view file, int line) {
    ++g_total_checks;
    if (!cond) {
        ++g_failed_checks;
        std::cerr << "[FAIL] Check failed: (" << expr << ") at " << file << ":" << line << "\n";
    }
}

#define SHUNYA_CONCAT_IMPL(a, b) a##b
#define SHUNYA_CONCAT(a, b) SHUNYA_CONCAT_IMPL(a, b)

#define TEST_CASE(name) \
    static void SHUNYA_CONCAT(test_func_, __LINE__)(); \
    static ::shunya::test::AutoRegister SHUNYA_CONCAT(reg_, __LINE__)(name, SHUNYA_CONCAT(test_func_, __LINE__)); \
    static void SHUNYA_CONCAT(test_func_, __LINE__)()


#define CHECK(expr) ::shunya::test::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define REQUIRE(expr) do { \
    bool req_cond = static_cast<bool>(expr); \
    ::shunya::test::check_impl(req_cond, #expr, __FILE__, __LINE__); \
    if (!req_cond) { \
        std::cerr << "[FATAL] Requirement (" #expr ") failed. Aborting test case.\n"; \
        return; \
    } \
} while(false)

inline int run_all_tests() {
    auto& tests = get_registry();
    std::cout << "========================================\n";
    std::cout << "Running " << tests.size() << " test cases...\n";
    std::cout << "========================================\n";

    int failed_cases = 0;
    for (const auto& t : tests) {
        int before_failures = g_failed_checks;
        std::cout << "[RUN ] " << t.name << "\n";
        try {
            t.func();
        } catch (const std::exception& ex) {
            ++g_failed_checks;
            std::cerr << "[EXCEPTION] Caught unhandled exception in test case: " << ex.what() << "\n";
        } catch (...) {
            ++g_failed_checks;
            std::cerr << "[EXCEPTION] Caught unknown unhandled exception in test case.\n";
        }

        if (g_failed_checks == before_failures) {
            std::cout << "[PASS] " << t.name << "\n";
        } else {
            ++failed_cases;
            std::cerr << "[FAIL] " << t.name << "\n";
        }
    }

    std::cout << "========================================\n";
    std::cout << "Test Summary: " << (tests.size() - failed_cases) << " passed, "
              << failed_cases << " failed of " << tests.size() << " cases. ("
              << (g_total_checks - g_failed_checks) << "/" << g_total_checks << " checks passed)\n";
    std::cout << "========================================\n";

    return failed_cases == 0 ? 0 : 1;
}

} // namespace shunya::test
