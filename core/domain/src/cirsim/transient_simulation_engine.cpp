#include "shunya/cirsim/transient_simulation_engine.hpp"
#include "shunya/cirsim/mna_system_builder.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cirsim {

using namespace core::domain;

double TransientSimulationEngine::evaluate_saturable_inductance(
    double l0, double isat, double current
) {
    if (l0 <= 0.0) l0 = 1.0e-6;
    if (isat <= 0.0) return l0;

    const double ratio = current / isat;
    return l0 / (1.0 + ratio * ratio);
}

core::base::Result<TransientSimulationResult> TransientSimulationEngine::solve(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components
) {
    TransientSimulationResult result;
    result.probes = probes_;

    // Map non-ground nodes to row indices
    std::vector<uint32_t> sorted_indices;
    for (const auto& [node_idx, info] : topology.nodes) {
        if (!info.is_ground) {
            sorted_indices.push_back(node_idx);
        }
    }
    std::sort(sorted_indices.begin(), sorted_indices.end());

    std::unordered_map<uint32_t, size_t> node_to_row;
    for (size_t i = 0; i < sorted_indices.size(); ++i) {
        node_to_row[sorted_indices[i]] = i;
        result.node_voltages[sorted_indices[i]] = {};
    }
    const size_t num_nodes = sorted_indices.size();

    // Identify sources
    std::vector<const ElectricalComponent*> v_sources;
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourceAC ||
            comp.type == ElectricalComponentType::VoltageSourcePulse) {
            v_sources.push_back(&comp);
            result.branch_currents[comp.id] = {};
        }
    }
    const size_t num_branches = v_sources.size();
    const size_t dim = num_nodes + num_branches;

    auto get_node_of_pin = [&](const std::string& comp_id, const std::string& pin_id) -> uint32_t {
        std::string key = comp_id + ":" + pin_id;
        auto it = topology.pin_to_node_map.find(key);
        return (it != topology.pin_to_node_map.end()) ? it->second : 0;
    };

    auto get_row_of_node = [&](uint32_t n) -> int {
        if (n == 0) return -1;
        auto it = node_to_row.find(n);
        return (it != node_to_row.end()) ? static_cast<int>(it->second) : -1;
    };

    // Initialize state for dynamic elements
    std::unordered_map<std::string, double> cap_v_prev;
    std::unordered_map<std::string, double> cap_v_prev2;
    std::unordered_map<std::string, double> cap_i_prev;
    std::unordered_map<std::string, double> ind_i_prev;
    std::unordered_map<std::string, double> ind_i_prev2;
    std::unordered_map<std::string, double> ind_v_prev;

    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::Capacitor) {
            cap_v_prev[comp.id] = 0.0;
            cap_v_prev2[comp.id] = 0.0;
            cap_i_prev[comp.id] = 0.0;
        } else if (comp.type == ElectricalComponentType::Inductor) {
            ind_i_prev[comp.id] = 0.0;
            ind_i_prev2[comp.id] = 0.0;
            ind_v_prev[comp.id] = 0.0;

            InductorTransientState ind_state;
            ind_state.component_id = comp.id;
            ind_state.l0 = std::max(1.0e-9, comp.value_primary);
            ind_state.isat = (comp.value_secondary > 0.0) ? comp.value_secondary : 5.0; // 5A default
            result.inductor_states[comp.id] = ind_state;
        } else if (comp.id.find("BAT") != std::string::npos) {
            BatteryTransientState bat;
            bat.component_id = comp.id;
            bat.current_soc = 1.0;
            bat.terminal_voltage = comp.value_primary > 0.0 ? comp.value_primary : 4.2;
            result.battery_states[comp.id] = bat;
        }
    }

    double t = options_.t_start;
    double dt = std::clamp(options_.dt_initial, options_.dt_min, options_.dt_max);
    size_t step_count = 0;

    std::unordered_map<uint32_t, double> v_current;
    for (uint32_t n : sorted_indices) {
        v_current[n] = 0.0;
    }

    while (t <= options_.t_stop) {
        DenseMatrix A(dim, dim);
        std::vector<double> z(dim, 0.0);

        // Ground shunt for numerical stability
        for (size_t r = 0; r < num_nodes; ++r) {
            A(r, r) += 1.0e-12;
        }

        // 1. Resistors
        for (const auto& comp : components) {
            if (comp.type == ElectricalComponentType::Resistor && comp.pins.size() >= 2) {
                const int p = get_row_of_node(get_node_of_pin(comp.id, comp.pins[0].pin_id));
                const int q = get_row_of_node(get_node_of_pin(comp.id, comp.pins[1].pin_id));
                const double r = std::max(1.0e-12, comp.value_primary);
                const double g = 1.0 / r;

                if (p >= 0) A(p, p) += g;
                if (q >= 0) A(q, q) += g;
                if (p >= 0 && q >= 0) {
                    A(p, q) -= g;
                    A(q, p) -= g;
                }
            }
        }

        // 2. Capacitors (Trapezoidal, BDF2, or BackwardEuler)
        for (const auto& comp : components) {
            if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
                const int p = get_row_of_node(get_node_of_pin(comp.id, comp.pins[0].pin_id));
                const int q = get_row_of_node(get_node_of_pin(comp.id, comp.pins[1].pin_id));
                const double c = std::max(1.0e-18, comp.value_primary);

                double g_eq = 0.0;
                double i_eq = 0.0;

                if (options_.method == TransientMethod::Trapezoidal) {
                    g_eq = (2.0 * c) / dt;
                    i_eq = g_eq * cap_v_prev[comp.id] + cap_i_prev[comp.id];
                } else if (options_.method == TransientMethod::GearBDF2 && step_count >= 2) {
                    g_eq = (1.5 * c) / dt;
                    i_eq = (c / dt) * (2.0 * cap_v_prev[comp.id] - 0.5 * cap_v_prev2[comp.id]);
                } else { // Backward Euler
                    g_eq = c / dt;
                    i_eq = g_eq * cap_v_prev[comp.id];
                }

                if (p >= 0) { A(p, p) += g_eq; z[p] += i_eq; }
                if (q >= 0) { A(q, q) += g_eq; z[q] -= i_eq; }
                if (p >= 0 && q >= 0) {
                    A(p, q) -= g_eq;
                    A(q, p) -= g_eq;
                }
            }
        }

        // 3. Inductors (with magnetic saturation)
        for (const auto& comp : components) {
            if (comp.type == ElectricalComponentType::Inductor && comp.pins.size() >= 2) {
                const int p = get_row_of_node(get_node_of_pin(comp.id, comp.pins[0].pin_id));
                const int q = get_row_of_node(get_node_of_pin(comp.id, comp.pins[1].pin_id));

                auto& ind_state = result.inductor_states[comp.id];
                double l_eff = evaluate_saturable_inductance(ind_state.l0, ind_state.isat, ind_i_prev[comp.id]);

                double g_eq = 0.0;
                double i_eq = 0.0;

                if (options_.method == TransientMethod::Trapezoidal) {
                    g_eq = dt / (2.0 * l_eff);
                    i_eq = ind_i_prev[comp.id] + g_eq * ind_v_prev[comp.id];
                } else if (options_.method == TransientMethod::GearBDF2 && step_count >= 2) {
                    g_eq = (2.0 * dt) / (3.0 * l_eff);
                    i_eq = (4.0 / 3.0) * ind_i_prev[comp.id] - (1.0 / 3.0) * ind_i_prev2[comp.id];
                } else { // Backward Euler
                    g_eq = dt / l_eff;
                    i_eq = ind_i_prev[comp.id];
                }

                if (p >= 0) { A(p, p) += g_eq; z[p] -= i_eq; }
                if (q >= 0) { A(q, q) += g_eq; z[q] += i_eq; }
                if (p >= 0 && q >= 0) {
                    A(p, q) -= g_eq;
                    A(q, p) -= g_eq;
                }
            }
        }

        // 4. Voltage Sources & Batteries
        for (size_t b = 0; b < num_branches; ++b) {
            const auto* vsrc = v_sources[b];
            const int p = get_row_of_node(get_node_of_pin(vsrc->id, vsrc->pins[0].pin_id));
            const int q = get_row_of_node(get_node_of_pin(vsrc->id, vsrc->pins[1].pin_id));

            if (p >= 0) {
                A(p, num_nodes + b) += 1.0;
                A(num_nodes + b, p) += 1.0;
            }
            if (q >= 0) {
                A(q, num_nodes + b) -= 1.0;
                A(num_nodes + b, q) -= 1.0;
            }

            double v_val = vsrc->value_primary;
            if (result.battery_states.count(vsrc->id)) {
                v_val = result.battery_states[vsrc->id].terminal_voltage;
            } else if (vsrc->type == ElectricalComponentType::VoltageSourcePulse) {
                // Pulse source: period = 100us, duty = 50%
                double period = 1.0e-4;
                double phase = std::fmod(t, period);
                v_val = (phase < 0.5 * period) ? vsrc->value_primary : 0.0;
            }
            z[num_nodes + b] = v_val;
        }

        // Solve system [A] [x] = [z]
        auto sol_res = A.solve_lu(z);
        if (sol_res.is_err()) {
            return core::base::Result<TransientSimulationResult>::err(sol_res.error());
        }

        const auto& x = sol_res.value();

        // LTE Estimation for Adaptive Step Control
        double max_lte = 0.0;
        for (size_t i = 0; i < num_nodes; ++i) {
            uint32_t n_idx = sorted_indices[i];
            double v_new = x[i];
            double dv = std::abs(v_new - v_current[n_idx]);
            if (dv > max_lte) max_lte = dv;
        }

        if (options_.enable_adaptive_stepping && max_lte > options_.lte_tolerance && dt > options_.dt_min) {
            // Reject step and reduce dt
            dt = std::max(options_.dt_min, dt * 0.5);
            result.rejected_steps++;
            continue;
        }

        // Step Accepted
        result.accepted_steps++;
        t += dt;
        result.time_points.push_back(t);

        // Record node voltages
        for (size_t i = 0; i < num_nodes; ++i) {
            uint32_t n_idx = sorted_indices[i];
            v_current[n_idx] = x[i];
            result.node_voltages[n_idx].push_back(x[i]);
        }

        // Record branch currents & update batteries
        for (size_t b = 0; b < num_branches; ++b) {
            const auto* vsrc = v_sources[b];
            double i_branch = x[num_nodes + b];
            result.branch_currents[vsrc->id].push_back(i_branch);

            if (result.battery_states.count(vsrc->id)) {
                auto& bat = result.battery_states[vsrc->id];
                double i_discharge = -i_branch; // current leaving positive terminal
                bat.current_soc = std::max(0.0, bat.current_soc - (i_discharge * dt) / (3600.0 * bat.capacity_ah));
                // 7-region empirical OCV curve (approx 3.0V at 0% to 4.2V at 100%)
                double ocv = 3.0 + 1.2 * bat.current_soc;
                bat.terminal_voltage = ocv - i_discharge * bat.internal_resistance_ohms;
                // Thermal heating
                double p_loss = i_discharge * i_discharge * bat.internal_resistance_ohms;
                bat.cell_temperature_k += (p_loss - 0.35 * (bat.cell_temperature_k - 298.15)) / 42.0 * dt;
            }
        }

        // Update capacitor states
        for (const auto& comp : components) {
            if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
                uint32_t np = get_node_of_pin(comp.id, comp.pins[0].pin_id);
                uint32_t nq = get_node_of_pin(comp.id, comp.pins[1].pin_id);
                double vp = (np > 0) ? v_current[np] : 0.0;
                double vq = (nq > 0) ? v_current[nq] : 0.0;
                double vc = vp - vq;
                double c = std::max(1.0e-18, comp.value_primary);

                cap_v_prev2[comp.id] = cap_v_prev[comp.id];
                cap_v_prev[comp.id] = vc;
                cap_i_prev[comp.id] = (c / dt) * (vc - cap_v_prev2[comp.id]);
            }
        }

        // Update inductor states
        for (const auto& comp : components) {
            if (comp.type == ElectricalComponentType::Inductor && comp.pins.size() >= 2) {
                uint32_t np = get_node_of_pin(comp.id, comp.pins[0].pin_id);
                uint32_t nq = get_node_of_pin(comp.id, comp.pins[1].pin_id);
                double vp = (np > 0) ? v_current[np] : 0.0;
                double vq = (nq > 0) ? v_current[nq] : 0.0;
                double vl = vp - vq;

                auto& ind_state = result.inductor_states[comp.id];
                double l_eff = evaluate_saturable_inductance(ind_state.l0, ind_state.isat, ind_i_prev[comp.id]);
                double i_new = ind_i_prev[comp.id] + (dt / l_eff) * vl;

                ind_i_prev2[comp.id] = ind_i_prev[comp.id];
                ind_i_prev[comp.id] = i_new;
                ind_v_prev[comp.id] = vl;

                ind_state.current = i_new;
                ind_state.stored_energy_joules = 0.5 * l_eff * i_new * i_new;
                ind_state.back_emf_volts = vl;
            }
        }

        // Oscilloscope Probes
        for (auto& probe : result.probes) {
            double sample = 0.0;
            if (probe.is_current_probe) {
                if (result.branch_currents.count(probe.target_label)) {
                    sample = result.branch_currents[probe.target_label].back();
                }
            } else {
                if (probe.target_node > 0 && v_current.count(probe.target_node)) {
                    sample = v_current[probe.target_node];
                }
            }
            probe.add_sample(t, sample);
        }

        step_count++;

        // Adaptive step enlargement if smooth
        if (options_.enable_adaptive_stepping && max_lte < 0.2 * options_.lte_tolerance) {
            dt = std::min(options_.dt_max, dt * 1.25);
        }
    }

    result.success = true;
    return core::base::Result<TransientSimulationResult>::ok(std::move(result));
}

} // namespace shunya::cirsim
