#include "shunya/cirsim/kcl_kvl_validator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace shunya::cirsim {

using namespace core::domain;

ConservationCheckResult KclKvlValidator::verify_conservation(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components,
    const std::vector<double>& solved_node_voltages,
    const std::vector<double>& solved_branch_currents,
    double current_tolerance,
    double voltage_tolerance
) {
    ConservationCheckResult result;
    result.kcl_conserved = true;
    result.kvl_conserved = true;
    result.max_kcl_residual_amperes = 0.0;
    result.max_kvl_residual_volts = 0.0;

    // Build node index to voltage mapping
    // Note: solved_node_voltages is indexed by non-ground MNA row order (sorted node index)
    std::vector<uint32_t> sorted_indices;
    for (const auto& [node_idx, info] : topology.nodes) {
        if (!info.is_ground) {
            sorted_indices.push_back(node_idx);
        }
    }
    std::sort(sorted_indices.begin(), sorted_indices.end());

    std::unordered_map<uint32_t, double> node_voltage_map;
    node_voltage_map[0] = 0.0; // Ground is always 0V
    for (size_t i = 0; i < sorted_indices.size() && i < solved_node_voltages.size(); ++i) {
        node_voltage_map[sorted_indices[i]] = solved_node_voltages[i];
    }

    // Branch current map for voltage sources
    std::unordered_map<std::string, double> vsource_currents;
    size_t branch_idx = 0;
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourceAC ||
            comp.type == ElectricalComponentType::VoltageSourcePulse) {
            if (branch_idx < solved_branch_currents.size()) {
                vsource_currents[comp.id] = solved_branch_currents[branch_idx++];
            }
        }
    }

    auto get_node_of_pin = [&](const std::string& comp_id, const std::string& pin_id) -> uint32_t {
        std::string key = comp_id + ":" + pin_id;
        auto it = topology.pin_to_node_map.find(key);
        if (it != topology.pin_to_node_map.end()) return it->second;
        return 0;
    };

    // 1. KCL Verification: Net current into each non-ground node
    std::unordered_map<uint32_t, double> net_nodal_currents;
    for (uint32_t n : sorted_indices) {
        net_nodal_currents[n] = 0.0;
    }

    for (const auto& comp : components) {
        if (comp.pins.size() < 2) continue;
        uint32_t n_p = get_node_of_pin(comp.id, comp.pins[0].pin_id);
        uint32_t n_q = get_node_of_pin(comp.id, comp.pins[1].pin_id);
        double vp = node_voltage_map[n_p];
        double vq = node_voltage_map[n_q];

        double i_comp = 0.0; // Current flowing from pin 0 to pin 1 through comp

        if (comp.type == ElectricalComponentType::Resistor) {
            double r = std::max(1e-12, comp.value_primary);
            i_comp = (vp - vq) / r;
        } else if (comp.type == ElectricalComponentType::CurrentSourceDC) {
            // Source injects current from pin 1 to pin 0, so pin 0 to pin 1 is -Is
            i_comp = -comp.value_primary;
        } else if (comp.type == ElectricalComponentType::VoltageSourceDC) {
            // Solved branch current flows into pin 0 and out pin 1
            auto it = vsource_currents.find(comp.id);
            if (it != vsource_currents.end()) {
                i_comp = it->second;
            }
        } else if (comp.type == ElectricalComponentType::Diode) {
            // Shockley model: pin 0 = anode, pin 1 = cathode
            double vd = vp - vq;
            const double is = 1e-14;
            const double vt = 0.02585;
            i_comp = is * (std::exp(std::clamp(vd / vt, -50.0, 40.0)) - 1.0);
        }

        // KCL: current leaving node n_p into comp (+i_comp)
        // current entering node n_q from comp (-i_comp)
        if (n_p != 0) net_nodal_currents[n_p] += i_comp;
        if (n_q != 0) net_nodal_currents[n_q] -= i_comp;
    }

    for (const auto& [node_idx, net_i] : net_nodal_currents) {
        double abs_i = std::abs(net_i);
        if (abs_i > result.max_kcl_residual_amperes) {
            result.max_kcl_residual_amperes = abs_i;
        }
        if (abs_i > current_tolerance) {
            result.kcl_conserved = false;
            std::ostringstream ss;
            ss << "KCL violation at Node " << node_idx
               << " (name: " << topology.nodes.at(node_idx).node_name
               << "): net current residual = " << net_i << " A (tol: " << current_tolerance << " A)";
            result.violation_reports.push_back(ss.str());
        }
    }

    // 2. KVL Verification: Verify branch voltage constraints and loop consistency
    // Check voltage sources branch voltage consistency: V(pin0) - V(pin1) == V_source
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC && comp.pins.size() >= 2) {
            uint32_t n_p = get_node_of_pin(comp.id, comp.pins[0].pin_id);
            uint32_t n_q = get_node_of_pin(comp.id, comp.pins[1].pin_id);
            double vp = node_voltage_map[n_p];
            double vq = node_voltage_map[n_q];
            double v_actual = vp - vq;
            double v_expected = comp.value_primary;
            double err = std::abs(v_actual - v_expected);
            if (err > result.max_kvl_residual_volts) {
                result.max_kvl_residual_volts = err;
            }
            if (err > voltage_tolerance) {
                result.kvl_conserved = false;
                std::ostringstream ss;
                ss << "KVL branch violation across source " << comp.id
                   << ": expected " << v_expected << " V, actual " << v_actual
                   << " V (residual: " << err << " V)";
                result.violation_reports.push_back(ss.str());
            }
        }
    }

    // Check parallel branch voltage consistency across shared node pairs
    std::unordered_map<uint64_t, std::vector<std::string>> node_pair_branches;
    for (const auto& comp : components) {
        if (comp.pins.size() >= 2) {
            uint32_t np = get_node_of_pin(comp.id, comp.pins[0].pin_id);
            uint32_t nq = get_node_of_pin(comp.id, comp.pins[1].pin_id);
            if (np > nq) std::swap(np, nq);
            uint64_t key = (static_cast<uint64_t>(np) << 32) | static_cast<uint64_t>(nq);
            node_pair_branches[key].push_back(comp.id);
        }
    }

    for (const auto& [pair_key, branch_ids] : node_pair_branches) {
        if (branch_ids.size() > 1) {
            uint32_t np = static_cast<uint32_t>(pair_key >> 32);
            uint32_t nq = static_cast<uint32_t>(pair_key & 0xFFFFFFFFULL);
            double v_expected = std::abs(node_voltage_map[np] - node_voltage_map[nq]);
            for (const auto& bid : branch_ids) {
                auto comp_it = std::find_if(components.begin(), components.end(),
                                            [&](const ElectricalComponent& c) { return c.id == bid; });
                if (comp_it != components.end() && comp_it->pins.size() >= 2) {
                    uint32_t p0 = get_node_of_pin(comp_it->id, comp_it->pins[0].pin_id);
                    uint32_t p1 = get_node_of_pin(comp_it->id, comp_it->pins[1].pin_id);
                    double v_branch = std::abs(node_voltage_map[p0] - node_voltage_map[p1]);
                    double diff = std::abs(v_branch - v_expected);
                    if (diff > result.max_kvl_residual_volts) {
                        result.max_kvl_residual_volts = diff;
                    }
                    if (diff > voltage_tolerance) {
                        result.kvl_conserved = false;
                        std::ostringstream ss;
                        ss << "KVL parallel branch violation across " << bid << ": expected " << v_expected
                           << " V, branch has " << v_branch << " V";
                        result.violation_reports.push_back(ss.str());
                    }
                }
            }
        }
    }

    return result;
}

} // namespace shunya::cirsim
