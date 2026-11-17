#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace shunya::perf {

enum class TracePhase : char {
    Complete = 'X',
    Begin = 'B',
    End = 'E',
    Instant = 'I',
    Counter = 'C'
};

struct TraceEvent {
    std::string name;
    std::string category{"default"};
    TracePhase phase{TracePhase::Complete};
    int64_t timestamp_us{0};
    int64_t duration_us{0}; // only for Complete ('X')
    uint32_t pid{1};
    uint32_t tid{1};
    std::map<std::string, std::string> string_args;
    std::map<std::string, double> numeric_args;
};

/**
 * @brief Thread-safe frame-by-frame timeline capture engine exportable to
 *        Chrome Tracing (chrome://tracing) and Perfetto (ui.perfetto.dev) JSON formats.
 * (Roadmap task PH4-02.04.004)
 */
class ChromeTracingExporter {
public:
    ChromeTracingExporter();
    ~ChromeTracingExporter() = default;

    void start_capture() noexcept;
    void stop_capture() noexcept;
    [[nodiscard]] bool is_capturing() const noexcept;

    void record_event(TraceEvent event);

    void record_complete_event(
        const std::string& name,
        const std::string& category,
        int64_t start_us,
        int64_t duration_us,
        uint32_t tid = 1,
        std::map<std::string, double> num_args = {});

    void record_instant(
        const std::string& name,
        const std::string& category,
        int64_t timestamp_us = -1,
        uint32_t tid = 1);

    void record_counter(
        const std::string& name,
        const std::string& category,
        const std::string& counter_key,
        double value,
        int64_t timestamp_us = -1);

    void clear() noexcept;

    [[nodiscard]] size_t event_count() const noexcept;
    [[nodiscard]] std::vector<TraceEvent> events() const;

    [[nodiscard]] std::string export_json() const;
    bool export_to_file(const std::string& file_path) const;

    [[nodiscard]] static int64_t current_time_us() noexcept;

private:
    bool capturing_{true};
    mutable std::mutex mutex_;
    std::vector<TraceEvent> events_;
    std::chrono::steady_clock::time_point origin_time_;
};

/**
 * @brief RAII scoped event recorder for Chrome Tracing timeline capture.
 */
class ScopedTraceEvent {
public:
    ScopedTraceEvent(
        ChromeTracingExporter& exporter,
        std::string name,
        std::string category,
        uint32_t tid = 1,
        std::map<std::string, double> num_args = {});

    ~ScopedTraceEvent();

    ScopedTraceEvent(const ScopedTraceEvent&) = delete;
    ScopedTraceEvent& operator=(const ScopedTraceEvent&) = delete;

private:
    ChromeTracingExporter& exporter_;
    std::string name_;
    std::string category_;
    uint32_t tid_;
    std::map<std::string, double> num_args_;
    int64_t start_us_;
};

} // namespace shunya::perf
