#include "shunya/cirsim/nonlinear_dc_solver.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace shunya::cirsim {

using namespace core::domain;

double NonlinearDcOperatingPointSolver::limit_pn_voltage(double v_new, double v_old, double vt) {
    if (vt <= 0.0) vt = 0.02585;
    const double v_crit = vt * std::log(vt / (std::sqrt(2.0) * 1.0e-14)); // ~0.61V

    if (v_new > v_crit && (v_new - v_old) > 0.0) {
        if (v_old > 0.0) {
            double arg = 1.0 + (v_new - v_old) / vt;
            if (arg > 0.0) {
                return v_old + vt * std::log(arg);
            }
            return v_crit;
        }
        return vt * std::log(std::max(1.0, v_new / vt));
    }
    return v_new;
}

void NonlinearDcOperatingPointSolver::linearize_diode(
    double v_d,
    double is,
    double vt,
    double n,
    double& out_g_eq,
    double& out_i_eq
) {
    if (is <= 0.0) is = 1.0e-14;
    if (vt <= 0.0) vt = 0.02585;
    if (n <= 0.0) n = 1.0;

    const double v_eff = std::clamp(v_d, -50.0, 40.0 * n * vt);
    const double exp_term = std::exp(v_eff / (n * vt));
    const double id = is * (exp_term - 1.0);
    const double gd = (is / (n * vt)) * exp_term;

    out_g_eq = std::max(1.0e-12, gd);
    // Norton companion current source: I_eq = i_d - g_eq * v_d
    out_i_eq = id - out_g_eq * v_d;
}

void NonlinearDcOperatingPointSolver::linearize_bjt_npn(
    double v_be,
    double v_ce,
    double is,
    double vt,
    double beta,
    double& out_g_be,
    double& out_g_m,
    double& out_g_ce,
    double& out_i_be_eq,
    double& out_i_c_eq
) {
    if (is <= 0.0) is = 1.0e-14;
    if (vt <= 0.0) vt = 0.02585;
    if (beta <= 0.0) beta = 100.0;

    const double vbe_eff = std::clamp(v_be, -50.0, 1.0);
    const double exp_be = std::exp(vbe_eff / vt);
    const double ib = (is / beta) * (exp_be - 1.0);
    const double gbe = (is / (beta * vt)) * exp_be;

    out_g_be = std::max(1.0e-12, gbe);
    out_i_be_eq = ib - out_g_be * v_be;

    // Active mode IC
    const double va = 100.0; // Early voltage
    const double early_factor = 1.0 + std::max(0.0, v_ce) / va;
    const double ic = is * (exp_be - 1.0) * early_factor;
    const double gm = (ic / vt);
    const double gce = ic / va;

    out_g_m = std::max(0.0, gm);
    out_g_ce = std::max(1.0e-12, gce);
    out_i_c_eq = ic - out_g_m * v_be - out_g_ce * v_ce;
}

void NonlinearDcOperatingPointSolver::linearize_nmos(
    double v_gs,
    double v_ds,
    double v_th,
    double kp,
    double lambda,
    double& out_g_m,
    double& out_g_ds,
    double& out_i_ds_eq
) {
    if (kp <= 0.0) kp = 1.0e-4;
    if (lambda < 0.0) lambda = 0.0;

    const double v_ov = v_gs - v_th;
    if (v_ov <= 0.0) {
        // Cutoff
        out_g_m = 0.0;
        out_g_ds = 1.0e-12;
        out_i_ds_eq = 0.0;
        return;
    }

    if (v_ds < v_ov) {
        // Linear / Triode
        const double ids = kp * (v_ov * v_ds - 0.5 * v_ds * v_ds) * (1.0 + lambda * v_ds);
        out_g_m = kp * v_ds * (1.0 + lambda * v_ds);
        out_g_ds = kp * (v_ov - v_ds) * (1.0 + lambda * v_ds) + lambda * ids;
        out_g_ds = std::max(1.0e-12, out_g_ds);
        out_i_ds_eq = ids - out_g_m * v_gs - out_g_ds * v_ds;
    } else {
        // Saturation
        const double ids = 0.5 * kp * v_ov * v_ov * (1.0 + lambda * v_ds);
        out_g_m = kp * v_ov * (1.0 + lambda * v_ds);
        out_g_ds = 0.5 * kp * v_ov * v_ov * lambda;
        out_g_ds = std::max(1.0e-12, out_g_ds);
        out_i_ds_eq = ids - out_g_m * v_gs - out_g_ds * v_ds;
    }
}

core::base::Result<NonlinearDcResult> NonlinearDcOperatingPointSolver::solve_newton_internal(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components,
    double source_scale,
    double added_gmin,
    std::unordered_map<uint32_t, double> v_current
) {
    // Map non-ground nodes to MNA rows
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
    }
    const size_t num_nodes = sorted_indices.size();

    // Voltage source branches
    std::vector<const ElectricalComponent*> v_sources;
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourceAC ||
            comp.type == ElectricalComponentType::VoltageSourcePulse) {
            v_sources.push_back(&comp);
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

    NonlinearDcResult result;
    result.node_voltages = v_current;
    result.node_voltages[0] = 0.0;

    for (uint32_t iter = 0; iter < options_.max_iterations; ++iter) {
        result.iterations = iter + 1;

        DenseMatrix A(dim, dim);
        std::vector<double> z(dim, 0.0);

        // Add Gmin shunt conductance to every node
        const double total_gmin = options_.nominal_gmin + added_gmin;
        for (size_t r = 0; r < num_nodes; ++r) {
            A(r, r) += total_gmin;
        }

        // Stamp linear passive elements & sources
        for (const auto& comp : components) {
            if (comp.pins.size() < 2) continue;
            const int p = get_row_of_node(get_node_of_pin(comp.id, comp.pins[0].pin_id));
            const int q = get_row_of_node(get_node_of_pin(comp.id, comp.pins[1].pin_id));

            if (comp.type == ElectricalComponentType::Resistor) {
                const double r = std::max(1.0e-12, comp.value_primary);
                const double g = 1.0 / r;
                if (p >= 0) A(p, p) += g;
                if (q >= 0) A(q, q) += g;
                if (p >= 0 && q >= 0) {
                    A(p, q) -= g;
                    A(q, p) -= g;
                }
            } else if (comp.type == ElectricalComponentType::CurrentSourceDC) {
                const double is = comp.value_primary * source_scale;
                if (p >= 0) z[p] += is;
                if (q >= 0) z[q] -= is;
            }
        }

        // Stamp voltage sources
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
            z[num_nodes + b] = vsrc->value_primary * source_scale;
        }

        // Stamp linearized non-linear devices (Diodes, BJTs, MOSFETs)
        for (const auto& comp : components) {
            if (comp.type == ElectricalComponentType::Diode && comp.pins.size() >= 2) {
                const uint32_t np = get_node_of_pin(comp.id, comp.pins[0].pin_id);
                const uint32_t nq = get_node_of_pin(comp.id, comp.pins[1].pin_id);
                const double vd = result.node_voltages[np] - result.node_voltages[nq];

                double geq = 0.0;
                double ieq = 0.0;
                linearize_diode(vd, 1.0e-14, 0.02585, 1.0, geq, ieq);

                const int p = get_row_of_node(np);
                const int q = get_row_of_node(nq);
                if (p >= 0) A(p, p) += geq;
                if (q >= 0) A(q, q) += geq;
                if (p >= 0 && q >= 0) {
                    A(p, q) -= geq;
                    A(q, p) -= geq;
                }
                // Norton current source enters anode p, leaves cathode q
                if (p >= 0) z[p] -= ieq;
                if (q >= 0) z[q] += ieq;
            }
        }

        // Solve linearized system [A] [x] = [z]
        auto sol_res = A.solve_lu(z);
        if (sol_res.is_err()) {
            return core::base::Result<NonlinearDcResult>::err(core::base::Error{
                .code = 1,
                .domain = core::base::ErrorDomain::Simulation,
                .severity = core::base::Severity::Error,
                .message = "Linearized Jacobian matrix is singular at iteration " + std::to_string(iter),
                .details = ""
            });
        }

        const auto& x_sol = sol_res.value();
        double max_dv = 0.0;

        // Apply damped updates and SPICE pn-junction voltage limiting
        for (size_t i = 0; i < num_nodes; ++i) {
            const uint32_t node_idx = sorted_indices[i];
            const double v_prev = result.node_voltages[node_idx];
            const double v_target = x_sol[i];

            // Damped update
            double v_cand = v_prev + options_.damping_factor * (v_target - v_prev);

            // Junction voltage limiting
            v_cand = limit_pn_voltage(v_cand, v_prev);

            const double dv = std::abs(v_cand - v_prev);
            if (dv > max_dv) max_dv = dv;

            result.node_voltages[node_idx] = v_cand;
        }

        result.final_residual = max_dv;

        // Check convergence
        if (max_dv < options_.voltage_tolerance) {
            result.converged = true;
            // Record branch currents
            for (size_t b = 0; b < num_branches; ++b) {
                result.branch_currents[v_sources[b]->id] = x_sol[num_nodes + b];
            }
            break;
        }
    }

    return result;
}

core::base::Result<NonlinearDcResult> NonlinearDcOperatingPointSolver::solve(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components
) {
    std::unordered_map<uint32_t, double> v_initial;
    for (const auto& [idx, _] : topology.nodes) {
        v_initial[idx] = 0.0;
    }

    // 1. Direct Damped Newton-Raphson attempt
    auto res = solve_newton_internal(topology, components, 1.0, 0.0, v_initial);
    if (res.is_ok() && res.value().converged) {
        auto final_result = res.value();
        detect_short_circuits_and_stress(topology, components, final_result);
        return final_result;
    }

    // 2. Source-stepping convergence aid
    if (options_.enable_source_stepping) {
        NonlinearDcResult current_result;
        bool step_failed = false;
        auto v_step = v_initial;
        const uint32_t steps = std::max(5u, options_.source_steps);

        for (uint32_t s = 1; s <= steps; ++s) {
            double lambda = static_cast<double>(s) / static_cast<double>(steps);
            auto step_res = solve_newton_internal(topology, components, lambda, 0.0, v_step);
            if (step_res.is_err() || !step_res.value().converged) {
                step_failed = true;
                break;
            }
            current_result = step_res.value();
            v_step = current_result.node_voltages;
        }

        if (!step_failed && current_result.converged) {
            current_result.used_source_stepping = true;
            detect_short_circuits_and_stress(topology, components, current_result);
            return current_result;
        }
    }

    // 3. Gmin-stepping convergence aid
    if (options_.enable_gmin_stepping) {
        NonlinearDcResult current_result;
        auto v_step = v_initial;
        bool gmin_success = false;

        // Step Gmin from 1e-2 down to 1e-12 in decades
        for (int exp = -2; exp >= -12; exp -= 2) {
            double gmin_val = std::pow(10.0, exp);
            auto step_res = solve_newton_internal(topology, components, 1.0, gmin_val, v_step);
            if (step_res.is_ok() && step_res.value().converged) {
                current_result = step_res.value();
                v_step = current_result.node_voltages;
                gmin_success = true;
            } else {
                gmin_success = false;
                break;
            }
        }

        if (gmin_success && current_result.converged) {
            current_result.used_gmin_stepping = true;
            detect_short_circuits_and_stress(topology, components, current_result);
            return current_result;
        }
    }

    return core::base::Result<NonlinearDcResult>::err(core::base::Error{
        .code = 2,
        .domain = core::base::ErrorDomain::Simulation,
        .severity = core::base::Severity::Error,
        .message = "Non-linear DC operating point failed to converge within specified iteration tolerance.",
        .details = ""
    });
}

void NonlinearDcOperatingPointSolver::detect_short_circuits_and_stress(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components,
    NonlinearDcResult& result
) {
    auto get_node_of_pin = [&](const std::string& comp_id, const std::string& pin_id) -> uint32_t {
        std::string key = comp_id + ":" + pin_id;
        auto it = topology.pin_to_node_map.find(key);
        return (it != topology.pin_to_node_map.end()) ? it->second : 0;
    };

    result.total_power_delivered_watts = 0.0;
    result.total_power_dissipated_watts = 0.0;

    for (const auto& comp : components) {
        if (comp.pins.size() < 2) continue;
        const uint32_t np = get_node_of_pin(comp.id, comp.pins[0].pin_id);
        const uint32_t nq = get_node_of_pin(comp.id, comp.pins[1].pin_id);
        const double vp = result.node_voltages[np];
        const double vq = result.node_voltages[nq];
        const double v_drop = std::abs(vp - vq);

        double i_branch = 0.0;
        if (comp.type == ElectricalComponentType::Resistor) {
            const double r = std::max(1.0e-12, comp.value_primary);
            i_branch = v_drop / r;
            result.branch_currents[comp.id] = i_branch;
        } else if (comp.type == ElectricalComponentType::VoltageSourceDC) {
            i_branch = std::abs(result.branch_currents[comp.id]);
        } else if (comp.type == ElectricalComponentType::CurrentSourceDC) {
            i_branch = comp.value_primary;
            result.branch_currents[comp.id] = i_branch;
        } else if (comp.type == ElectricalComponentType::Diode) {
            const double vd = vp - vq;
            double geq = 0.0, ieq = 0.0;
            linearize_diode(vd, 1.0e-14, 0.02585, 1.0, geq, ieq);
            i_branch = std::max(0.0, geq * vd + ieq);
            result.branch_currents[comp.id] = i_branch;
        }

        const double p_comp = v_drop * i_branch;

        // Energy bookkeeping
        if (comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::CurrentSourceDC) {
            result.total_power_delivered_watts += p_comp;
        } else {
            result.total_power_dissipated_watts += p_comp;
        }

        // Component Stress Analysis
        ComponentPowerStress stress;
        stress.component_id = comp.id;
        stress.voltage_drop = v_drop;
        stress.current = i_branch;
        stress.power_dissipated_watts = p_comp;
        stress.max_power_rating_watts = 0.25; // default 250mW for 1/4W resistor
        stress.stress_ratio = stress.power_dissipated_watts / stress.max_power_rating_watts;
        stress.is_over_stressed = stress.stress_ratio > 1.0;
        result.component_stresses.push_back(stress);

        // Short-Circuit and Over-Current Detection
        if (comp.type == ElectricalComponentType::VoltageSourceDC) {
            if (i_branch > options_.max_current_limit) {
                CircuitDiagnosticReport diag;
                diag.severity = CircuitDiagnosticReport::Severity::CriticalError;
                diag.code = "CIRSIM_SHORT_CIRCUIT";
                diag.component_id = comp.id;
                diag.node_from = np;
                diag.node_to = nq;
                diag.current_level = i_branch;
                std::ostringstream ss;
                ss << "Dead short-circuit detected across voltage source " << comp.id
                   << ": branch current " << i_branch << " A exceeds safety limit ("
                   << options_.max_current_limit << " A)";
                diag.message = ss.str();
                result.diagnostics.push_back(diag);
            }
        } else {
            if (i_branch > options_.max_current_limit) {
                CircuitDiagnosticReport diag;
                diag.severity = CircuitDiagnosticReport::Severity::Warning;
                diag.code = "CIRSIM_OVER_CURRENT";
                diag.component_id = comp.id;
                diag.node_from = np;
                diag.node_to = nq;
                diag.current_level = i_branch;
                std::ostringstream ss;
                ss << "Over-current detected through component " << comp.id
                   << ": " << i_branch << " A (stress: " << (stress.stress_ratio * 100.0) << "%)";
                diag.message = ss.str();
                result.diagnostics.push_back(diag);
            }
        }
    }
}

} // namespace shunya::cirsim
