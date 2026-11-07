#include "tests/test_framework.hpp"

#include "shunya/core/base/ring_buffer.hpp"
#include "shunya/core/domain/observability.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::base;
using namespace shunya::core::domain;

TEST_CASE("OBS-01: Measurement Probes and Types") {
    CHECK(measurement_type_to_string(MeasurementType::Scalar) == "Scalar");
    CHECK(measurement_type_to_string(MeasurementType::Vector) == "Vector");
    CHECK(measurement_type_to_string(MeasurementType::Tensor) == "Tensor");
    CHECK(measurement_type_to_string(MeasurementType::SpatialField) == "SpatialField");
    CHECK(measurement_type_to_string(MeasurementType::DiscreteEvent) == "DiscreteEvent");

    CHECK(measurement_type_from_string("Scalar").value() == MeasurementType::Scalar);
    CHECK(measurement_type_from_string("SpatialField").value() == MeasurementType::SpatialField);
    CHECK_FALSE(measurement_type_from_string("Invalid").has_value());

    ProbeDefinition probe;
    probe.name = "Voltage_V1";
    probe.target_entity_id = Uuid::generate_random();
    probe.target_variable_or_port = "out_v";
    probe.type = MeasurementType::Scalar;
    probe.unit_symbol = "V";
    probe.sampling_rate_hz = 1000.0;

    std::string json = probe.to_json();
    auto restored_res = ProbeDefinition::from_json(json);
    REQUIRE(restored_res.is_ok());

    const auto& restored = restored_res.value();
    CHECK(restored.name == "Voltage_V1");
    CHECK(restored.target_variable_or_port == "out_v");
    CHECK(restored.type == MeasurementType::Scalar);
    CHECK(restored.unit_symbol == "V");
    CHECK(restored.sampling_rate_hz == 1000.0);
}

TEST_CASE("OBS-02: Live Dashboards and Widget Management") {
    Dashboard dash("PowerSubsystemMonitoring");

    DashboardWidget w1;
    w1.title = "Bus Voltage Live";
    w1.type = WidgetType::TimeSeriesChart;
    w1.min_display_range = 0.0;
    w1.max_display_range = 48.0;

    DashboardWidget w2;
    w2.title = "Core Temperature";
    w2.type = WidgetType::Gauge;
    w2.min_display_range = 20.0;
    w2.max_display_range = 100.0;

    dash.add_widget(w1);
    dash.add_widget(w2);
    CHECK(dash.widget_count() == 2);

    std::string json = dash.to_json();
    auto restored_res = Dashboard::from_json(json);
    REQUIRE(restored_res.is_ok());

    const auto& restored = restored_res.value();
    CHECK(restored.name() == "PowerSubsystemMonitoring");
    CHECK(restored.widget_count() == 2);
    CHECK(restored.widgets()[0].title == "Bus Voltage Live");
    CHECK(restored.widgets()[1].title == "Core Temperature");

    // Remove widget
    CHECK(dash.remove_widget(w1.widget_id));
    CHECK(dash.widget_count() == 1);
}

TEST_CASE("OBS-03 & IMP-03: Lock-Free Ring Buffer and Time-Series Store") {
    // 1. Lock-free SPSC Ring Buffer verification
    RingBuffer<double> rb(16);
    CHECK(rb.capacity() == 16);
    CHECK(rb.empty());
    CHECK_FALSE(rb.full());

    for (int i = 0; i < 16; ++i) {
        CHECK(rb.push(static_cast<double>(i)));
    }
    CHECK(rb.full());
    CHECK_FALSE(rb.push(999.0)); // Overfill rejected

    for (int i = 0; i < 16; ++i) {
        auto val = rb.pop();
        REQUIRE(val.has_value());
        CHECK(std::abs(*val - static_cast<double>(i)) < 1e-9);
    }
    CHECK(rb.empty());

    // 2. Time-Series Store and Decimation
    TimeSeriesStore store(10000);
    // Generate sine wave: t in [0, 10], 1000 points
    for (int i = 0; i < 1000; ++i) {
        const double t = static_cast<double>(i) * 0.01;
        const double v = std::sin(t);
        store.push_live(t, v);
    }

    CHECK(store.flush_live_buffer() == 1000);
    CHECK(store.size() == 1000);
    CHECK(std::abs(store.min_value() - (-1.0)) < 0.05);
    CHECK(std::abs(store.max_value() - 1.0) < 0.05);

    // Decimation query with MinMax
    auto decimated = store.query_range(0.0, 10.0, 100, DecimationPolicy::MinMax);
    CHECK(decimated.size() <= 100);
    CHECK(decimated.size() > 50);

    // Decimation query with Average
    auto avg_decimated = store.query_range(0.0, 10.0, 50, DecimationPolicy::Average);
    CHECK(avg_decimated.size() == 50);
}

TEST_CASE("OBS-04: Spatial Field Data and Slice Planes") {
    SpatialFieldData field;
    field.field_name = "TemperatureField3D";
    field.timestamp = 1.5;

    // Create a 2x2x2 grid of points
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                field.points.push_back({static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)});
                field.scalar_values.push_back(100.0 + 10.0 * x + 20.0 * y + 50.0 * z);
            }
        }
    }

    CHECK(field.min_scalar() == 100.0);
    CHECK(field.max_scalar() == 180.0);

    // Extract slice at z = 1.0
    auto slice = field.slice_z(1.0);
    CHECK(slice.points.size() == 4);
    CHECK(slice.min_scalar() == 150.0); // 100 + 50
    CHECK(slice.max_scalar() == 180.0);
}

TEST_CASE("OBS-05: Instrument Models: Oscilloscope, Multimeter, Thermometer") {
    // 1. Oscilloscope trigger and trace capture
    VirtualOscilloscope scope;
    scope.set_time_base(0.001); // 1 ms/div -> 10 ms full screen
    scope.set_trigger(VirtualOscilloscope::TriggerMode::Normal, 0.5);

    CHECK_FALSE(scope.is_triggered());
    scope.feed_sample(0.000, 0.0);
    scope.feed_sample(0.001, 0.2);
    CHECK_FALSE(scope.is_triggered());

    // Crossing trigger level 0.5
    scope.feed_sample(0.002, 0.6);
    CHECK(scope.is_triggered());
    CHECK(scope.trace_buffer().size() == 1);

    // 2. Multimeter DC and AC RMS
    VirtualMultimeter dmm;
    dmm.set_mode(VirtualMultimeter::Mode::VoltageDC);
    dmm.sample(5.0);
    dmm.sample(5.2);
    dmm.sample(4.8);
    CHECK(std::abs(dmm.current_reading() - 5.0) < 1e-6);

    // AC mode RMS: for sine wave with amplitude A = 10, RMS = 10 / sqrt(2) ~ 7.071
    dmm.reset();
    dmm.set_mode(VirtualMultimeter::Mode::VoltageAC);
    for (int i = 0; i < 1000; ++i) {
        const double phi = static_cast<double>(i) * 2.0 * 3.141592653589793 / 1000.0;
        dmm.sample(10.0 * std::sin(phi));
    }
    CHECK(std::abs(dmm.current_reading() - (10.0 / std::sqrt(2.0))) < 0.05);

    // 3. Thermometer scales
    VirtualThermometer thermo;
    thermo.set_temperature_kelvin(300.0);
    CHECK(std::abs(thermo.read(VirtualThermometer::Scale::Kelvin) - 300.0) < 1e-9);
    CHECK(std::abs(thermo.read(VirtualThermometer::Scale::Celsius) - 26.85) < 1e-6);
    CHECK(std::abs(thermo.read(VirtualThermometer::Scale::Fahrenheit) - 80.33) < 0.05);
}

TEST_CASE("OBS-06: Units and Axis Formatting with Significant Digits") {
    AxisFormatting fmt;
    fmt.unit_symbol = "A";
    fmt.significant_digits = 2;
    fmt.use_si_prefixes = true;

    CHECK(fmt.format_value(0.0015).find("1.50 mA") != std::string::npos);
    CHECK(fmt.format_value(2500.0).find("2.50 kA") != std::string::npos);
    CHECK(fmt.format_value(0.000004).find("4.00 uA") != std::string::npos);

    fmt.use_si_prefixes = false;
    CHECK(fmt.format_value(12.3456).find("12 A") != std::string::npos);
}

TEST_CASE("OBS-07: Result Annotations, Notes and Bookmarks") {
    ResultAnnotation annot;
    annot.timestamp = 2.5;
    annot.text = "Phase transition point observed";
    annot.author = "SimulationObserver";

    CHECK(annot.timestamp == 2.5);
    CHECK(annot.text == "Phase transition point observed");

    ExperimentNote note;
    note.title = "Capacitor Discharge Transient";
    note.content_markdown = "### Observation\nTime constant $\tau = RC$ matched analytical calculation.";
    CHECK(note.title == "Capacitor Discharge Transient");

    ResultBookmark bm;
    bm.name = "ResonancePeak";
    bm.target_time = 4.75;
    CHECK(bm.target_time == 4.75);
}

TEST_CASE("OBS-08 & OBS-09: Export Data, Sidecars and Provenance") {
    TimeSeriesStore store;
    store.append(0.0, 1.0);
    store.append(0.5, 2.0);
    store.append(1.0, 3.0);

    // CSV Export
    std::string csv = ResultExporter::export_csv(store, "current");
    CHECK(csv.find("time,current") != std::string::npos);
    CHECK(csv.find("0.5,2") != std::string::npos);

    // Result Provenance
    ResultProvenance prov;
    prov.run_id = Uuid::generate_random();
    prov.project_content_hash = "a1b2c3d4e5f6";
    prov.solver_name = "AdaptiveRK45";
    prov.engine_version = Version(1, 0, 0, "final");
    prov.timestamp_iso = "2026-11-07T12:00:00Z";
    prov.host_info = "Windows x64";

    std::string prov_json = prov.to_json();
    auto restored_prov = ResultProvenance::from_json(prov_json);
    REQUIRE(restored_prov.is_ok());
    CHECK(restored_prov.value().project_content_hash == "a1b2c3d4e5f6");
    CHECK(restored_prov.value().solver_name == "AdaptiveRK45");

    // Sidecar metadata generation
    ProbeDefinition probe;
    probe.name = "V_out";
    std::string sidecar = ResultExporter::generate_sidecar(prov, {probe});
    CHECK(sidecar.find("AdaptiveRK45") != std::string::npos);
    CHECK(sidecar.find("V_out") != std::string::npos);
}

TEST_CASE("OBS-10: Result Retention Manager and Budget Enforcement") {
    // 100 KB budget
    ResultRetentionManager manager(100 * 1024);

    auto store1 = std::make_shared<TimeSeriesStore>();
    auto store2 = std::make_shared<TimeSeriesStore>();

    // Fill store1 with 5000 samples (~80 KB)
    for (int i = 0; i < 5000; ++i) {
        store1->append(static_cast<double>(i), 1.0);
    }

    // Fill store2 with 5000 samples (~80 KB)
    for (int i = 0; i < 5000; ++i) {
        store2->append(static_cast<double>(i), 2.0);
    }

    // Register store1 with low priority (0) and store2 with higher priority (10)
    manager.register_store(Uuid::generate_random(), store1, 0);
    manager.register_store(Uuid::generate_random(), store2, 10);

    CHECK(manager.current_memory_usage() > manager.memory_budget());

    // Enforce retention: should evict store1 first due to lower priority
    size_t evicted = manager.enforce_retention();
    CHECK(evicted >= 5000);
    CHECK(store1->empty());
    CHECK_FALSE(store2->empty());
    CHECK(manager.current_memory_usage() <= manager.memory_budget());
}
