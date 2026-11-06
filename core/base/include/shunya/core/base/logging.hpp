#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>

namespace shunya::core::base {

enum class LogLevel : uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

[[nodiscard]] constexpr std::string_view log_level_to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRIT";
    }
    return "UNKNOWN";
}

struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
    std::string run_id;
    std::string file;
    int line = 0;
    std::string function;

    [[nodiscard]] std::string format() const;
};

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};

class MemorySink : public ILogSink {
public:
    explicit MemorySink(std::size_t max_records = 1000)
        : max_records_(max_records) {}

    void write(const LogRecord& record) override;
    void flush() override {}

    [[nodiscard]] std::vector<LogRecord> get_records() const;
    [[nodiscard]] std::size_t count() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<LogRecord> records_;
    std::size_t max_records_;
};

class ConsoleSink : public ILogSink {
public:
    explicit ConsoleSink(bool use_stderr_for_errors = true)
        : use_stderr_for_errors_(use_stderr_for_errors) {}

    void write(const LogRecord& record) override;
    void flush() override;

private:
    std::mutex mutex_;
    bool use_stderr_for_errors_;
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel min_level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    void set_active_run_id(std::string run_id);
    [[nodiscard]] std::string active_run_id() const;

    void add_sink(std::shared_ptr<ILogSink> sink);
    void clear_sinks();

    void log(LogLevel level,
             std::string_view category,
             std::string_view message,
             std::string_view file = "",
             int line = 0,
             std::string_view function = "");

    [[nodiscard]] std::string export_diagnostic_bundle() const;

private:
    Logger() = default;
    mutable std::mutex mutex_;
    LogLevel min_level_{LogLevel::Info};
    std::string active_run_id_;
    std::vector<std::shared_ptr<ILogSink>> sinks_;
};

} // namespace shunya::core::base

#define SHUNYA_LOG_DEBUG(cat, msg) \
    ::shunya::core::base::Logger::instance().log(::shunya::core::base::LogLevel::Debug, cat, msg, __FILE__, __LINE__, __func__)
#define SHUNYA_LOG_INFO(cat, msg) \
    ::shunya::core::base::Logger::instance().log(::shunya::core::base::LogLevel::Info, cat, msg, __FILE__, __LINE__, __func__)
#define SHUNYA_LOG_WARN(cat, msg) \
    ::shunya::core::base::Logger::instance().log(::shunya::core::base::LogLevel::Warning, cat, msg, __FILE__, __LINE__, __func__)
#define SHUNYA_LOG_ERROR(cat, msg) \
    ::shunya::core::base::Logger::instance().log(::shunya::core::base::LogLevel::Error, cat, msg, __FILE__, __LINE__, __func__)
#define SHUNYA_LOG_CRIT(cat, msg) \
    ::shunya::core::base::Logger::instance().log(::shunya::core::base::LogLevel::Critical, cat, msg, __FILE__, __LINE__, __func__)
