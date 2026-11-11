#include "oscilloscope_controller.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::gui {

ShunyaOscilloscopeController::ShunyaOscilloscopeController(QObject* parent)
    : QObject(parent)
{
    trigger_cfg_.edge = core::domain::TriggerEdge::Rising;
    trigger_cfg_.level = 1.0;
    trigger_cfg_.hysteresis = 0.05;
    loadDemoCapture("mixed_signal");
}

QString ShunyaOscilloscopeController::triggerEdge() const {
    switch (trigger_cfg_.edge) {
        case core::domain::TriggerEdge::Rising:  return "Rising";
        case core::domain::TriggerEdge::Falling: return "Falling";
        case core::domain::TriggerEdge::Either:  return "Either";
        default: return "None";
    }
}

void ShunyaOscilloscopeController::setTriggerEdge(const QString& edge) {
    core::domain::TriggerEdge next = core::domain::TriggerEdge::Rising;
    if (edge == "Falling") next = core::domain::TriggerEdge::Falling;
    else if (edge == "Either") next = core::domain::TriggerEdge::Either;
    else if (edge == "None") next = core::domain::TriggerEdge::None;

    if (trigger_cfg_.edge != next) {
        trigger_cfg_.edge = next;
        recompute_trigger_and_metrics();
        emit triggerChanged();
        emit waveformChanged();
    }
}

void ShunyaOscilloscopeController::setTriggerLevel(double lvl) {
    if (trigger_cfg_.level != lvl) {
        trigger_cfg_.level = lvl;
        recompute_trigger_and_metrics();
        emit triggerChanged();
        emit waveformChanged();
    }
}

void ShunyaOscilloscopeController::setTimeSpanMs(double ms) {
    double s = std::max(0.0005, ms * 0.001);
    if (time_span_s_ != s) {
        time_span_s_ = s;
        emit waveformChanged();
    }
}

void ShunyaOscilloscopeController::loadDemoCapture(const QString& mode) {
    analog_channels_.clear();
    logic_samples_.clear();

    core::domain::AnalogChannel ch1;
    ch1.name = "CH1: V_in (500Hz Sine)";
    ch1.color_hex = "#e5c07b";

    core::domain::AnalogChannel ch2;
    ch2.name = (mode == "pwm") ? "CH2: PWM Gate" : "CH2: V_out (RLC Filter)";
    ch2.color_hex = "#61afef";

    const int N = 1000;
    double dt = 0.00002; // 20 us step -> 20 ms total capture
    for (int i = 0; i < N; ++i) {
        double t = i * dt;
        double v1 = 3.3 * std::sin(2.0 * 3.14159265358979323846 * 500.0 * t);
        double v2 = (mode == "pwm")
            ? (std::fmod(t * 1000.0, 1.0) < 0.35 ? 3.3 : 0.0)
            : 2.2 * std::sin(2.0 * 3.14159265358979323846 * 500.0 * t - 0.65);
        ch1.samples.push_back({t, v1});
        ch2.samples.push_back({t, v2});
    }

    analog_channels_.push_back(std::move(ch1));
    analog_channels_.push_back(std::move(ch2));

    // Synthesize 4-bit logic bus + Clock across 16 steps (0 to 10ms)
    const uint8_t bytes[] = {0xA, 0x5, 0xF, 0x3, 0xC, 0x9, 0x0, 0xE};
    for (int step = 0; step < 8; ++step) {
        double t = step * (0.010 / 8.0);
        uint8_t val = bytes[step];
        core::domain::LogicSample s;
        s.time = t;
        // Channel 0: CLK alternating, Channels 1..4: D0..D3
        s.levels.push_back((step % 2 == 0) ? core::domain::LogicLevel::Low : core::domain::LogicLevel::High);
        for (int b = 0; b < 4; ++b) {
            s.levels.push_back(((val >> b) & 1) ? core::domain::LogicLevel::High : core::domain::LogicLevel::Low);
        }
        logic_samples_.push_back(std::move(s));
    }

    recompute_trigger_and_metrics();
    emit waveformChanged();
}

void ShunyaOscilloscopeController::loadLiveChannels(
    const std::vector<double>& timeS,
    const std::vector<double>& ch1Values,
    const std::vector<double>& ch2Values) {
    if (timeS.empty()) return;
    analog_channels_.clear();

    core::domain::AnalogChannel c1;
    c1.name = "CH1: Node Voltage (V)";
    c1.color_hex = "#2563EB";
    for (size_t i = 0; i < timeS.size() && i < ch1Values.size(); ++i) {
        c1.samples.push_back({timeS[i], ch1Values[i]});
    }
    analog_channels_.push_back(std::move(c1));

    core::domain::AnalogChannel c2;
    c2.name = "CH2: Branch Current (A)";
    c2.color_hex = "#EA580C";
    for (size_t i = 0; i < timeS.size() && i < ch2Values.size(); ++i) {
        c2.samples.push_back({timeS[i], ch2Values[i]});
    }
    analog_channels_.push_back(std::move(c2));

    time_span_s_ = std::max(1e-3, timeS.back() - timeS.front());
    recompute_trigger_and_metrics();
    emit waveformChanged();
}

void ShunyaOscilloscopeController::recompute_trigger_and_metrics() {
    if (analog_channels_.size() >= 1) {
        ch1_metrics_ = core::domain::OscilloscopeAnalyzer::measure_signal(analog_channels_[0].samples);
        last_trig_ = core::domain::OscilloscopeAnalyzer::find_trigger(analog_channels_[0].samples, trigger_cfg_, 0);
    }
    if (analog_channels_.size() >= 2) {
        ch2_metrics_ = core::domain::OscilloscopeAnalyzer::measure_signal(analog_channels_[1].samples);
    }
    decoded_words_ = core::domain::LogicAnalyzerDecoder::decode_parallel_bus(logic_samples_, {1, 2, 3, 4});
}

QVariantList ShunyaOscilloscopeController::getAnalogTraceNormalized(int channelIdx) const {
    QVariantList out;
    if (channelIdx < 0 || channelIdx >= static_cast<int>(analog_channels_.size())) return out;

    double trig_t = last_trig_.triggered ? last_trig_.trigger_time : 0.005;
    auto window = core::domain::OscilloscopeAnalyzer::extract_triggered_window(
        analog_channels_[channelIdx].samples,
        trig_t,
        time_span_s_,
        trigger_cfg_.pre_trigger_fraction
    );

    double pre_s = trigger_cfg_.pre_trigger_fraction * time_span_s_;
    double x_start = -pre_s;

    for (const auto& pt : window) {
        QVariantMap m;
        m["x"] = std::clamp((pt.x - x_start) / time_span_s_, 0.0, 1.0);
        m["y"] = std::clamp(0.5 - (pt.y / (2.0 * v_range_half_)), 0.0, 1.0);
        out.append(m);
    }
    return out;
}

QVariantList ShunyaOscilloscopeController::getLogicChannels() const {
    QVariantList channels;
    const QString names[] = {"CLK", "D0 (LSB)", "D1", "D2", "D3 (MSB)"};
    double total_t = 0.010;

    for (int ch = 0; ch < 5; ++ch) {
        QVariantMap chObj;
        chObj["name"] = names[ch];
        QVariantList transitions;
        for (size_t i = 0; i < logic_samples_.size(); ++i) {
            QVariantMap tr;
            double next_t = (i + 1 < logic_samples_.size()) ? logic_samples_[i + 1].time : total_t;
            tr["startNorm"] = std::clamp(logic_samples_[i].time / total_t, 0.0, 1.0);
            tr["endNorm"] = std::clamp(next_t / total_t, 0.0, 1.0);
            tr["level"] = static_cast<int>(logic_samples_[i].levels[ch]);
            transitions.append(tr);
        }
        chObj["segments"] = transitions;
        channels.append(chObj);
    }
    return channels;
}

QVariantList ShunyaOscilloscopeController::getDecodedBusWords() const {
    QVariantList out;
    double total_t = 0.010;
    for (size_t i = 0; i < decoded_words_.size(); ++i) {
        const auto& w = decoded_words_[i];
        double end_t = (w.end_time > w.start_time) ? w.end_time : total_t;
        QVariantMap m;
        m["startNorm"] = std::clamp(w.start_time / total_t, 0.0, 1.0);
        m["endNorm"] = std::clamp(end_t / total_t, 0.0, 1.0);
        m["hex"] = QString::fromStdString(w.hex_str);
        m["bin"] = QString::fromStdString(w.bin_str);
        m["hasUnknown"] = w.has_unknown;
        out.append(m);
    }
    return out;
}

} // namespace shunya::gui
