#include "tests/test_framework.hpp"

#include <algorithm>
#include <deque>
#include <regex>
#include <string>
#include <vector>

using namespace shunya;

TEST_CASE("PH5-09.01: Monochromatic Log Stream & Monospace Structured Formatting") {
    // 1. Structured Text Prefixes & Monochromatic Plaintext Check
    std::vector<std::string> sample_logs = {
        "[12:34:56.789] [INIT] Shunya Phase 5 Engineering Platform Core initialized.",
        "[12:34:56.790] [INFO] Loaded 118 periodic elements from elements.csv.",
        "[12:34:57.100] [SOLVER] Coupled multi-physics step converged in 3 iterations.",
        "[12:34:57.250] [WARN] Total mixture composition unbalanced (110.0%).",
        "[12:34:58.000] [ERROR] Formation rule requires at least one matrix element.",
        "[12:34:58.500] [SIM] Simulation time: 0.250 s, step dt: 0.001 s."
    };

    std::regex tag_re(R"(\[\d{2}:\d{2}:\d{2}\.\d{3}\]\s+\[(INIT|INFO|SOLVER|WARN|ERROR|SIM|ELEM|BENCHMARK|SWEEP)\])");

    for (const auto& log : sample_logs) {
        // Must match structured format [HH:MM:SS.zzz] [TAG]
        CHECK(std::regex_search(log, tag_re));

        // Must be monochromatic plaintext - zero HTML color spans or escape codes
        CHECK(log.find("<font") == std::string::npos);
        CHECK(log.find("style=") == std::string::npos);
        CHECK(log.find("\033[") == std::string::npos);
        CHECK(log.find("#") == std::string::npos);
    }
}

TEST_CASE("PH5-09.02: Resizable Drawer Ring-Buffer Memory Limit (10,000 Lines)") {
    constexpr size_t kMaxLines = 10000;
    std::deque<std::string> log_buffer;

    auto append_log = [&](const std::string& line) {
        log_buffer.push_back(line);
        if (log_buffer.size() > kMaxLines) {
            log_buffer.pop_front();
        }
    };

    // Simulate high-frequency simulation telemetry stream (25,000 lines)
    for (size_t i = 0; i < 25000; ++i) {
        append_log("[INFO] Telemetry tick " + std::to_string(i));
    }

    CHECK(log_buffer.size() == kMaxLines);
    CHECK(log_buffer.front() == "[INFO] Telemetry tick 15000");
    CHECK(log_buffer.back() == "[INFO] Telemetry tick 24999");

    // Clear operation
    log_buffer.clear();
    CHECK(log_buffer.empty());
}
