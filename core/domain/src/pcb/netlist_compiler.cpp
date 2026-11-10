#include "shunya/pcb/netlist_compiler.hpp"

#include <algorithm>
#include <unordered_map>

namespace shunya::pcb {

namespace {

std::string normalize_net_name(std::string_view name) {
    if (name == "GND" || name == "gnd" || name == "VSS" || name == "0") {
        return "0";
    }
    return std::string(name);
}

} // namespace

core::base::Result<CompiledSpiceBoardResult> NetlistCompiler::compile(
    const SchematicDocument& schematic,
    const std::vector<ComponentSpiceSpec>& specs,
    const std::vector<PcbTrackSegment>& tracks,
    const std::vector<PcbVia>& vias,
    const BoardStackupManager* stackup,
    bool include_trace_parasitics) {

    const CompiledNetlist flat = schematic.compile_netlist();

    // Build lookup: (component_ref, pin_number) -> net_name
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> comp_pin_net;
    for (const auto& net : flat.nets) {
        const std::string nname = normalize_net_name(net.name);
        for (const auto& pin : net.pins) {
            comp_pin_net[pin.component_ref][pin.pin_number] = nname;
        }
    }

    CompiledSpiceBoardResult out;
    if (!tracks.empty() || !vias.empty()) {
        BoardStackupManager default_stack = BoardStackupManager::standard_4layer_fr4();
        const BoardStackupManager& used_stack = stackup ? *stackup : default_stack;
        out.net_parasitics = ParasiticExtractor::extract_board_nets(tracks, vias, used_stack);
    }

    // Track which nets have trace parasitics significant enough to insert a series-R shunt-C lumped cell
    std::unordered_map<std::string, int> net_terminal_counter;
    std::unordered_map<std::string, std::string> parasitic_end_node;

    if (include_trace_parasitics) {
        for (const auto& [raw_net, summary] : out.net_parasitics) {
            const std::string net = normalize_net_name(raw_net);
            if (net == "0") continue;
            if (summary.total_resistance_ohm > 1e-6 || summary.total_capacitance_pf > 1e-4) {
                parasitic_end_node[net] = net + "_pcb_load";
            }
        }
    }

    for (const auto& spec : specs) {
        core::domain::ElectricalComponent ec;
        ec.id = spec.ref_des;
        ec.type = spec.type;
        ec.value_primary = spec.value_primary;
        ec.value_secondary = spec.value_secondary;
        ec.value_tertiary = spec.value_tertiary;
        ec.waveform = spec.waveform;
        ec.is_saturation_current = spec.is_saturation_current;
        ec.ideality_factor = spec.ideality_factor;
        ec.mos_vth = spec.mos_vth;
        ec.mos_kp = spec.mos_kp;

        const auto comp_it = comp_pin_net.find(spec.ref_des);
        if (comp_it == comp_pin_net.end()) {
            return core::base::Result<CompiledSpiceBoardResult>::err(
                core::base::Error{10, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "Component '" + spec.ref_des + "' has no compiled schematic connections", ""});
        }

        for (size_t i = 0; i < spec.pin_order.size(); ++i) {
            const std::string& pnum = spec.pin_order[i];
            auto pit = comp_it->second.find(pnum);
            std::string net_name = (pit != comp_it->second.end()) ? pit->second : "0";

            // For nets split by trace parasitics, first connected instance keeps `<net>` and later loads see `<net>_pcb_load`
            if (auto split_it = parasitic_end_node.find(net_name); split_it != parasitic_end_node.end()) {
                const int occurrence = net_terminal_counter[net_name]++;
                if (occurrence > 0) {
                    net_name = split_it->second;
                }
            }

            ec.pins.push_back(core::domain::CircuitPin::make(
                spec.ref_des + "_" + pnum, net_name,
                core::domain::ElectricalPinPolarity::PassiveBiDirectional,
                spec.ref_des, pnum));
        }
        out.circuit.add_component(std::move(ec));
    }

    // Inject series trace resistance + optional inductance & shunt capacitance for split nets
    for (const auto& [net, load_node] : parasitic_end_node) {
        auto it = out.net_parasitics.find(net);
        if (it == out.net_parasitics.end()) continue;
        const auto& sum = it->second;

        const bool has_split_loads = (net_terminal_counter[net] >= 2);
        const std::string mid_node = (sum.total_inductance_nh > 1e-3 && has_split_loads)
            ? (net + "_pcb_mid")
            : load_node;

        if (has_split_loads) {
            core::domain::ElectricalComponent r_trace;
            r_trace.id = "R_PAR_" + net;
            r_trace.type = core::domain::ElectricalComponentType::Resistor;
            r_trace.value_primary = std::max(1e-6, sum.total_resistance_ohm);
            r_trace.pins.push_back(core::domain::CircuitPin::make(r_trace.id + "_1", net));
            r_trace.pins.push_back(core::domain::CircuitPin::make(r_trace.id + "_2", mid_node));
            out.circuit.add_component(std::move(r_trace));

            if (mid_node != load_node) {
                core::domain::ElectricalComponent l_trace;
                l_trace.id = "L_PAR_" + net;
                l_trace.type = core::domain::ElectricalComponentType::Inductor;
                l_trace.value_primary = sum.total_inductance_nh * 1e-9;
                l_trace.pins.push_back(core::domain::CircuitPin::make(l_trace.id + "_1", mid_node));
                l_trace.pins.push_back(core::domain::CircuitPin::make(l_trace.id + "_2", load_node));
                out.circuit.add_component(std::move(l_trace));
            }
        }

        if (sum.total_capacitance_pf > 1e-4) {
            core::domain::ElectricalComponent c_trace;
            c_trace.id = "C_PAR_" + net;
            c_trace.type = core::domain::ElectricalComponentType::Capacitor;
            c_trace.value_primary = sum.total_capacitance_pf * 1e-12;
            c_trace.pins.push_back(core::domain::CircuitPin::make(c_trace.id + "_1", has_split_loads ? load_node : net));
            c_trace.pins.push_back(core::domain::CircuitPin::make(c_trace.id + "_2", "0"));
            out.circuit.add_component(std::move(c_trace));
        }
    }

    auto topo = out.circuit.build_node_topology();
    if (!topo.is_ok()) {
        return core::base::Result<CompiledSpiceBoardResult>::err(topo.error());
    }

    out.spice_deck = out.circuit.export_spice_netlist("Shunya PCB Netlist");
    return core::base::Result<CompiledSpiceBoardResult>::ok(std::move(out));
}

} // namespace shunya::pcb
