#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/base/ring_buffer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// OBS-01: Measurement Probes
// =============================================================================

enum class MeasurementType {
    Scalar,
    Vector,
    Tensor,
    SpatialField,
    DiscreteEvent
};

[[nodiscard]] constexpr std::string_view measurement_type_to_string(MeasurementType type) noexcept {
    switch (type) {
        case MeasurementType::Scalar: return "Scalar";
        case MeasurementType::Vector: return "Vector";
        case MeasurementType::Tensor: return "Tensor";
        case MeasurementType::SpatialField: return "SpatialField";
        case MeasurementType::DiscreteEvent: return "DiscreteEvent";
    }
    return "Scalar";
}

[[nodiscard]] std::optional<MeasurementType> measurement_type_from_string(std::string_view str) noexcept;

struct ProbeMeasurement {
    double timestamp{0.0};
    double scalar_value{0.0};
    std::vector<double> vector_value;
    std::string event_name;
};

struct ProbeDefinition {
    base::Uuid id{base::Uuid::generate_random()};
    std::string name;
    base::Uuid target_entity_id{};
    std::string target_variable_or_port;
    MeasurementType type{MeasurementType::Scalar};
    std::string unit_symbol{"1"};
    double sampling_rate_hz{100.0};

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<ProbeDefinition> from_json(std::string_view json_str);
};

// =============================================================================
// OBS-02: Live Dashboards & Widgets
// =============================================================================

enum class WidgetType {
    TimeSeriesChart,
    Gauge,
    Table,
    StatusIndicator,
    Histogram
};

[[nodiscard]] constexpr std::string_view widget_type_to_string(WidgetType type) noexcept {
    switch (type) {
        case WidgetType::TimeSeriesChart: return "TimeSeriesChart";
        case WidgetType::Gauge: return "Gauge";
        case WidgetType::Table: return "Table";
        case WidgetType::StatusIndicator: return "StatusIndicator";
        case WidgetType::Histogram: return "Histogram";
    }
    return "TimeSeriesChart";
}

struct DashboardWidget {
    base::Uuid widget_id{base::Uuid::generate_random()};
    std::string title;
    WidgetType type{WidgetType::TimeSeriesChart};
    std::vector<base::Uuid> connected_probe_ids;
    double min_display_range{0.0};
    double max_display_range{100.0};
};

class Dashboard {
public:
    Dashboard() = default;
    explicit Dashboard(std::string name);

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    void add_widget(DashboardWidget widget);
    bool remove_widget(const base::Uuid& widget_id);
    [[nodiscard]] const std::vector<DashboardWidget>& widgets() const noexcept { return widgets_; }
    [[nodiscard]] size_t widget_count() const noexcept { return widgets_.size(); }

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<Dashboard> from_json(std::string_view json_str);

private:
    std::string name_{"DefaultDashboard"};
    std::vector<DashboardWidget> widgets_;
};

// =============================================================================
// OBS-03: Time-Series Storage with Decimation & Lock-Free Ring Buffer
// =============================================================================

enum class DecimationPolicy {
    None,
    MinMax,
    Average
};

struct TimeSeriesSample {
    double t{0.0};
    double v{0.0};
};

class TimeSeriesStore {
public:
    explicit TimeSeriesStore(size_t max_capacity = 100000, size_t ring_buffer_size = 4096);

    // Ingestion via lock-free ring buffer
    bool push_live(double t, double v) noexcept;
    size_t flush_live_buffer() noexcept;

    // Direct insertion
    void append(double t, double v);

    [[nodiscard]] size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] bool empty() const noexcept { return samples_.empty(); }
    [[nodiscard]] const std::vector<TimeSeriesSample>& raw_samples() const noexcept { return samples_; }

    [[nodiscard]] std::vector<TimeSeriesSample> query_range(
        double t_start, double t_end, size_t max_points = 1000, DecimationPolicy policy = DecimationPolicy::MinMax
    ) const;

    [[nodiscard]] double min_value() const noexcept;
    [[nodiscard]] double max_value() const noexcept;
    [[nodiscard]] double mean_value() const noexcept;

    void clear() noexcept;

private:
    size_t max_capacity_;
    base::RingBuffer<TimeSeriesSample> live_ring_buffer_;
    std::vector<TimeSeriesSample> samples_;
};

// =============================================================================
// OBS-04: Spatial Field Visualization
// =============================================================================

struct SpatialPoint3D {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct SpatialFieldData {
    std::string field_name;
    double timestamp{0.0};
    std::vector<SpatialPoint3D> points;
    std::vector<double> scalar_values;
    std::vector<SpatialPoint3D> vector_values;

    [[nodiscard]] double min_scalar() const noexcept;
    [[nodiscard]] double max_scalar() const noexcept;

    // Extracts 2D slice on plane z = z_slice
    [[nodiscard]] SpatialFieldData slice_z(double z_slice, double tolerance = 1e-4) const;
};

// =============================================================================
// OBS-05: Instrument Models
// =============================================================================

class IVirtualInstrument {
public:
    virtual ~IVirtualInstrument() = default;
    [[nodiscard]] virtual std::string instrument_name() const = 0;
    virtual void reset() = 0;
};

class VirtualOscilloscope : public IVirtualInstrument {
public:
    enum class TriggerMode { Auto, Normal, Single };

    [[nodiscard]] std::string instrument_name() const override { return "Oscilloscope"; }
    void reset() override;

    void set_time_base(double sec_per_div) noexcept { time_base_ = sec_per_div; }
    [[nodiscard]] double time_base() const noexcept { return time_base_; }

    void set_volts_per_div(double v_per_div) noexcept { volts_per_div_ = v_per_div; }
    [[nodiscard]] double volts_per_div() const noexcept { return volts_per_div_; }

    void set_trigger(TriggerMode mode, double level) noexcept { trigger_mode_ = mode; trigger_level_ = level; }
    void feed_sample(double t, double v);

    [[nodiscard]] bool is_triggered() const noexcept { return triggered_; }
    [[nodiscard]] const std::vector<TimeSeriesSample>& trace_buffer() const noexcept { return trace_buffer_; }

private:
    double time_base_{0.001}; // 1 ms/div
    double volts_per_div_{1.0}; // 1 V/div
    TriggerMode trigger_mode_{TriggerMode::Auto};
    double trigger_level_{0.0};
    bool triggered_{false};
    double last_v_{0.0};
    std::vector<TimeSeriesSample> trace_buffer_;
};

class VirtualMultimeter : public IVirtualInstrument {
public:
    enum class Mode { VoltageDC, VoltageAC, CurrentDC, Resistance };

    [[nodiscard]] std::string instrument_name() const override { return "Multimeter"; }
    void reset() override { readings_.clear(); }

    void set_mode(Mode m) noexcept { mode_ = m; }
    [[nodiscard]] Mode mode() const noexcept { return mode_; }

    void sample(double val) noexcept;
    [[nodiscard]] double current_reading() const noexcept;

private:
    Mode mode_{Mode::VoltageDC};
    std::vector<double> readings_;
};

class VirtualThermometer : public IVirtualInstrument {
public:
    enum class Scale { Kelvin, Celsius, Fahrenheit };

    [[nodiscard]] std::string instrument_name() const override { return "Thermometer"; }
    void reset() override { current_temp_kelvin_ = 273.15; }

    void set_temperature_kelvin(double t_k) noexcept { current_temp_kelvin_ = t_k; }
    [[nodiscard]] double read(Scale scale = Scale::Celsius) const noexcept;

private:
    double current_temp_kelvin_{293.15}; // 20 C default
};

// =============================================================================
// OBS-06: Units, Significant Digits & Axis Formatting
// =============================================================================

struct AxisFormatting {
    std::string unit_symbol{"1"};
    int significant_digits{3};
    bool use_si_prefixes{true};

    [[nodiscard]] std::string format_value(double val) const;
};

// =============================================================================
// OBS-07: Annotations, Bookmarks and Notes
// =============================================================================

struct ResultAnnotation {
    base::Uuid id{base::Uuid::generate_random()};
    double timestamp{0.0};
    std::string text;
    std::string author;
    std::string color_hex{"#FFFF00"};
};

struct ExperimentNote {
    base::Uuid id{base::Uuid::generate_random()};
    std::string title;
    std::string content_markdown;
    std::string author;
    std::string created_at_iso;
};

struct ResultBookmark {
    base::Uuid id{base::Uuid::generate_random()};
    std::string name;
    double target_time{0.0};
    std::string view_state_json;
};

// =============================================================================
// OBS-08 & OBS-09: Data Export, Sidecars & Provenance
// =============================================================================

struct ResultProvenance {
    base::Uuid run_id{};
    std::string project_content_hash;
    std::string solver_name;
    base::Version engine_version{1, 0, 0, "final"};
    std::string timestamp_iso;
    std::string host_info;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<ResultProvenance> from_json(std::string_view json_str);
};

class ResultExporter {
public:
    [[nodiscard]] static std::string export_csv(const TimeSeriesStore& store, std::string_view signal_name = "value");
    [[nodiscard]] static std::string export_json(const TimeSeriesStore& store, const ResultProvenance& provenance);
    [[nodiscard]] static std::string generate_sidecar(const ResultProvenance& provenance, const std::vector<ProbeDefinition>& probes);
};

// =============================================================================
// OBS-10: Large-Result Retention & Cleanup Policies
// =============================================================================

enum class EvictionPolicy {
    LRU,
    FIFO,
    BudgetCap
};

class ResultRetentionManager {
public:
    explicit ResultRetentionManager(size_t memory_budget_bytes = 100 * 1024 * 1024); // 100 MB default

    void register_store(base::Uuid store_id, std::shared_ptr<TimeSeriesStore> store, int priority = 0);
    void unregister_store(const base::Uuid& store_id);

    [[nodiscard]] size_t current_memory_usage() const noexcept;
    [[nodiscard]] size_t memory_budget() const noexcept { return budget_bytes_; }
    void set_memory_budget(size_t budget) noexcept { budget_bytes_ = budget; }

    // Evicts lowest priority / oldest samples until usage is below budget
    size_t enforce_retention(EvictionPolicy policy = EvictionPolicy::BudgetCap);

private:
    struct StoreEntry {
        base::Uuid id;
        std::shared_ptr<TimeSeriesStore> store;
        int priority{0};
        std::chrono::steady_clock::time_point last_access;
    };

    size_t budget_bytes_;
    std::vector<StoreEntry> stores_;
};

} // namespace shunya::core::domain
