#include "shunya/perf/chrome_tracing_exporter.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace shunya::perf {

ChromeTracingExporter::ChromeTracingExporter()
    : origin_time_(std::chrono::steady_clock::now()) {}

void ChromeTracingExporter::start_capture() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    capturing_ = true;
}

void ChromeTracingExporter::stop_capture() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    capturing_ = false;
}

bool ChromeTracingExporter::is_capturing() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return capturing_;
}

void ChromeTracingExporter::record_event(TraceEvent event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!capturing_) return;
    events_.push_back(std::move(event));
}

void ChromeTracingExporter::record_complete_event(
    const std::string& name,
    const std::string& category,
    int64_t start_us,
    int64_t duration_us,
    uint32_t tid,
    std::map<std::string, double> num_args) {
    TraceEvent ev;
    ev.name = name;
    ev.category = category;
    ev.phase = TracePhase::Complete;
    ev.timestamp_us = start_us;
    ev.duration_us = duration_us;
    ev.pid = 1;
    ev.tid = tid;
    ev.numeric_args = std::move(num_args);
    record_event(std::move(ev));
}

void ChromeTracingExporter::record_instant(
    const std::string& name,
    const std::string& category,
    int64_t timestamp_us,
    uint32_t tid) {
    TraceEvent ev;
    ev.name = name;
    ev.category = category;
    ev.phase = TracePhase::Instant;
    ev.timestamp_us = (timestamp_us >= 0) ? timestamp_us : current_time_us();
    ev.duration_us = 0;
    ev.pid = 1;
    ev.tid = tid;
    record_event(std::move(ev));
}

void ChromeTracingExporter::record_counter(
    const std::string& name,
    const std::string& category,
    const std::string& counter_key,
    double value,
    int64_t timestamp_us) {
    TraceEvent ev;
    ev.name = name;
    ev.category = category;
    ev.phase = TracePhase::Counter;
    ev.timestamp_us = (timestamp_us >= 0) ? timestamp_us : current_time_us();
    ev.duration_us = 0;
    ev.pid = 1;
    ev.tid = 1;
    ev.numeric_args[counter_key] = value;
    record_event(std::move(ev));
}

void ChromeTracingExporter::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
    origin_time_ = std::chrono::steady_clock::now();
}

size_t ChromeTracingExporter::event_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
}

std::vector<TraceEvent> ChromeTracingExporter::events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

int64_t ChromeTracingExporter::current_time_us() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

std::string ChromeTracingExporter::export_json() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream ss;
    ss << "{\n  \"traceEvents\": [\n";

    for (size_t i = 0; i < events_.size(); ++i) {
        const auto& ev = events_[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << ev.name << "\",\n";
        ss << "      \"cat\": \"" << ev.category << "\",\n";
        ss << "      \"ph\": \"" << static_cast<char>(ev.phase) << "\",\n";
        ss << "      \"ts\": " << ev.timestamp_us << ",\n";

        if (ev.phase == TracePhase::Complete) {
            ss << "      \"dur\": " << ev.duration_us << ",\n";
        }

        ss << "      \"pid\": " << ev.pid << ",\n";
        ss << "      \"tid\": " << ev.tid;

        if (!ev.string_args.empty() || !ev.numeric_args.empty()) {
            ss << ",\n      \"args\": {\n";
            bool first_arg = true;
            for (const auto& [k, v] : ev.string_args) {
                if (!first_arg) ss << ",\n";
                first_arg = false;
                ss << "        \"" << k << "\": \"" << v << "\"";
            }
            for (const auto& [k, v] : ev.numeric_args) {
                if (!first_arg) ss << ",\n";
                first_arg = false;
                ss << "        \"" << k << "\": " << v;
            }
            ss << "\n      }\n";
        } else {
            ss << "\n";
        }

        ss << "    }";
        if (i + 1 < events_.size()) {
            ss << ",";
        }
        ss << "\n";
    }

    ss << "  ],\n  \"displayTimeUnit\": \"ms\"\n}\n";
    return ss.str();
}

bool ChromeTracingExporter::export_to_file(const std::string& file_path) const {
    const std::string content = export_json();
    std::ofstream out(file_path);
    if (!out.is_open()) return false;
    out << content;
    return true;
}

ScopedTraceEvent::ScopedTraceEvent(
    ChromeTracingExporter& exporter,
    std::string name,
    std::string category,
    uint32_t tid,
    std::map<std::string, double> num_args)
    : exporter_(exporter),
      name_(std::move(name)),
      category_(std::move(category)),
      tid_(tid),
      num_args_(std::move(num_args)),
      start_us_(ChromeTracingExporter::current_time_us()) {}

ScopedTraceEvent::~ScopedTraceEvent() {
    const int64_t end_us = ChromeTracingExporter::current_time_us();
    const int64_t dur_us = (end_us >= start_us_) ? (end_us - start_us_) : 0;
    exporter_.record_complete_event(name_, category_, start_us_, dur_us, tid_, std::move(num_args_));
}

} // namespace shunya::perf
