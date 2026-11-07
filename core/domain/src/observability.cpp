#include "shunya/core/domain/observability.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string unescape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '"': out += '"'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case '/': out += '/'; ++i; break;
                case 'n': out += '\n'; ++i; break;
                case 'r': out += '\r'; ++i; break;
                case 't': out += '\t'; ++i; break;
                default: out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace

// =============================================================================
// OBS-01: Probes
// =============================================================================

std::optional<MeasurementType> measurement_type_from_string(std::string_view str) noexcept {
    if (str == "Scalar") return MeasurementType::Scalar;
    if (str == "Vector") return MeasurementType::Vector;
    if (str == "Tensor") return MeasurementType::Tensor;
    if (str == "SpatialField") return MeasurementType::SpatialField;
    if (str == "DiscreteEvent") return MeasurementType::DiscreteEvent;
    return std::nullopt;
}

std::string ProbeDefinition::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name) << "\",\n";
    ss << "  \"target_entity_id\": \"" << target_entity_id.to_string() << "\",\n";
    ss << "  \"target_variable_or_port\": \"" << escape_json(target_variable_or_port) << "\",\n";
    ss << "  \"type\": \"" << measurement_type_to_string(type) << "\",\n";
    ss << "  \"unit_symbol\": \"" << escape_json(unit_symbol) << "\",\n";
    ss << "  \"sampling_rate_hz\": " << sampling_rate_hz << "\n";
    ss << "}";
    return ss.str();
}

base::Result<ProbeDefinition> ProbeDefinition::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::optional<std::string> {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str();
        return std::nullopt;
    };

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stod(m[1].str());
        return std::nullopt;
    };

    ProbeDefinition def;
    if (auto v = extract_str("id")) {
        auto uuid_opt = base::Uuid::from_string(*v);
        if (uuid_opt.has_value()) def.id = uuid_opt.value();
    }
    if (auto v = extract_str("name")) def.name = unescape_json(*v);
    if (auto v = extract_str("target_entity_id")) {
        auto uuid_opt = base::Uuid::from_string(*v);
        if (uuid_opt.has_value()) def.target_entity_id = uuid_opt.value();
    }
    if (auto v = extract_str("target_variable_or_port")) def.target_variable_or_port = unescape_json(*v);
    if (auto v = extract_str("type")) {
        if (auto t = measurement_type_from_string(*v)) def.type = *t;
    }
    if (auto v = extract_str("unit_symbol")) def.unit_symbol = unescape_json(*v);
    if (auto v = extract_num("sampling_rate_hz")) def.sampling_rate_hz = *v;

    return def;
}

// =============================================================================
// OBS-02: Dashboard
// =============================================================================

Dashboard::Dashboard(std::string name) : name_(std::move(name)) {}

void Dashboard::add_widget(DashboardWidget widget) {
    widgets_.push_back(std::move(widget));
}

bool Dashboard::remove_widget(const base::Uuid& widget_id) {
    const size_t orig_size = widgets_.size();
    widgets_.erase(
        std::remove_if(widgets_.begin(), widgets_.end(),
                       [&](const DashboardWidget& w) { return w.widget_id == widget_id; }),
        widgets_.end()
    );
    return widgets_.size() < orig_size;
}

std::string Dashboard::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << escape_json(name_) << "\",\n";
    ss << "  \"widgets\": [\n";
    for (size_t i = 0; i < widgets_.size(); ++i) {
        const auto& w = widgets_[i];
        ss << "    {\n";
        ss << "      \"widget_id\": \"" << w.widget_id.to_string() << "\",\n";
        ss << "      \"title\": \"" << escape_json(w.title) << "\",\n";
        ss << "      \"type\": \"" << widget_type_to_string(w.type) << "\",\n";
        ss << "      \"min_display_range\": " << w.min_display_range << ",\n";
        ss << "      \"max_display_range\": " << w.max_display_range << "\n";
        ss << "    }";
        if (i + 1 < widgets_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<Dashboard> Dashboard::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::optional<std::string> {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str();
        return std::nullopt;
    };

    Dashboard dash;
    if (auto v = extract_str("name")) dash.set_name(unescape_json(*v));

    // Parse widgets
    std::regex w_re(R"re(\{\s*"widget_id"\s*:\s*"([^"]*)"\s*,\s*"title"\s*:\s*"([^"]*)"\s*,\s*"type"\s*:\s*"([^"]*)")re");
    auto it_begin = std::sregex_iterator(s.begin(), s.end(), w_re);
    auto it_end = std::sregex_iterator();

    for (auto it = it_begin; it != it_end; ++it) {
        DashboardWidget w;
        auto id_opt = base::Uuid::from_string((*it)[1].str());
        if (id_opt.has_value()) w.widget_id = id_opt.value();
        w.title = unescape_json((*it)[2].str());
        std::string type_str = (*it)[3].str();
        if (type_str == "TimeSeriesChart") w.type = WidgetType::TimeSeriesChart;
        else if (type_str == "Gauge") w.type = WidgetType::Gauge;
        else if (type_str == "Table") w.type = WidgetType::Table;
        else if (type_str == "StatusIndicator") w.type = WidgetType::StatusIndicator;
        else if (type_str == "Histogram") w.type = WidgetType::Histogram;

        dash.add_widget(std::move(w));
    }

    return dash;
}

// =============================================================================
// OBS-03: TimeSeriesStore
// =============================================================================

TimeSeriesStore::TimeSeriesStore(size_t max_capacity, size_t ring_buffer_size)
    : max_capacity_(max_capacity), live_ring_buffer_(ring_buffer_size) {
    samples_.reserve(std::min(max_capacity, static_cast<size_t>(10000)));
}

bool TimeSeriesStore::push_live(double t, double v) noexcept {
    return live_ring_buffer_.push({t, v});
}

size_t TimeSeriesStore::flush_live_buffer() noexcept {
    size_t count = 0;
    while (auto sample = live_ring_buffer_.pop()) {
        append(sample->t, sample->v);
        ++count;
    }
    return count;
}

void TimeSeriesStore::append(double t, double v) {
    if (samples_.size() >= max_capacity_) {
        // Drop oldest 10%
        const size_t drop = max_capacity_ / 10;
        samples_.erase(samples_.begin(), samples_.begin() + drop);
    }
    samples_.push_back({t, v});
}

std::vector<TimeSeriesSample> TimeSeriesStore::query_range(
    double t_start, double t_end, size_t max_points, DecimationPolicy policy
) const {
    std::vector<TimeSeriesSample> in_range;
    for (const auto& s : samples_) {
        if (s.t >= t_start && s.t <= t_end) {
            in_range.push_back(s);
        }
    }

    if (in_range.size() <= max_points || policy == DecimationPolicy::None || max_points < 2) {
        return in_range;
    }

    std::vector<TimeSeriesSample> decimated;
    decimated.reserve(max_points);

    if (policy == DecimationPolicy::Average) {
        const double bucket_size = static_cast<double>(in_range.size()) / static_cast<double>(max_points);
        for (size_t b = 0; b < max_points; ++b) {
            const size_t start_idx = static_cast<size_t>(b * bucket_size);
            const size_t end_idx = std::min(in_range.size(), static_cast<size_t>((b + 1) * bucket_size));
            if (start_idx >= end_idx) continue;

            double sum_t = 0.0;
            double sum_v = 0.0;
            for (size_t i = start_idx; i < end_idx; ++i) {
                sum_t += in_range[i].t;
                sum_v += in_range[i].v;
            }
            const double count = static_cast<double>(end_idx - start_idx);
            decimated.push_back({sum_t / count, sum_v / count});
        }
    } else { // MinMax
        const size_t num_buckets = max_points / 2;
        const double bucket_size = static_cast<double>(in_range.size()) / static_cast<double>(num_buckets > 0 ? num_buckets : 1);

        for (size_t b = 0; b < num_buckets; ++b) {
            const size_t start_idx = static_cast<size_t>(b * bucket_size);
            const size_t end_idx = std::min(in_range.size(), static_cast<size_t>((b + 1) * bucket_size));
            if (start_idx >= end_idx) continue;

            size_t min_idx = start_idx;
            size_t max_idx = start_idx;
            for (size_t i = start_idx + 1; i < end_idx; ++i) {
                if (in_range[i].v < in_range[min_idx].v) min_idx = i;
                if (in_range[i].v > in_range[max_idx].v) max_idx = i;
            }

            if (min_idx < max_idx) {
                decimated.push_back(in_range[min_idx]);
                decimated.push_back(in_range[max_idx]);
            } else {
                decimated.push_back(in_range[max_idx]);
                decimated.push_back(in_range[min_idx]);
            }
        }
    }

    return decimated;
}

double TimeSeriesStore::min_value() const noexcept {
    if (samples_.empty()) return 0.0;
    double min_v = samples_[0].v;
    for (const auto& s : samples_) {
        if (s.v < min_v) min_v = s.v;
    }
    return min_v;
}

double TimeSeriesStore::max_value() const noexcept {
    if (samples_.empty()) return 0.0;
    double max_v = samples_[0].v;
    for (const auto& s : samples_) {
        if (s.v > max_v) max_v = s.v;
    }
    return max_v;
}

double TimeSeriesStore::mean_value() const noexcept {
    if (samples_.empty()) return 0.0;
    double sum = 0.0;
    for (const auto& s : samples_) sum += s.v;
    return sum / static_cast<double>(samples_.size());
}

void TimeSeriesStore::clear() noexcept {
    live_ring_buffer_.clear();
    samples_.clear();
}

// =============================================================================
// OBS-04: Spatial Field Visualization
// =============================================================================

double SpatialFieldData::min_scalar() const noexcept {
    if (scalar_values.empty()) return 0.0;
    return *std::min_element(scalar_values.begin(), scalar_values.end());
}

double SpatialFieldData::max_scalar() const noexcept {
    if (scalar_values.empty()) return 0.0;
    return *std::max_element(scalar_values.begin(), scalar_values.end());
}

SpatialFieldData SpatialFieldData::slice_z(double z_slice, double tolerance) const {
    SpatialFieldData slice;
    slice.field_name = field_name + "_sliceZ";
    slice.timestamp = timestamp;

    for (size_t i = 0; i < points.size(); ++i) {
        if (std::abs(points[i].z - z_slice) <= tolerance) {
            slice.points.push_back(points[i]);
            if (i < scalar_values.size()) slice.scalar_values.push_back(scalar_values[i]);
            if (i < vector_values.size()) slice.vector_values.push_back(vector_values[i]);
        }
    }

    return slice;
}

// =============================================================================
// OBS-05: Virtual Instruments
// =============================================================================

void VirtualOscilloscope::reset() {
    triggered_ = false;
    trace_buffer_.clear();
}

void VirtualOscilloscope::feed_sample(double t, double v) {
    if (!triggered_) {
        if (trigger_mode_ == TriggerMode::Auto ||
            (last_v_ < trigger_level_ && v >= trigger_level_)) {
            triggered_ = true;
            trace_buffer_.clear();
        }
    }

    if (triggered_) {
        trace_buffer_.push_back({t, v});
        // Limit trace buffer to 10 time divisions
        const double max_trace_duration = 10.0 * time_base_;
        if (!trace_buffer_.empty() && (t - trace_buffer_.front().t) > max_trace_duration) {
            if (trigger_mode_ == TriggerMode::Single) {
                // Keep frozen
            } else {
                triggered_ = false; // Arm for next trigger
            }
        }
    }

    last_v_ = v;
}

void VirtualMultimeter::sample(double val) noexcept {
    readings_.push_back(val);
    if (readings_.size() > 1000) {
        readings_.erase(readings_.begin());
    }
}

double VirtualMultimeter::current_reading() const noexcept {
    if (readings_.empty()) return 0.0;

    if (mode_ == Mode::VoltageAC) {
        // RMS calculation
        double sum_sq = 0.0;
        for (double v : readings_) sum_sq += v * v;
        return std::sqrt(sum_sq / static_cast<double>(readings_.size()));
    }

    // DC Voltage, Current, Resistance: arithmetic mean
    double sum = 0.0;
    for (double v : readings_) sum += v;
    return sum / static_cast<double>(readings_.size());
}

double VirtualThermometer::read(Scale scale) const noexcept {
    switch (scale) {
        case Scale::Kelvin: return current_temp_kelvin_;
        case Scale::Celsius: return current_temp_kelvin_ - 273.15;
        case Scale::Fahrenheit: return (current_temp_kelvin_ - 273.15) * 9.0 / 5.0 + 32.0;
    }
    return current_temp_kelvin_ - 273.15;
}

// =============================================================================
// OBS-06: Axis & Value Formatting
// =============================================================================

std::string AxisFormatting::format_value(double val) const {
    if (!use_si_prefixes || std::abs(val) < 1e-15) {
        std::ostringstream ss;
        ss << std::setprecision(significant_digits) << val << " " << unit_symbol;
        return ss.str();
    }

    const double abs_val = std::abs(val);
    double scaled_val = val;
    std::string prefix;

    if (abs_val >= 1e9) { scaled_val /= 1e9; prefix = "G"; }
    else if (abs_val >= 1e6) { scaled_val /= 1e6; prefix = "M"; }
    else if (abs_val >= 1e3) { scaled_val /= 1e3; prefix = "k"; }
    else if (abs_val < 1e-6) { scaled_val *= 1e9; prefix = "n"; }
    else if (abs_val < 1e-3) { scaled_val *= 1e6; prefix = "u"; }
    else if (abs_val < 1.0)  { scaled_val *= 1e3; prefix = "m"; }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(significant_digits) << scaled_val << " " << prefix << unit_symbol;
    return ss.str();
}

// =============================================================================
// OBS-08 & OBS-09: Export, Sidecar & Provenance
// =============================================================================

std::string ResultProvenance::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"run_id\": \"" << run_id.to_string() << "\",\n";
    ss << "  \"project_content_hash\": \"" << project_content_hash << "\",\n";
    ss << "  \"solver_name\": \"" << solver_name << "\",\n";
    ss << "  \"engine_version\": \"" << engine_version.as_string() << "\",\n";
    ss << "  \"timestamp_iso\": \"" << timestamp_iso << "\",\n";
    ss << "  \"host_info\": \"" << escape_json(host_info) << "\"\n";
    ss << "}";
    return ss.str();
}

base::Result<ResultProvenance> ResultProvenance::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::optional<std::string> {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str();
        return std::nullopt;
    };

    ResultProvenance prov;
    if (auto v = extract_str("run_id")) {
        auto uuid_opt = base::Uuid::from_string(*v);
        if (uuid_opt.has_value()) prov.run_id = uuid_opt.value();
    }
    if (auto v = extract_str("project_content_hash")) prov.project_content_hash = *v;
    if (auto v = extract_str("solver_name")) prov.solver_name = *v;
    if (auto v = extract_str("engine_version")) {
        auto ver_res = base::Version::parse(*v);
        if (ver_res.is_ok()) prov.engine_version = ver_res.value();
    }
    if (auto v = extract_str("timestamp_iso")) prov.timestamp_iso = *v;
    if (auto v = extract_str("host_info")) prov.host_info = unescape_json(*v);

    return prov;
}

std::string ResultExporter::export_csv(const TimeSeriesStore& store, std::string_view signal_name) {
    std::ostringstream ss;
    ss << "time," << signal_name << "\n";
    for (const auto& s : store.raw_samples()) {
        ss << s.t << "," << s.v << "\n";
    }
    return ss.str();
}

std::string ResultExporter::export_json(const TimeSeriesStore& store, const ResultProvenance& provenance) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"provenance\": " << provenance.to_json() << ",\n";
    ss << "  \"data\": [\n";
    const auto& samples = store.raw_samples();
    for (size_t i = 0; i < samples.size(); ++i) {
        ss << "    [" << samples[i].t << ", " << samples[i].v << "]";
        if (i + 1 < samples.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

std::string ResultExporter::generate_sidecar(
    const ResultProvenance& provenance, const std::vector<ProbeDefinition>& probes
) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"provenance\": " << provenance.to_json() << ",\n";
    ss << "  \"probes\": [\n";
    for (size_t i = 0; i < probes.size(); ++i) {
        ss << probes[i].to_json();
        if (i + 1 < probes.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// OBS-10: Result Retention Manager
// =============================================================================

ResultRetentionManager::ResultRetentionManager(size_t memory_budget_bytes)
    : budget_bytes_(memory_budget_bytes) {}

void ResultRetentionManager::register_store(base::Uuid store_id, std::shared_ptr<TimeSeriesStore> store, int priority) {
    stores_.push_back({store_id, std::move(store), priority, std::chrono::steady_clock::now()});
}

void ResultRetentionManager::unregister_store(const base::Uuid& store_id) {
    stores_.erase(
        std::remove_if(stores_.begin(), stores_.end(),
                       [&](const StoreEntry& e) { return e.id == store_id; }),
        stores_.end()
    );
}

size_t ResultRetentionManager::current_memory_usage() const noexcept {
    size_t total_bytes = 0;
    for (const auto& e : stores_) {
        if (e.store) {
            total_bytes += e.store->size() * sizeof(TimeSeriesSample);
        }
    }
    return total_bytes;
}

size_t ResultRetentionManager::enforce_retention(EvictionPolicy policy) {
    (void)policy;
    size_t evicted_samples = 0;

    while (current_memory_usage() > budget_bytes_ && !stores_.empty()) {
        // Sort stores by priority ascending
        std::sort(stores_.begin(), stores_.end(), [](const StoreEntry& a, const StoreEntry& b) {
            return a.priority < b.priority;
        });

        auto& target = stores_.front();
        if (target.store && !target.store->empty()) {
            const size_t before = target.store->size();
            target.store->clear();
            evicted_samples += before;
        } else {
            stores_.erase(stores_.begin());
        }
    }

    return evicted_samples;
}

} // namespace shunya::core::domain
