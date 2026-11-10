#include "shunya/matdb/decay_chain_builder.hpp"

#include <cmath>
#include <unordered_map>

namespace shunya::matdb {

namespace {

core::base::Error make_dc_error(std::string msg) {
    return core::base::Error{
        .code = 4140,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

std::pair<int, int> expected_deltas(DecayMode mode) noexcept {
    switch (mode) {
    case DecayMode::Alpha:              return {-2, -4};
    case DecayMode::BetaMinus:          return {+1,  0};
    case DecayMode::BetaPlus:           return {-1,  0};
    case DecayMode::ElectronCapture:    return {-1,  0};
    case DecayMode::IsomericTransition: return { 0,  0};
    case DecayMode::ProtonEmission:     return {-1, -1};
    case DecayMode::NeutronEmission:    return { 0, -1};
    case DecayMode::Stable:
    case DecayMode::SpontaneousFission: return { 0,  0};
    }
    return {0, 0};
}

void add_step(DecayChainGraph& g, size_t from_idx, size_t to_idx, DecayMode mode, double ratio = 1.0) {
    auto [dz, da] = expected_deltas(mode);
    g.edges.push_back(DecayTransitionEdge{from_idx, to_idx, mode, ratio, dz, da});
}

} // namespace

size_t DecayChainGraph::terminal_stable_count() const noexcept {
    size_t count = 0;
    for (const auto& n : nodes) {
        if (n.is_stable) ++count;
    }
    return count;
}

DecayChainGraph DecayChainGraphBuilder::build_uranium_238_series() {
    DecayChainGraph g;
    g.series_name = "Uranium-238 (4n+2) Series";

    // Dominant 14-step decay path from U-238 -> Pb-206
    g.nodes = {
        {92, 238, "U-238",   half_life_to_seconds(4.468e9, TimeUnit::Years),   false}, // 0
        {90, 234, "Th-234",  half_life_to_seconds(24.10,   TimeUnit::Days),    false}, // 1
        {91, 234, "Pa-234m", half_life_to_seconds(1.159,   TimeUnit::Minutes), false}, // 2
        {92, 234, "U-234",   half_life_to_seconds(2.455e5, TimeUnit::Years),   false}, // 3
        {90, 230, "Th-230",  half_life_to_seconds(7.538e4, TimeUnit::Years),   false}, // 4
        {88, 226, "Ra-226",  half_life_to_seconds(1600.0,  TimeUnit::Years),   false}, // 5
        {86, 222, "Rn-222",  half_life_to_seconds(3.8235,  TimeUnit::Days),    false}, // 6
        {84, 218, "Po-218",  half_life_to_seconds(3.10,    TimeUnit::Minutes), false}, // 7
        {82, 214, "Pb-214",  half_life_to_seconds(26.8,    TimeUnit::Minutes), false}, // 8
        {83, 214, "Bi-214",  half_life_to_seconds(19.9,    TimeUnit::Minutes), false}, // 9
        {84, 214, "Po-214",  half_life_to_seconds(164.3,   TimeUnit::Microseconds), false}, // 10
        {82, 210, "Pb-210",  half_life_to_seconds(22.20,   TimeUnit::Years),   false}, // 11
        {83, 210, "Bi-210",  half_life_to_seconds(5.012,   TimeUnit::Days),    false}, // 12
        {84, 210, "Po-210",  half_life_to_seconds(138.376, TimeUnit::Days),    false}, // 13
        {82, 206, "Pb-206",  0.0,                                               true}   // 14
    };

    const DecayMode modes[14] = {
        DecayMode::Alpha,     // 0: U-238 -> Th-234
        DecayMode::BetaMinus, // 1: Th-234 -> Pa-234m
        DecayMode::BetaMinus, // 2: Pa-234m -> U-234
        DecayMode::Alpha,     // 3: U-234 -> Th-230
        DecayMode::Alpha,     // 4: Th-230 -> Ra-226
        DecayMode::Alpha,     // 5: Ra-226 -> Rn-222
        DecayMode::Alpha,     // 6: Rn-222 -> Po-218
        DecayMode::Alpha,     // 7: Po-218 -> Pb-214
        DecayMode::BetaMinus, // 8: Pb-214 -> Bi-214
        DecayMode::BetaMinus, // 9: Bi-214 -> Po-214
        DecayMode::Alpha,     // 10: Po-214 -> Pb-210
        DecayMode::BetaMinus, // 11: Pb-210 -> Bi-210
        DecayMode::BetaMinus, // 12: Bi-210 -> Po-210
        DecayMode::Alpha      // 13: Po-210 -> Pb-206
    };
    for (size_t i = 0; i < 14; ++i) {
        add_step(g, i, i + 1, modes[i], 1.0);
    }
    return g;
}

DecayChainGraph DecayChainGraphBuilder::build_thorium_232_series() {
    DecayChainGraph g;
    g.series_name = "Thorium-232 (4n) Series";
    g.nodes = {
        {90, 232, "Th-232", half_life_to_seconds(1.405e10, TimeUnit::Years), false},
        {88, 228, "Ra-228", half_life_to_seconds(5.75,     TimeUnit::Years), false},
        {89, 228, "Ac-228", half_life_to_seconds(6.15,     TimeUnit::Hours), false},
        {90, 228, "Th-228", half_life_to_seconds(1.912,    TimeUnit::Years), false},
        {88, 224, "Ra-224", half_life_to_seconds(3.66,     TimeUnit::Days),  false},
        {86, 220, "Rn-220", half_life_to_seconds(55.6,     TimeUnit::Seconds), false},
        {84, 216, "Po-216", half_life_to_seconds(0.145,    TimeUnit::Seconds), false},
        {82, 212, "Pb-212", half_life_to_seconds(10.64,    TimeUnit::Hours), false},
        {83, 212, "Bi-212", half_life_to_seconds(60.55,    TimeUnit::Minutes), false},
        {84, 212, "Po-212", half_life_to_seconds(299.0,    TimeUnit::Nanoseconds), false},
        {81, 208, "Tl-208", half_life_to_seconds(3.053,    TimeUnit::Minutes), false},
        {82, 208, "Pb-208", 0.0, true}
    };
    add_step(g, 0,  1,  DecayMode::Alpha,     1.0);
    add_step(g, 1,  2,  DecayMode::BetaMinus, 1.0);
    add_step(g, 2,  3,  DecayMode::BetaMinus, 1.0);
    add_step(g, 3,  4,  DecayMode::Alpha,     1.0);
    add_step(g, 4,  5,  DecayMode::Alpha,     1.0);
    add_step(g, 5,  6,  DecayMode::Alpha,     1.0);
    add_step(g, 6,  7,  DecayMode::Alpha,     1.0);
    add_step(g, 7,  8,  DecayMode::BetaMinus, 1.0);
    // Bi-212 branches: 64.06% beta- -> Po-212, 35.94% alpha -> Tl-208
    add_step(g, 8,  9,  DecayMode::BetaMinus, 0.6406);
    add_step(g, 8,  10, DecayMode::Alpha,     0.3594);
    add_step(g, 9,  11, DecayMode::Alpha,     1.0);
    add_step(g, 10, 11, DecayMode::BetaMinus, 1.0);
    return g;
}

DecayChainGraph DecayChainGraphBuilder::build_carbon_14_decay() {
    DecayChainGraph g;
    g.series_name = "Carbon-14 Beta Decay";
    g.nodes = {
        {6, 14, "C-14", half_life_to_seconds(5730.0, TimeUnit::Years), false},
        {7, 14, "N-14", 0.0, true}
    };
    add_step(g, 0, 1, DecayMode::BetaMinus, 1.0);
    return g;
}

core::base::Result<DecayChainGraph> DecayChainGraphBuilder::build_series(std::string_view series_id) {
    if (series_id == "U-238")  return build_uranium_238_series();
    if (series_id == "Th-232") return build_thorium_232_series();
    if (series_id == "C-14")   return build_carbon_14_decay();
    return make_dc_error("Unknown decay chain series: " + std::string(series_id));
}

core::base::ValidationReport DecayChainGraphBuilder::audit_conservation(const DecayChainGraph& graph) {
    core::base::ValidationReport report;
    std::unordered_map<size_t, double> out_branch_sums;

    for (const auto& edge : graph.edges) {
        if (edge.from_index >= graph.nodes.size() || edge.to_index >= graph.nodes.size()) {
            report.add_error(graph.series_name, "Transition edge index out of bounds");
            continue;
        }
        const auto& src = graph.nodes[edge.from_index];
        const auto& dst = graph.nodes[edge.to_index];
        auto [exp_dz, exp_da] = expected_deltas(edge.mode);
        if (edge.delta_z != exp_dz || edge.delta_a != exp_da) {
            report.add_error(src.label, "Edge deltas contradict DecayMode law");
        }
        if (src.atomic_number + edge.delta_z != dst.atomic_number) {
            report.add_error(src.label + "->" + dst.label, "Charge number Z not conserved");
        }
        if (src.mass_number + edge.delta_a != dst.mass_number) {
            report.add_error(src.label + "->" + dst.label, "Nucleon number A not conserved");
        }
        out_branch_sums[edge.from_index] += edge.branching_ratio;
    }

    for (const auto& [idx, sum] : out_branch_sums) {
        if (std::abs(sum - 1.0) > 1e-4) {
            report.add_error(graph.nodes[idx].label, "Outgoing branching ratios do not sum to 1.0");
        }
    }
    return report;
}

} // namespace shunya::matdb
