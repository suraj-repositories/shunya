#include "shunya/core/base/logging.hpp"
#include "shunya/core/base/version.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace shunya::core::base {

namespace {

std::string format_time_point(std::chrono::system_clock::time_point tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace

std::string LogRecord::format() const {
    std::ostringstream ss;
    ss << "[" << format_time_point(timestamp) << "] "
       << "[" << log_level_to_string(level) << "] "
       << "[" << category << "] ";

    if (!run_id.empty()) {
        ss << "[Run:" << run_id << "] ";
    }

    ss << message;

    if (!file.empty()) {
        ss << " (" << file << ":" << line << ")";
    }

    return ss.str();
}

void MemorySink::write(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (records_.size() >= max_records_) {
        records_.erase(records_.begin());
    }
    records_.push_back(record);
}

std::vector<LogRecord> MemorySink::get_records() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_;
}

std::size_t MemorySink::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.size();
}

void MemorySink::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.clear();
}

void ConsoleSink::write(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string formatted = record.format();
    if (use_stderr_for_errors_ && record.level >= LogLevel::Error) {
        std::cerr << formatted << "\n";
    } else {
        std::cout << formatted << "\n";
    }
}

void ConsoleSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout.flush();
    std::cerr.flush();
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel min_level) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = min_level;
}

LogLevel Logger::level() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return min_level_;
}

void Logger::set_active_run_id(std::string run_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_run_id_ = std::move(run_id);
}

std::string Logger::active_run_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_run_id_;
}

void Logger::add_sink(std::shared_ptr<ILogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level,
                 std::string_view category,
                 std::string_view message,
                 std::string_view file,
                 int line,
                 std::string_view function) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < min_level_) {
        return;
    }

    LogRecord record{
        .timestamp = std::chrono::system_clock::now(),
        .level = level,
        .category = std::string(category),
        .message = std::string(message),
        .run_id = active_run_id_,
        .file = std::string(file),
        .line = line,
        .function = std::string(function)
    };

    for (auto& sink : sinks_) {
        if (sink) {
            sink->write(record);
        }
    }
}

std::string Logger::export_diagnostic_bundle() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;
    ss << "=== SHUNYA DIAGNOSTIC BUNDLE ===\n";
    ss << "Platform Version: " << CURRENT_VERSION.as_string() << "\n";
    ss << "Active Run ID: " << (active_run_id_.empty() ? "<none>" : active_run_id_) << "\n";
    ss << "Log Sinks Active: " << sinks_.size() << "\n";
    ss << "Min Log Level: " << log_level_to_string(min_level_) << "\n";
    ss << "=================================\n";
    return ss.str();
}

} // namespace shunya::core::base
