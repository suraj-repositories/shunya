#include "shunya/perf/hot_loop_profiler.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

namespace shunya::perf {

HotLoopBenchmarkResult HotLoopProfiler::profile_material_mixing(
    const mat::MixtureExperiment& experiment,
    size_t iterations) {
    HotLoopBenchmarkResult res;
    res.benchmark_name = "MaterialMixingLab::evaluate";
    res.iterations = std::max<size_t>(1, iterations);

    // Warm-up iteration
    (void)mat::MaterialMixingLab::evaluate(experiment);

    std::vector<double> durations_us;
    durations_us.reserve(res.iterations);

    for (size_t i = 0; i < res.iterations; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        auto eval_res = mat::MaterialMixingLab::evaluate(experiment);
        const auto t1 = std::chrono::steady_clock::now();
        (void)eval_res;

        const double dt_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        durations_us.push_back(dt_us);
    }

    double sum_us = std::accumulate(durations_us.begin(), durations_us.end(), 0.0);
    res.total_time_ms = sum_us / 1000.0;
    res.mean_time_us = sum_us / static_cast<double>(res.iterations);
    res.min_time_us = *std::min_element(durations_us.begin(), durations_us.end());
    res.max_time_us = *std::max_element(durations_us.begin(), durations_us.end());
    res.meets_sub_millisecond_target = (res.mean_time_us < 1000.0);

    return res;
}

HotLoopBenchmarkResult HotLoopProfiler::profile_circuit_dc_solve(
    const core::domain::CircuitNetlist& netlist,
    size_t iterations) {
    HotLoopBenchmarkResult res;
    res.benchmark_name = "SpiceSimulationEngine::solve_dc";
    res.iterations = std::max<size_t>(1, iterations);

    // Warm-up iteration
    (void)pcb::SpiceSimulationEngine::solve_dc(netlist);

    std::vector<double> durations_us;
    durations_us.reserve(res.iterations);

    for (size_t i = 0; i < res.iterations; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        auto solve_res = pcb::SpiceSimulationEngine::solve_dc(netlist);
        const auto t1 = std::chrono::steady_clock::now();
        (void)solve_res;

        const double dt_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        durations_us.push_back(dt_us);
    }

    double sum_us = std::accumulate(durations_us.begin(), durations_us.end(), 0.0);
    res.total_time_ms = sum_us / 1000.0;
    res.mean_time_us = sum_us / static_cast<double>(res.iterations);
    res.min_time_us = *std::min_element(durations_us.begin(), durations_us.end());
    res.max_time_us = *std::max_element(durations_us.begin(), durations_us.end());
    res.meets_sub_millisecond_target = (res.mean_time_us < 1000.0);

    return res;
}

bool HotLoopProfiler::verify_sub_millisecond_guarantees(
    double* out_mixing_mean_us,
    double* out_circuit_mean_us) {
    // 1. Setup standard alloy test mixture (Cu-Zn brass)
    mat::MixtureExperiment mix_exp;
    mix_exp.name = "Cu-Zn Brass Profiling Sample";
    mix_exp.scenario = mat::FormationScenario::Alloy;
    mix_exp.inputs.push_back({"Cu", mat::QuantityInputBasis::MassFractionPct, 70.0, 1e-3, false});
    mix_exp.inputs.push_back({"Zn", mat::QuantityInputBasis::MassFractionPct, 30.0, 1e-3, false});

    auto mix_res = profile_material_mixing(mix_exp, 100);
    if (out_mixing_mean_us) *out_mixing_mean_us = mix_res.mean_time_us;

    // 2. Setup standard 5-node resistor bridge circuit
    core::domain::CircuitNetlist netlist;
    using T = core::domain::ElectricalComponentType;

    core::domain::ElectricalComponent vs;
    vs.id = "V1";
    vs.type = T::VoltageSourceDC;
    vs.value_primary = 12.0;
    vs.pins = {core::domain::CircuitPin::make("p", "n1"), core::domain::CircuitPin::make("n", "0")};
    netlist.add_component(vs);

    core::domain::ElectricalComponent r1;
    r1.id = "R1";
    r1.type = T::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {core::domain::CircuitPin::make("1", "n1"), core::domain::CircuitPin::make("2", "n2")};
    netlist.add_component(r1);

    core::domain::ElectricalComponent r2;
    r2.id = "R2";
    r2.type = T::Resistor;
    r2.value_primary = 2200.0;
    r2.pins = {core::domain::CircuitPin::make("1", "n2"), core::domain::CircuitPin::make("2", "0")};
    netlist.add_component(r2);

    core::domain::ElectricalComponent r3;
    r3.id = "R3";
    r3.type = T::Resistor;
    r3.value_primary = 4700.0;
    r3.pins = {core::domain::CircuitPin::make("1", "n1"), core::domain::CircuitPin::make("2", "n3")};
    netlist.add_component(r3);

    core::domain::ElectricalComponent r4;
    r4.id = "R4";
    r4.type = T::Resistor;
    r4.value_primary = 10000.0;
    r4.pins = {core::domain::CircuitPin::make("1", "n3"), core::domain::CircuitPin::make("2", "0")};
    netlist.add_component(r4);

    auto cir_res = profile_circuit_dc_solve(netlist, 100);
    if (out_circuit_mean_us) *out_circuit_mean_us = cir_res.mean_time_us;

    return mix_res.meets_sub_millisecond_target && cir_res.meets_sub_millisecond_target;
}

} // namespace shunya::perf
