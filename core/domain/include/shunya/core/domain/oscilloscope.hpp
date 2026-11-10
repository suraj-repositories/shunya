#pragma once

#include "shunya/core/domain/scientific_plot.hpp"
#include "shunya/core/domain/digital_logic.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace shunya::core::domain {

enum class TriggerEdge {
    None,
    Rising,
    Falling,
    Either
};

struct TriggerConfig {
    TriggerEdge edge{TriggerEdge::Rising};
    double level{0.0};
    double hysteresis{0.05};
    size_t source_channel{0};
    double pre_trigger_fraction{0.25};
};

struct TriggerResult {
    bool triggered{false};
    size_t sample_index{0};
    double trigger_time{0.0};
    TriggerEdge detected_edge{TriggerEdge::None};
};

struct SignalMetrics {
    double v_min{0.0};
    double v_max{0.0};
    double v_pp{0.0};
    double v_avg{0.0};
    double v_rms{0.0};
    double frequency_hz{0.0};
    double period_s{0.0};
};

struct AnalogChannel {
    std::string name;
    std::string color_hex{"#61afef"};
    double volts_per_div{1.0};
    double offset_v{0.0};
    bool enabled{true};
    std::vector<PlotPoint2D> samples;
};

struct LogicSample {
    double time{0.0};
    std::vector<LogicLevel> levels;
};

struct DecodedBusWord {
    double start_time{0.0};
    double end_time{0.0};
    uint64_t value{0};
    bool has_unknown{false};
    bool has_highz{false};
    std::string hex_str;
    std::string bin_str;
};

/**
 * @brief Signal processing, trigger alignment, and measurement engine for multi-channel oscilloscopes.
 */
class OscilloscopeAnalyzer {
public:
    /**
     * @brief Detects trigger crossing event using hysteresis arming and linear sub-sample interpolation.
     */
    [[nodiscard]] static TriggerResult find_trigger(
        std::span<const PlotPoint2D> samples,
        const TriggerConfig& config,
        size_t start_index = 0
    );

    /**
     * @brief Computes Vmin, Vmax, Vpp, Vavg, Vrms, and fundamental frequency from midpoint crossings.
     */
    [[nodiscard]] static SignalMetrics measure_signal(
        std::span<const PlotPoint2D> samples
    );

    /**
     * @brief Extracts time-aligned trace window around detected trigger point.
     */
    [[nodiscard]] static std::vector<PlotPoint2D> extract_triggered_window(
        std::span<const PlotPoint2D> samples,
        double trigger_time,
        double time_span,
        double pre_trigger_fraction = 0.25
    );
};

/**
 * @brief Parallel and clocked bus decoder for IEEE 4-state digital logic analyzer waveforms.
 */
class LogicAnalyzerDecoder {
public:
    /**
     * @brief Decodes specified bit channels (LSB first at bit_indices[0]) across LogicSample records.
     */
    [[nodiscard]] static std::vector<DecodedBusWord> decode_parallel_bus(
        std::span<const LogicSample> samples,
        const std::vector<size_t>& bit_indices
    );

    /**
     * @brief Formats a uint64_t bus value as fixed-width hexadecimal and binary strings.
     */
    [[nodiscard]] static std::string format_hex(uint64_t value, size_t bit_count);
    [[nodiscard]] static std::string format_bin(
        std::span<const LogicLevel> bits,
        const std::vector<size_t>& bit_indices
    );
};

} // namespace shunya::core::domain
