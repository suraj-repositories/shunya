#include "shunya/core/domain/oscilloscope.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace shunya::core::domain {

TriggerResult OscilloscopeAnalyzer::find_trigger(
    std::span<const PlotPoint2D> samples,
    const TriggerConfig& config,
    size_t start_index
) {
    TriggerResult res;
    if (samples.size() < 2 || start_index >= samples.size() - 1 || config.edge == TriggerEdge::None) {
        return res;
    }

    double hyst = std::max(0.0, config.hysteresis);
    double arm_low = config.level - hyst;
    double arm_high = config.level + hyst;

    bool armed_rising = (samples[start_index].y <= arm_low);
    bool armed_falling = (samples[start_index].y >= arm_high);

    for (size_t i = start_index + 1; i < samples.size(); ++i) {
        double y0 = samples[i - 1].y;
        double y1 = samples[i].y;

        if (!armed_rising && y0 <= arm_low) {
            armed_rising = true;
        }
        if (!armed_falling && y0 >= arm_high) {
            armed_falling = true;
        }

        bool want_rising = (config.edge == TriggerEdge::Rising || config.edge == TriggerEdge::Either);
        bool want_falling = (config.edge == TriggerEdge::Falling || config.edge == TriggerEdge::Either);

        if (want_rising && armed_rising && y0 < config.level && y1 >= config.level) {
            double dy = y1 - y0;
            double frac = (std::abs(dy) > 1e-12) ? (config.level - y0) / dy : 0.0;
            frac = std::clamp(frac, 0.0, 1.0);
            res.triggered = true;
            res.sample_index = i;
            res.trigger_time = samples[i - 1].x + frac * (samples[i].x - samples[i - 1].x);
            res.detected_edge = TriggerEdge::Rising;
            return res;
        }

        if (want_falling && armed_falling && y0 > config.level && y1 <= config.level) {
            double dy = y1 - y0;
            double frac = (std::abs(dy) > 1e-12) ? (config.level - y0) / dy : 0.0;
            frac = std::clamp(frac, 0.0, 1.0);
            res.triggered = true;
            res.sample_index = i;
            res.trigger_time = samples[i - 1].x + frac * (samples[i].x - samples[i - 1].x);
            res.detected_edge = TriggerEdge::Falling;
            return res;
        }
    }

    return res;
}

SignalMetrics OscilloscopeAnalyzer::measure_signal(std::span<const PlotPoint2D> samples) {
    SignalMetrics m;
    if (samples.empty()) return m;

    double lo = samples[0].y;
    double hi = samples[0].y;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (const auto& p : samples) {
        if (p.y < lo) lo = p.y;
        if (p.y > hi) hi = p.y;
        sum += p.y;
        sum_sq += p.y * p.y;
    }

    double n = static_cast<double>(samples.size());
    m.v_min = lo;
    m.v_max = hi;
    m.v_pp = hi - lo;
    m.v_avg = sum / n;
    m.v_rms = std::sqrt(sum_sq / n);

    if (m.v_pp > 1e-9 && samples.size() >= 3) {
        double mid = 0.5 * (lo + hi);
        std::vector<double> rising_times;
        TriggerConfig trig;
        trig.edge = TriggerEdge::Rising;
        trig.level = mid;
        trig.hysteresis = 0.05 * m.v_pp;

        size_t cursor = 0;
        while (cursor < samples.size() - 1) {
            auto tr = find_trigger(samples, trig, cursor);
            if (!tr.triggered) break;
            rising_times.push_back(tr.trigger_time);
            cursor = tr.sample_index + 1;
        }

        if (rising_times.size() >= 2) {
            double total_period = rising_times.back() - rising_times.front();
            double avg_period = total_period / static_cast<double>(rising_times.size() - 1);
            if (avg_period > 1e-15) {
                m.period_s = avg_period;
                m.frequency_hz = 1.0 / avg_period;
            }
        }
    }

    return m;
}

std::vector<PlotPoint2D> OscilloscopeAnalyzer::extract_triggered_window(
    std::span<const PlotPoint2D> samples,
    double trigger_time,
    double time_span,
    double pre_trigger_fraction
) {
    std::vector<PlotPoint2D> out;
    if (samples.empty() || time_span <= 0.0) return out;

    double pre = std::clamp(pre_trigger_fraction, 0.0, 1.0) * time_span;
    double t_start = trigger_time - pre;
    double t_end = t_start + time_span;

    for (const auto& p : samples) {
        if (p.x >= t_start && p.x <= t_end) {
            out.push_back(PlotPoint2D{p.x - trigger_time, p.y});
        }
    }
    return out;
}

std::string LogicAnalyzerDecoder::format_hex(uint64_t value, size_t bit_count) {
    if (bit_count == 0) return "0x0";
    size_t hex_digits = (bit_count + 3) / 4;
    char fmt[16];
    std::snprintf(fmt, sizeof(fmt), "0x%%0%zuX", hex_digits);
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt, static_cast<unsigned long long>(value));
    return std::string(buf);
}

std::string LogicAnalyzerDecoder::format_bin(
    std::span<const LogicLevel> bits,
    const std::vector<size_t>& bit_indices
) {
    std::string out = "0b";
    // MSB first visually
    for (auto it = bit_indices.rbegin(); it != bit_indices.rend(); ++it) {
        size_t idx = *it;
        if (idx >= bits.size()) {
            out.push_back('X');
        } else {
            out.push_back(logic_level_to_char(bits[idx]));
        }
    }
    return out;
}

std::vector<DecodedBusWord> LogicAnalyzerDecoder::decode_parallel_bus(
    std::span<const LogicSample> samples,
    const std::vector<size_t>& bit_indices
) {
    std::vector<DecodedBusWord> words;
    if (samples.empty() || bit_indices.empty()) return words;

    auto decode_sample = [&](const LogicSample& s) -> DecodedBusWord {
        DecodedBusWord w;
        w.start_time = s.time;
        w.end_time = s.time;
        uint64_t val = 0;
        for (size_t b = 0; b < bit_indices.size() && b < 64; ++b) {
            size_t ch = bit_indices[b];
            if (ch >= s.levels.size()) {
                w.has_unknown = true;
            } else {
                LogicLevel lvl = s.levels[ch];
                if (lvl == LogicLevel::High) {
                    val |= (uint64_t{1} << b);
                } else if (lvl == LogicLevel::Unknown) {
                    w.has_unknown = true;
                } else if (lvl == LogicLevel::HighZ) {
                    w.has_highz = true;
                }
            }
        }
        w.value = val;
        w.bin_str = format_bin(s.levels, bit_indices);
        if (w.has_unknown) {
            w.hex_str = "XX";
        } else if (w.has_highz) {
            w.hex_str = "ZZ";
        } else {
            w.hex_str = format_hex(val, bit_indices.size());
        }
        return w;
    };

    DecodedBusWord current = decode_sample(samples[0]);
    for (size_t i = 1; i < samples.size(); ++i) {
        DecodedBusWord next = decode_sample(samples[i]);
        bool same_state = (next.value == current.value &&
                           next.has_unknown == current.has_unknown &&
                           next.has_highz == current.has_highz &&
                           next.bin_str == current.bin_str);
        if (same_state) {
            current.end_time = next.start_time;
        } else {
            current.end_time = next.start_time;
            words.push_back(std::move(current));
            current = std::move(next);
        }
    }
    words.push_back(std::move(current));
    return words;
}

} // namespace shunya::core::domain
