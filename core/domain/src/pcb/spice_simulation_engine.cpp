#include "shunya/pcb/spice_simulation_engine.hpp"
#include "shunya/pcb/newton_solver_circuit.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace shunya::pcb {

namespace {

constexpr double kPi = 3.14159265358979323846;

bool is_voltage_branch_source(core::domain::ElectricalComponentType t) noexcept {
    using T = core::domain::ElectricalComponentType;
    return t == T::VoltageSourceDC || t == T::VoltageSourceAC ||
           t == T::VoltageSourcePulse || t == T::VCVS;
}

double eval_source_voltage(const core::domain::ElectricalComponent& comp, double t) noexcept {
    using T = core::domain::ElectricalComponentType;
    if (comp.type == T::VoltageSourceAC) {
        return comp.waveform.evaluate_sine(t);
    }
    if (comp.type == T::VoltageSourcePulse) {
        return comp.waveform.evaluate_pulse(t);
    }
    return comp.value_primary;
}

void stamp_conductance(core::domain::DenseMatrix& A, uint32_t p, uint32_t q, double g) noexcept {
    if (p > 0) A.add(p - 1, p - 1, g);
    if (q > 0) A.add(q - 1, q - 1, g);
    if (p > 0 && q > 0) {
        A.add(p - 1, q - 1, -g);
        A.add(q - 1, p - 1, -g);
    }
}

void stamp_complex_conductance(
    core::domain::ComplexDenseMatrix& A,
    uint32_t p, uint32_t q,
    std::complex<double> y) noexcept {
    if (p > 0) A.add(p - 1, p - 1, y);
    if (q > 0) A.add(q - 1, q - 1, y);
    if (p > 0 && q > 0) {
        A.add(p - 1, q - 1, -y);
        A.add(q - 1, p - 1, -y);
    }
}

size_t count_non_ground_nodes(const core::domain::CircuitNetlist& netlist) noexcept {
    size_t n = 0;
    for (const auto& [name, node] : netlist.nodes()) {
        if (!node.is_ground) ++n;
    }
    return n;
}

double damp_diode_newton_step(
    const core::domain::CircuitNetlist& netlist,
    const std::vector<double>& x_old,
    std::vector<double>& x_new) noexcept {
    double alpha = 1.0;
    auto v_of = [](const std::vector<double>& x, uint32_t idx) noexcept -> double {
        return (idx == 0) ? 0.0 : x[idx - 1];
    };

    for (const auto& comp : netlist.components()) {
        if (comp.type != core::domain::ElectricalComponentType::Diode || comp.pins.size() < 2) {
            continue;
        }
        const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
        const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);
        const double vd_old = v_of(x_old, p) - v_of(x_old, q);
        const double vd_raw = v_of(x_new, p) - v_of(x_new, q);
        const double vd_lim = NewtonCircuitSolver::limit_junction_voltage(
            vd_raw, vd_old, comp.thermal_voltage, comp.ideality_factor, comp.is_saturation_current);
        const double diff_raw = vd_raw - vd_old;
        if (std::abs(diff_raw) > 1e-12 && std::abs(vd_lim - vd_raw) > 1e-9) {
            const double ratio = std::abs(vd_lim - vd_old) / std::abs(diff_raw);
            alpha = std::min(alpha, std::clamp(ratio, 0.05, 1.0));
        }
    }

    if (alpha < 1.0) {
        for (size_t i = 0; i < x_new.size(); ++i) {
            x_new[i] = x_old[i] + alpha * (x_new[i] - x_old[i]);
        }
    }
    return alpha;
}

bool newton_converged(
    const std::vector<double>& x_old,
    const std::vector<double>& x_new,
    size_t num_nodes,
    double vtol,
    double reltol) noexcept {
    for (size_t i = 0; i < num_nodes; ++i) {
        const double diff = std::abs(x_new[i] - x_old[i]);
        const double tol = vtol + reltol * std::max(std::abs(x_new[i]), std::abs(x_old[i]));
        if (diff > tol) return false;
    }
    for (size_t i = num_nodes; i < x_new.size(); ++i) {
        const double diff = std::abs(x_new[i] - x_old[i]);
        const double tol = 1e-9 + reltol * std::max(std::abs(x_new[i]), std::abs(x_old[i]));
        if (diff > tol) return false;
    }
    return true;
}

} // namespace

core::base::Result<core::domain::DCOperatingPointResult> SpiceSimulationEngine::solve_dc(
    const core::domain::CircuitNetlist& netlist,
    uint32_t max_newton_iters,
    double voltage_tol_v,
    double gmin) {
    using namespace core::domain;

    const size_t num_nodes = count_non_ground_nodes(netlist);
    if (num_nodes == 0) {
        return core::base::Result<DCOperatingPointResult>::err(
            core::base::Error{20, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Circuit contains no non-ground nodes", ""});
    }

    std::unordered_map<std::string, size_t> branch_map;
    size_t num_branches = 0;
    for (const auto& comp : netlist.components()) {
        if (is_voltage_branch_source(comp.type) || comp.type == ElectricalComponentType::Inductor) {
            branch_map[comp.id] = num_branches++;
        }
    }

    const size_t dim = num_nodes + num_branches;
    DenseMatrix A_lin(dim, dim);
    std::vector<double> z_lin(dim, 0.0);

    for (size_t i = 0; i < num_nodes; ++i) {
        A_lin.add(i, i, gmin);
    }

    for (const auto& comp : netlist.components()) {
        if (comp.pins.size() < 2) continue;
        const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
        const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

        switch (comp.type) {
            case ElectricalComponentType::Resistor: {
                stamp_conductance(A_lin, p, q, 1.0 / std::max(1e-12, comp.value_primary));
                break;
            }
            case ElectricalComponentType::CurrentSourceDC: {
                if (p > 0) z_lin[p - 1] -= comp.value_primary;
                if (q > 0) z_lin[q - 1] += comp.value_primary;
                break;
            }
            case ElectricalComponentType::VoltageSourceDC:
            case ElectricalComponentType::VoltageSourceAC:
            case ElectricalComponentType::VoltageSourcePulse:
            case ElectricalComponentType::Inductor: {
                const size_t row = num_nodes + branch_map[comp.id];
                if (p > 0) {
                    A_lin.add(p - 1, row, 1.0);
                    A_lin.add(row, p - 1, 1.0);
                }
                if (q > 0) {
                    A_lin.add(q - 1, row, -1.0);
                    A_lin.add(row, q - 1, -1.0);
                }
                if (comp.type == ElectricalComponentType::Inductor) {
                    z_lin[row] = 0.0;
                } else {
                    z_lin[row] = eval_source_voltage(comp, 0.0);
                }
                break;
            }
            case ElectricalComponentType::VCVS: {
                const size_t row = num_nodes + branch_map[comp.id];
                if (p > 0) {
                    A_lin.add(p - 1, row, 1.0);
                    A_lin.add(row, p - 1, 1.0);
                }
                if (q > 0) {
                    A_lin.add(q - 1, row, -1.0);
                    A_lin.add(row, q - 1, -1.0);
                }
                const uint32_t cp = comp.ctrl_node_pos.empty() ? 0 : netlist.get_node_index(comp.ctrl_node_pos);
                const uint32_t cq = comp.ctrl_node_neg.empty() ? 0 : netlist.get_node_index(comp.ctrl_node_neg);
                if (cp > 0) A_lin.add(row, cp - 1, -comp.value_tertiary);
                if (cq > 0) A_lin.add(row, cq - 1, comp.value_tertiary);
                break;
            }
            default:
                break;
        }
    }

    std::vector<double> x(dim, 0.0);
    DCOperatingPointResult result;

    for (uint32_t iter = 1; iter <= max_newton_iters; ++iter) {
        DenseMatrix A = A_lin;
        std::vector<double> z = z_lin;
        NewtonCircuitSolver::stamp_nonlinear_devices(netlist, x, A, z);

        auto sol = A.solve_lu(z);
        if (!sol.is_ok()) {
            return core::base::Result<DCOperatingPointResult>::err(sol.error());
        }
        std::vector<double> x_next = std::move(sol.value());
        damp_diode_newton_step(netlist, x, x_next);

        const bool done = newton_converged(x, x_next, num_nodes, voltage_tol_v, 1e-4);
        x = std::move(x_next);
        result.iterations = iter;
        if (done) {
            result.converged = true;
            break;
        }
    }

    if (!result.converged) {
        return core::base::Result<DCOperatingPointResult>::err(
            core::base::Error{21, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
                              "DC operating point Newton-Raphson failed to converge", ""});
    }

    for (const auto& [name, node] : netlist.nodes()) {
        result.node_voltages[name] = node.is_ground ? 0.0 : x[node.index - 1];
    }
    for (const auto& [comp_id, bidx] : branch_map) {
        result.branch_currents[comp_id] = x[num_nodes + bidx];
    }
    return core::base::Result<DCOperatingPointResult>::ok(std::move(result));
}

core::base::Result<core::domain::TransientResult> SpiceSimulationEngine::solve_transient(
    const core::domain::CircuitNetlist& netlist,
    const SpiceTransientOptions& opts) {
    using namespace core::domain;

    if (opts.t_stop_s <= 0.0 || opts.dt_s <= 0.0) {
        return core::base::Result<TransientResult>::err(
            core::base::Error{22, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Transient t_stop_s and dt_s must be positive", ""});
    }

    const size_t num_nodes = count_non_ground_nodes(netlist);
    if (num_nodes == 0) {
        return core::base::Result<TransientResult>::err(
            core::base::Error{23, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Circuit has no non-ground nodes", ""});
    }

    std::unordered_map<std::string, size_t> branch_map;
    size_t num_branches = 0;
    for (const auto& comp : netlist.components()) {
        if (is_voltage_branch_source(comp.type)) {
            branch_map[comp.id] = num_branches++;
        }
    }
    const size_t dim = num_nodes + num_branches;

    // Companion history storage per reactive element
    std::unordered_map<std::string, double> cap_v1, cap_v2, cap_i1;
    std::unordered_map<std::string, double> ind_v1, ind_i1, ind_i2;

    std::vector<double> x_guess(dim, 0.0);
    if (opts.use_dc_op_initial) {
        auto dc_res = solve_dc(netlist, opts.max_newton_iters, opts.voltage_tol_v, opts.gmin);
        if (dc_res.is_ok()) {
            const auto& dc = dc_res.value();
            for (const auto& [name, node] : netlist.nodes()) {
                if (!node.is_ground) {
                    x_guess[node.index - 1] = dc.get_voltage(name);
                }
            }
            for (const auto& [id, bidx] : branch_map) {
                x_guess[num_nodes + bidx] = dc.get_current(id);
            }
            for (const auto& comp : netlist.components()) {
                if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
                    const double vp = dc.get_voltage(comp.pins[0].connected_net);
                    const double vq = dc.get_voltage(comp.pins[1].connected_net);
                    cap_v1[comp.id] = vp - vq;
                    cap_v2[comp.id] = vp - vq;
                    cap_i1[comp.id] = 0.0;
                } else if (comp.type == ElectricalComponentType::Inductor) {
                    const double il = dc.get_current(comp.id);
                    ind_i1[comp.id] = il;
                    ind_i2[comp.id] = il;
                    ind_v1[comp.id] = 0.0;
                }
            }
        }
    }

    // Explicit .IC overrides stored in value_secondary
    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::Capacitor && comp.value_secondary != 0.0) {
            cap_v1[comp.id] = comp.value_secondary;
            cap_v2[comp.id] = comp.value_secondary;
            cap_i1[comp.id] = 0.0;
            if (comp.pins.size() >= 2) {
                const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
                const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);
                if (p > 0 && q == 0) x_guess[p - 1] = comp.value_secondary;
            }
        } else if (comp.type == ElectricalComponentType::Inductor && comp.value_secondary != 0.0) {
            ind_i1[comp.id] = comp.value_secondary;
            ind_i2[comp.id] = comp.value_secondary;
            ind_v1[comp.id] = 0.0;
        }
    }

    const uint32_t num_steps = static_cast<uint32_t>(std::ceil(opts.t_stop_s / opts.dt_s));
    const double dt = opts.dt_s;

    TransientResult result;
    result.time_points.reserve(num_steps + 1);
    result.time_points.push_back(0.0);
    for (const auto& [name, node] : netlist.nodes()) {
        const double v0 = node.is_ground ? 0.0 : x_guess[node.index - 1];
        result.node_voltages[name].push_back(v0);
    }
    for (const auto& comp : netlist.components()) {
        if (comp.type == ElectricalComponentType::Capacitor) {
            result.branch_currents[comp.id].push_back(cap_i1[comp.id]);
        } else if (comp.type == ElectricalComponentType::Inductor) {
            result.branch_currents[comp.id].push_back(ind_i1[comp.id]);
        } else if (branch_map.count(comp.id)) {
            result.branch_currents[comp.id].push_back(x_guess[num_nodes + branch_map[comp.id]]);
        }
    }

    for (uint32_t step = 1; step <= num_steps; ++step) {
        const double t = step * dt;
        // Use Backward Euler on step 1 to start Gear-2 cleanly and suppress Trapezoidal t=0 ringing
        const SpiceIntegrationMethod m = (step == 1) ? SpiceIntegrationMethod::BackwardEuler : opts.method;

        DenseMatrix A_lin(dim, dim);
        std::vector<double> z_lin(dim, 0.0);
        for (size_t i = 0; i < num_nodes; ++i) {
            A_lin.add(i, i, opts.gmin);
        }

        // Track companion stamp parameters per reactive component so we update currents cleanly
        struct ReactStamp {
            double g_eq{0.0};
            double i_src{0.0};
        };
        std::unordered_map<std::string, ReactStamp> react_stamps;

        for (const auto& comp : netlist.components()) {
            if (comp.pins.size() < 2) continue;
            const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
            const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

            switch (comp.type) {
                case ElectricalComponentType::Resistor: {
                    stamp_conductance(A_lin, p, q, 1.0 / std::max(1e-12, comp.value_primary));
                    break;
                }
                case ElectricalComponentType::Capacitor: {
                    const double c = std::max(1e-18, comp.value_primary);
                    const double v1 = cap_v1[comp.id];
                    const double v2 = cap_v2[comp.id];
                    const double i1 = cap_i1[comp.id];
                    double g_eq = 0.0;
                    double i_eq = 0.0;
                    if (m == SpiceIntegrationMethod::BackwardEuler) {
                        g_eq = c / dt;
                        i_eq = g_eq * v1;
                    } else if (m == SpiceIntegrationMethod::Trapezoidal) {
                        g_eq = 2.0 * c / dt;
                        i_eq = g_eq * v1 + i1;
                    } else { // Gear2
                        g_eq = 1.5 * c / dt;
                        i_eq = (c / (2.0 * dt)) * (4.0 * v1 - v2);
                    }
                    stamp_conductance(A_lin, p, q, g_eq);
                    if (p > 0) z_lin[p - 1] += i_eq;
                    if (q > 0) z_lin[q - 1] -= i_eq;
                    react_stamps[comp.id] = {g_eq, i_eq};
                    break;
                }
                case ElectricalComponentType::Inductor: {
                    const double l = std::max(1e-18, comp.value_primary);
                    const double i1 = ind_i1[comp.id];
                    const double i2 = ind_i2[comp.id];
                    const double v1 = ind_v1[comp.id];
                    double g_eq = 0.0;
                    double i_hist = 0.0;
                    if (m == SpiceIntegrationMethod::BackwardEuler) {
                        g_eq = dt / l;
                        i_hist = i1;
                    } else if (m == SpiceIntegrationMethod::Trapezoidal) {
                        g_eq = dt / (2.0 * l);
                        i_hist = g_eq * v1 + i1;
                    } else { // Gear2
                        g_eq = (2.0 * dt) / (3.0 * l);
                        i_hist = (4.0 * i1 - i2) / 3.0;
                    }
                    stamp_conductance(A_lin, p, q, g_eq);
                    if (p > 0) z_lin[p - 1] -= i_hist;
                    if (q > 0) z_lin[q - 1] += i_hist;
                    react_stamps[comp.id] = {g_eq, i_hist};
                    break;
                }
                case ElectricalComponentType::CurrentSourceDC: {
                    if (p > 0) z_lin[p - 1] -= comp.value_primary;
                    if (q > 0) z_lin[q - 1] += comp.value_primary;
                    break;
                }
                case ElectricalComponentType::CurrentSourceAC: {
                    const double ic = comp.waveform.evaluate_sine(t);
                    if (p > 0) z_lin[p - 1] -= ic;
                    if (q > 0) z_lin[q - 1] += ic;
                    break;
                }
                case ElectricalComponentType::VoltageSourceDC:
                case ElectricalComponentType::VoltageSourceAC:
                case ElectricalComponentType::VoltageSourcePulse: {
                    const size_t row = num_nodes + branch_map[comp.id];
                    if (p > 0) {
                        A_lin.add(p - 1, row, 1.0);
                        A_lin.add(row, p - 1, 1.0);
                    }
                    if (q > 0) {
                        A_lin.add(q - 1, row, -1.0);
                        A_lin.add(row, q - 1, -1.0);
                    }
                    z_lin[row] = eval_source_voltage(comp, t);
                    break;
                }
                case ElectricalComponentType::VCVS: {
                    const size_t row = num_nodes + branch_map[comp.id];
                    if (p > 0) {
                        A_lin.add(p - 1, row, 1.0);
                        A_lin.add(row, p - 1, 1.0);
                    }
                    if (q > 0) {
                        A_lin.add(q - 1, row, -1.0);
                        A_lin.add(row, q - 1, -1.0);
                    }
                    const uint32_t cp = comp.ctrl_node_pos.empty() ? 0 : netlist.get_node_index(comp.ctrl_node_pos);
                    const uint32_t cq = comp.ctrl_node_neg.empty() ? 0 : netlist.get_node_index(comp.ctrl_node_neg);
                    if (cp > 0) A_lin.add(row, cp - 1, -comp.value_tertiary);
                    if (cq > 0) A_lin.add(row, cq - 1, comp.value_tertiary);
                    break;
                }
                default:
                    break;
            }
        }

        // Newton-Raphson inner loop on the time step
        std::vector<double> x = x_guess;
        bool step_ok = false;
        for (uint32_t nr = 1; nr <= opts.max_newton_iters; ++nr) {
            DenseMatrix A = A_lin;
            std::vector<double> z = z_lin;
            NewtonCircuitSolver::stamp_nonlinear_devices(netlist, x, A, z);

            auto sol = A.solve_lu(z);
            if (!sol.is_ok()) {
                return core::base::Result<TransientResult>::err(sol.error());
            }
            std::vector<double> x_next = std::move(sol.value());
            damp_diode_newton_step(netlist, x, x_next);

            const bool conv = newton_converged(x, x_next, num_nodes, opts.voltage_tol_v, opts.reltol);
            x = std::move(x_next);
            if (conv) {
                step_ok = true;
                break;
            }
        }

        if (!step_ok) {
            return core::base::Result<TransientResult>::err(
                core::base::Error{24, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
                                  "Transient Newton-Raphson failed to converge at step " + std::to_string(step), ""});
        }

        x_guess = x;
        result.time_points.push_back(t);
        for (const auto& [name, node] : netlist.nodes()) {
            result.node_voltages[name].push_back(node.is_ground ? 0.0 : x[node.index - 1]);
        }

        auto v_of = [&](uint32_t idx) noexcept -> double {
            return (idx == 0) ? 0.0 : x[idx - 1];
        };

        for (const auto& comp : netlist.components()) {
            if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
                const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
                const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);
                const double v_now = v_of(p) - v_of(q);
                const auto& rs = react_stamps[comp.id];
                const double i_now = rs.g_eq * v_now - rs.i_src;
                cap_v2[comp.id] = cap_v1[comp.id];
                cap_v1[comp.id] = v_now;
                cap_i1[comp.id] = i_now;
                result.branch_currents[comp.id].push_back(i_now);
            } else if (comp.type == ElectricalComponentType::Inductor && comp.pins.size() >= 2) {
                const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
                const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);
                const double v_now = v_of(p) - v_of(q);
                const auto& rs = react_stamps[comp.id];
                const double i_now = rs.g_eq * v_now + rs.i_src;
                ind_i2[comp.id] = ind_i1[comp.id];
                ind_i1[comp.id] = i_now;
                ind_v1[comp.id] = v_now;
                result.branch_currents[comp.id].push_back(i_now);
            } else if (branch_map.count(comp.id)) {
                result.branch_currents[comp.id].push_back(x[num_nodes + branch_map[comp.id]]);
            }
        }
    }

    result.success = true;
    return core::base::Result<TransientResult>::ok(std::move(result));
}

core::base::Result<core::domain::AcAnalysisResult> SpiceSimulationEngine::solve_ac(
    const core::domain::CircuitNetlist& netlist,
    const SpiceAcOptions& opts) {
    using namespace core::domain;

    if (opts.f_start_hz <= 0.0 || opts.f_stop_hz < opts.f_start_hz || opts.points_per_decade == 0) {
        return core::base::Result<AcAnalysisResult>::err(
            core::base::Error{25, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Invalid AC frequency sweep range or points_per_decade", ""});
    }

    auto dc_res = solve_dc(netlist);
    if (!dc_res.is_ok()) {
        return core::base::Result<AcAnalysisResult>::err(dc_res.error());
    }
    const auto& op = dc_res.value();

    const size_t num_nodes = count_non_ground_nodes(netlist);
    std::unordered_map<std::string, size_t> branch_map;
    size_t num_branches = 0;
    for (const auto& comp : netlist.components()) {
        if (is_voltage_branch_source(comp.type)) {
            branch_map[comp.id] = num_branches++;
        }
    }
    const size_t dim = num_nodes + num_branches;

    const double dec = std::log10(opts.f_stop_hz / opts.f_start_hz);
    const size_t total_pts = std::max<size_t>(1, static_cast<size_t>(std::round(dec * opts.points_per_decade)) + 1);

    AcAnalysisResult result;
    for (size_t k = 0; k < total_pts; ++k) {
        const double frac = (total_pts > 1) ? (static_cast<double>(k) / static_cast<double>(total_pts - 1)) : 0.0;
        const double freq = opts.f_start_hz * std::pow(10.0, frac * dec);
        const double omega = 2.0 * kPi * freq;

        ComplexDenseMatrix A(dim, dim);
        std::vector<std::complex<double>> z(dim, {0.0, 0.0});
        for (size_t i = 0; i < num_nodes; ++i) {
            A.add(i, i, {1e-12, 0.0});
        }

        for (const auto& comp : netlist.components()) {
            if (comp.pins.size() < 2) continue;
            const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net);
            const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net);

            switch (comp.type) {
                case ElectricalComponentType::Resistor:
                    stamp_complex_conductance(A, p, q, {1.0 / std::max(1e-12, comp.value_primary), 0.0});
                    break;
                case ElectricalComponentType::Capacitor:
                    stamp_complex_conductance(A, p, q, {0.0, omega * comp.value_primary});
                    break;
                case ElectricalComponentType::Inductor: {
                    const double x_l = std::max(1e-15, omega * comp.value_primary);
                    stamp_complex_conductance(A, p, q, {0.0, -1.0 / x_l});
                    break;
                }
                case ElectricalComponentType::Diode: {
                    const double vd = op.get_voltage(comp.pins[0].connected_net) -
                                      op.get_voltage(comp.pins[1].connected_net);
                    double id = 0.0, gd = 0.0;
                    NewtonCircuitSolver::evaluate_diode_safe(
                        vd, comp.is_saturation_current, comp.thermal_voltage, comp.ideality_factor, id, gd);
                    stamp_complex_conductance(A, p, q, {gd, 0.0});
                    break;
                }
                case ElectricalComponentType::VoltageSourceDC:
                case ElectricalComponentType::VoltageSourceAC:
                case ElectricalComponentType::VoltageSourcePulse: {
                    const size_t row = num_nodes + branch_map[comp.id];
                    if (p > 0) {
                        A.add(p - 1, row, {1.0, 0.0});
                        A.add(row, p - 1, {1.0, 0.0});
                    }
                    if (q > 0) {
                        A.add(q - 1, row, {-1.0, 0.0});
                        A.add(row, q - 1, {-1.0, 0.0});
                    }
                    if (comp.id == opts.input_source_id) {
                        z[row] = {1.0, 0.0};
                    }
                    break;
                }
                default:
                    break;
            }
        }

        auto sol = A.solve_lu(z);
        if (!sol.is_ok()) {
            return core::base::Result<AcAnalysisResult>::err(sol.error());
        }
        const auto& x = sol.value();

        result.frequencies.push_back(freq);
        for (const auto& [name, node] : netlist.nodes()) {
            std::complex<double> v = node.is_ground ? std::complex<double>{0.0, 0.0} : x[node.index - 1];
            result.node_voltages[name].push_back(v);
        }

        std::complex<double> v_out{0.0, 0.0};
        const uint32_t out_idx = netlist.get_node_index(opts.output_node);
        if (out_idx > 0 && out_idx - 1 < num_nodes) {
            v_out = x[out_idx - 1];
        }
        const double mag = std::max(1e-18, std::abs(v_out));
        AcFrequencyPoint pt;
        pt.frequency_hz = freq;
        pt.complex_gain = v_out;
        pt.magnitude_gain_db = 20.0 * std::log10(mag);
        pt.phase_deg = std::arg(v_out) * 180.0 / kPi;
        result.bode_points.push_back(pt);
    }

    result.success = true;
    return core::base::Result<AcAnalysisResult>::ok(std::move(result));
}

} // namespace shunya::pcb
