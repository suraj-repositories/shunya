#include "tests/test_framework.hpp"
#include "shunya/core/base/logging.hpp"
#include "shunya/core/base/uuid.hpp"

using namespace shunya::core::base;

TEST_CASE("FND-05: Logging Levels and Minimum Level Filtering") {
    auto& logger = Logger::instance();
    logger.clear_sinks();

    auto mem_sink = std::make_shared<MemorySink>(50);
    logger.add_sink(mem_sink);
    logger.set_level(LogLevel::Warning);

    SHUNYA_LOG_DEBUG("Core", "This debug message should be dropped");
    SHUNYA_LOG_INFO("Core", "This info message should be dropped");
    CHECK(mem_sink->count() == 0);

    SHUNYA_LOG_WARN("Simulation", "A potential timestep instability detected");
    SHUNYA_LOG_ERROR("Solver", "Matrix singular error encountered");
    CHECK(mem_sink->count() == 2);

    auto records = mem_sink->get_records();
    REQUIRE(records.size() == 2);
    CHECK(records[0].level == LogLevel::Warning);
    CHECK(records[0].category == "Simulation");
    CHECK(records[1].level == LogLevel::Error);
    CHECK(records[1].category == "Solver");
}

TEST_CASE("FND-05: Run ID Tracking and Formatting") {
    auto& logger = Logger::instance();
    logger.clear_sinks();

    auto mem_sink = std::make_shared<MemorySink>(50);
    logger.add_sink(mem_sink);
    logger.set_level(LogLevel::Debug);

    Uuid run_id = Uuid::generate_random();
    logger.set_active_run_id(run_id.to_string());
    CHECK(logger.active_run_id() == run_id.to_string());

    SHUNYA_LOG_INFO("RunEngine", "Simulation step started");
    auto records = mem_sink->get_records();
    REQUIRE(!records.empty());
    CHECK(records.back().run_id == run_id.to_string());

    std::string formatted = records.back().format();
    CHECK(formatted.find(run_id.to_string()) != std::string::npos);
    CHECK(formatted.find("[INFO]") != std::string::npos);
    CHECK(formatted.find("[RunEngine]") != std::string::npos);
}

TEST_CASE("FND-05: Memory Sink Capacity Limit") {
    auto mem_sink = std::make_shared<MemorySink>(5);
    for (int i = 0; i < 10; ++i) {
        LogRecord rec{};
        rec.timestamp = std::chrono::system_clock::now();
        rec.level = LogLevel::Info;
        rec.category = "Test";
        rec.message = "Message " + std::to_string(i);
        mem_sink->write(rec);
    }

    CHECK(mem_sink->count() == 5);
    auto records = mem_sink->get_records();
    CHECK(records.front().message == "Message 5");
    CHECK(records.back().message == "Message 9");

    mem_sink->clear();
    CHECK(mem_sink->count() == 0);
}

TEST_CASE("FND-05: Diagnostic Bundle Generation") {
    auto& logger = Logger::instance();
    std::string bundle = logger.export_diagnostic_bundle();
    CHECK(bundle.find("SHUNYA DIAGNOSTIC BUNDLE") != std::string::npos);
    CHECK(bundle.find("Platform Version") != std::string::npos);
}
