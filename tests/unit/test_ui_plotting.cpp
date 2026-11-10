#include "tests/test_framework.hpp"

#include "shunya/core/domain/scientific_plot.hpp"
#include "shunya/core/domain/colormap_tables.hpp"
#include "shunya/core/domain/oscilloscope.hpp"

#include <chrono>
#include <cmath>
#include <vector>

using namespace shunya::core::domain;

namespace {
bool is_approx(double a, double b, double eps = 1e-2) {
    return std::abs(a - b) <= eps;
}
} // namespace

// =============================================================================
// Task UI-03-002-01: GPU Plotter Decimation & Axis Calculator
// =============================================================================

TEST_CASE("UI Plotting: MinMax Decimator 10M Stress & Transient Peak Preservation") {
    const size_t N = 10000000;
    std::vector<PlotPoint2D> data;
    data.resize(N);
    for (size_t i = 0; i < N; ++i) {
        double t = static_cast<double>(i) * 1e-6; // 0 to 10s
        data[i] = {t, std::sin(t * 10.0)};
    }

    // Inject sharp positive and negative transient spikes at known indices
    const size_t spike_pos_idx = 3456789;
    const size_t spike_neg_idx = 7654321;
    data[spike_pos_idx].y = 42.5;
    data[spike_neg_idx].y = -38.5;

    std::vector<PlotPoint2D> out;
    auto t0 = std::chrono::high_resolution_clock::now();
    MinMaxDecimator::decimate(data, 0.0, 10.0, 1000, out);
    auto elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - t0
    ).count();

    CHECK(out.size() <= 2002);
    CHECK(out.size() >= 1000);
    // Should decimate 10M points well under typical Debug/Release interactive limits
    CHECK(elapsed_ms < 500.0);

    bool found_pos_spike = false;
    bool found_neg_spike = false;
    for (size_t i = 0; i < out.size(); ++i) {
        if (is_approx(out[i].y, 42.5, 1e-6)) found_pos_spike = true;
        if (is_approx(out[i].y, -38.5, 1e-6)) found_neg_spike = true;
        if (i > 0) {
            CHECK(out[i].x >= out[i - 1].x);
        }
    }
    CHECK(found_pos_spike);
    CHECK(found_neg_spike);
}

TEST_CASE("UI Plotting: Axis Auto-Range, Ticks, Zoom/Pan, and Nearest-X Probe") {
    std::vector<PlotPoint2D> pts = {
        {0.0, -2.0}, {1.0, 0.0}, {2.0, 4.0}, {3.0, 2.0}, {4.0, 6.0}
    };

    auto range = PlotAxisCalculator::compute_auto_range(pts, 0.1);
    CHECK(is_approx(range.x_min, 0.0));
    CHECK(is_approx(range.x_max, 4.0));
    CHECK(range.y_min < -2.0);
    CHECK(range.y_max > 6.0);

    auto ticks = PlotAxisCalculator::generate_ticks(0.0, 10.0, 6);
    REQUIRE(ticks.size() >= 3);
    CHECK(ticks.front().normalized_pos >= 0.0);
    CHECK(ticks.back().normalized_pos <= 1.0);

    // Zoom at center (0.5, 0.5) by factor 0.5 -> width & height halve around center
    auto zoomed = PlotAxisCalculator::zoom_around(PlotRange{0.0, 10.0, -5.0, 5.0}, 0.5, 0.5, 0.5);
    CHECK(is_approx(zoomed.x_min, 2.5));
    CHECK(is_approx(zoomed.x_max, 7.5));
    CHECK(is_approx(zoomed.y_min, -2.5));
    CHECK(is_approx(zoomed.y_max, 2.5));

    // Nearest X probe
    PlotPoint2D probed;
    REQUIRE(PlotAxisCalculator::find_nearest_x(pts, 2.1, probed));
    CHECK(is_approx(probed.x, 2.0));
    CHECK(is_approx(probed.y, 4.0));
}

// =============================================================================
// Task UI-03-002-02: Colormap Tables & Scalar Field Probes
// =============================================================================

TEST_CASE("UI Plotting: Matplotlib Viridis, Plasma, Turbo, Jet Interpolation Accuracy") {
    // Viridis t=0.0 -> (68, 1, 84)
    auto v0 = ColormapTables::sample_rgb8(ColormapType::Viridis, 0.0);
    CHECK(std::abs(int(v0.r) - 68) <= 2);
    CHECK(std::abs(int(v0.g) - 1) <= 2);
    CHECK(std::abs(int(v0.b) - 84) <= 2);

    // Viridis t=0.5 -> (33, 145, 140)
    auto v5 = ColormapTables::sample_rgb8(ColormapType::Viridis, 0.5);
    CHECK(std::abs(int(v5.r) - 33) <= 2);
    CHECK(std::abs(int(v5.g) - 145) <= 2);
    CHECK(std::abs(int(v5.b) - 140) <= 2);

    // Viridis t=1.0 -> (253, 231, 37)
    auto v1 = ColormapTables::sample_rgb8(ColormapType::Viridis, 1.0);
    CHECK(std::abs(int(v1.r) - 253) <= 2);
    CHECK(std::abs(int(v1.g) - 231) <= 2);
    CHECK(std::abs(int(v1.b) - 37) <= 2);

    // Turbo midpoint t=0.5 -> greenish yellow (~164, 252, 60)
    auto t5 = ColormapTables::sample_rgb8(ColormapType::Turbo, 0.5);
    CHECK(t5.g > 240);

    // 256-entry LUT check
    auto lut = ColormapTables::generate_lut256(ColormapType::Plasma);
    CHECK(lut[0].b > lut[0].r);       // Deep indigo start
    CHECK(lut[255].r > lut[255].b);   // Bright yellow end

    // Scalar probe mapping
    auto probe = ColormapTables::probe_scalar(ColormapType::Viridis, 350.0, 300.0, 400.0, "K");
    CHECK(is_approx(probe.normalized, 0.5));
    CHECK(probe.formatted_label.find("350") != std::string::npos);
    CHECK(probe.formatted_label.find("K") != std::string::npos);
}

// =============================================================================
// Task UI-03-002-03: Oscilloscope Trigger & Logic Analyzer Decoder
// =============================================================================

TEST_CASE("UI Plotting: Oscilloscope Rising/Falling Triggers & Signal Measurements") {
    // 500 Hz, 3.3V amplitude sine wave sampled at 50 kHz over 10 ms
    std::vector<PlotPoint2D> sine_wave;
    const double fs = 50000.0;
    const double f_sig = 500.0;
    for (int i = 0; i < 500; ++i) {
        double t = static_cast<double>(i) / fs;
        double v = 3.3 * std::sin(2.0 * 3.14159265358979323846 * f_sig * t);
        sine_wave.push_back({t, v});
    }

    // 1. Signal metrics
    auto metrics = OscilloscopeAnalyzer::measure_signal(sine_wave);
    CHECK(is_approx(metrics.v_pp, 6.6, 0.05));
    CHECK(is_approx(metrics.v_rms, 3.3 / std::sqrt(2.0), 0.05));
    CHECK(is_approx(metrics.frequency_hz, 500.0, 2.0));

    // 2. Rising trigger at 1.65 V (sin(pi/6) = 0.5 -> first crossing at t = 1/(6*500) = 0.0003333s or next cycle 0.002333s)
    TriggerConfig trig_rise{TriggerEdge::Rising, 1.65, 0.05, 0, 0.25};
    auto r_res = OscilloscopeAnalyzer::find_trigger(sine_wave, trig_rise, 0);
    REQUIRE(r_res.triggered);
    CHECK(r_res.detected_edge == TriggerEdge::Rising);
    // First sample at i=0 is 0.0 <= 1.60 -> armed! First crossing occurs around t = 0.0001667s (sin(2*pi*500*t) = 0.5 -> 2*500*t = 1/6 rad? Wait: 2*pi*500*t = pi/6 => t = 1/6000 = 0.00016667s!)
    CHECK(is_approx(r_res.trigger_time, 1.0 / 6000.0, 2e-5));

    // 3. Falling trigger on Square Pulse train
    std::vector<PlotPoint2D> pulse = {
        {0.0, 0.0}, {1.0, 0.0}, {1.1, 5.0}, {2.0, 5.0}, {2.1, 0.0}, {3.0, 0.0}
    };
    TriggerConfig trig_fall{TriggerEdge::Falling, 2.5, 0.1, 0, 0.25};
    auto f_res = OscilloscopeAnalyzer::find_trigger(pulse, trig_fall, 0);
    REQUIRE(f_res.triggered);
    CHECK(f_res.detected_edge == TriggerEdge::Falling);
    CHECK(is_approx(f_res.trigger_time, 2.05, 1e-4));
}

TEST_CASE("UI Plotting: Logic Analyzer 4-State Parallel Bus Hex/Binary Decoding") {
    // 4-bit bus across indices {0, 1, 2, 3} with LSB at 0
    std::vector<LogicSample> samples = {
        {0.0, {LogicLevel::Low,  LogicLevel::High, LogicLevel::Low,  LogicLevel::High}}, // 0b1010 = 0xA
        {1.0, {LogicLevel::Low,  LogicLevel::High, LogicLevel::Low,  LogicLevel::High}}, // Hold same 0xA
        {2.0, {LogicLevel::High, LogicLevel::High, LogicLevel::High, LogicLevel::High}}, // 0b1111 = 0xF
        {3.0, {LogicLevel::High, LogicLevel::Low,  LogicLevel::Low,  LogicLevel::Low}},  // 0b0001 = 0x1
        {4.0, {LogicLevel::Unknown, LogicLevel::Low, LogicLevel::High, LogicLevel::Low}}, // Contains X -> "XX"
        {5.0, {LogicLevel::HighZ, LogicLevel::HighZ, LogicLevel::HighZ, LogicLevel::HighZ}} // Contains Z -> "ZZ"
    };

    auto words = LogicAnalyzerDecoder::decode_parallel_bus(samples, {0, 1, 2, 3});
    REQUIRE(words.size() == 5); // First two merged into one segment [0.0, 2.0]

    CHECK(words[0].value == 0xA);
    CHECK(words[0].hex_str == "0xA");
    CHECK(words[0].bin_str == "0b1010");
    CHECK(is_approx(words[0].start_time, 0.0));
    CHECK(is_approx(words[0].end_time, 2.0));

    CHECK(words[1].value == 0xF);
    CHECK(words[1].hex_str == "0xF");
    CHECK(words[1].bin_str == "0b1111");

    CHECK(words[2].value == 0x1);
    CHECK(words[2].hex_str == "0x1");
    CHECK(words[2].bin_str == "0b0001");

    CHECK(words[3].has_unknown);
    CHECK(words[3].hex_str == "XX");

    CHECK(words[4].has_highz);
    CHECK(words[4].hex_str == "ZZ");
}
