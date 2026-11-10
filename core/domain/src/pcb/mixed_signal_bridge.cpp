#include "shunya/pcb/mixed_signal_bridge.hpp"
#include "shunya/pcb/newton_solver_circuit.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace shunya::pcb {

namespace {

bool is_vbranch(core::domain::ElectricalComponentType t) noexcept {
    using T = core::domain::ElectricalComponentType;
    return t == T::VoltageSourceDC || t == T::VoltageSourceAC ||
           t == T::VoltageSourcePulse || t == T::VCVS;
}

} // namespace

void MixedSignalBridge::add_a2d(AnalogToDigitalBridgeSpec spec) {
    a2d_channels_.push_back(std::move(spec));
}

void MixedSignalBridge::add_d2a(DigitalToAnalogBridgeSpec spec) {
    d2a_channels_.push_back(std::move(spec));
}

core::base::Result<core::domain::TransientResult> MixedSignalBridge::run_cosimulation(
    core::domain::CircuitNetlist& analog_netlist,
    DigitalEventEngine& digital_engine,
    double t_stop_s,
    double sync_dt_s,
    DigitalStepHook hook) {
    using namespace core::domain;

    if (t_stop_s <= 0.0 || sync_dt_s <= 0.0) {
        return core::base::Result<TransientResult>::err(
            core::base::Error{30, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Mixed-signal co-simulation t_stop_s and sync_dt_s must be positive", ""});
    }

    auto topo = analog_netlist.build_node_topology();
    if (!topo.is_ok()) {
        return core::base::Result<TransientResult>::err(topo.error());
    }

    size_t num_nodes = 0;
    for (const auto& [name, node] : analog_netlist.nodes()) {
        if (!node.is_ground) ++num_nodes;
    }

    std::unordered_map<std::string, size_t> branch_map;
    size_t num_branches = 0;
    for (const auto& comp : analog_netlist.components()) {
        if (is_vbranch(comp.type)) {
            branch_map[comp.id] = num_branches++;
        }
    }
    const size_t dim = num_nodes + num_branches;

    std::unordered_map<std::string, double> cap_v1;
    std::unordered_map<std::string, double> cap_i1;
    std::unordered_map<std::string, double> ind_i1;
    std::unordered_map<std::string, double> ind_v1;

    std::vector<double> x_prev(dim, 0.0);
    const uint32_t num_steps = static_cast<uint32_t>(std::ceil(t_stop_s / sync_dt_s));

    TransientResult result;
    result.time_points.reserve(num_steps + 1);
    result.time_points.push_back(0.0);
    for (const auto& [name, node] : analog_netlist.nodes()) {
        result.node_voltages[name].push_back(0.0);
    }

    auto stamp_g = [](DenseMatrix& A, uint32_t p, uint32_t q, double g) {
        if (p > 0) A.add(p - 1, p - 1, g);
        if (q > 0) A.add(q - 1, q - 1, g);
        if (p > 0 && q > 0) {
            A.add(p - 1, q - 1, -g);
            A.add(q - 1, p - 1, -g);
        }
    };

    for (uint32_t step = 1; step <= num_steps; ++step) {
        const double t_s = step * sync_dt_s;
        const uint64_t t_ps = static_cast<uint64_t>(std::llround(t_s * 1e12));

        // 1) Update digital sources driven by D2A models BEFORE advancing the analog MNA step
        for (const auto& d2a : d2a_channels_) {
            std::vector<StdLogic> bits;
            bits.reserve(d2a.digital_bits_lsb_first.size());
            for (const auto& net : d2a.digital_bits_lsb_first) {
                bits.push_back(digital_engine.get_net_value(net));
            }
            const double v_dac = d2a.dac.evaluate_voltage(bits);
            for (auto& comp : analog_netlist.components()) {
                if (comp.id == d2a.analog_source_id) {
                    comp.value_primary = v_dac;
                    break;
                }
            }
        }

        // 2) Step continuous-time MNA (Backward Euler on step 1, Trapezoidal thereafter)
        const bool use_trap = (step > 1);
        DenseMatrix A_lin(dim, dim);
        std::vector<double> z_lin(dim, 0.0);
        for (size_t i = 0; i < num_nodes; ++i) A_lin.add(i, i, 1e-12);

        std::unordered_map<std::string, std::pair<double, double>> cap_stamp, ind_stamp;

        for (const auto& comp : analog_netlist.components()) {
            if (comp.pins.size() < 2) continue;
            const uint32_t p = analog_netlist.get_node_index(comp.pins[0].connected_net);
            const uint32_t q = analog_netlist.get_node_index(comp.pins[1].connected_net);

            switch (comp.type) {
                case ElectricalComponentType::Resistor:
                    stamp_g(A_lin, p, q, 1.0 / std::max(1e-12, comp.value_primary));
                    break;
                case ElectricalComponentType::Capacitor: {
                    const double c = std::max(1e-18, comp.value_primary);
                    const double g_eq = use_trap ? (2.0 * c / sync_dt_s) : (c / sync_dt_s);
                    const double i_eq = use_trap ? (g_eq * cap_v1[comp.id] + cap_i1[comp.id]) : (g_eq * cap_v1[comp.id]);
                    stamp_g(A_lin, p, q, g_eq);
                    if (p > 0) z_lin[p - 1] += i_eq;
                    if (q > 0) z_lin[q - 1] -= i_eq;
                    cap_stamp[comp.id] = {g_eq, i_eq};
                    break;
                }
                case ElectricalComponentType::Inductor: {
                    const double l = std::max(1e-18, comp.value_primary);
                    const double g_eq = use_trap ? (sync_dt_s / (2.0 * l)) : (sync_dt_s / l);
                    const double i_hist = use_trap ? (g_eq * ind_v1[comp.id] + ind_i1[comp.id]) : ind_i1[comp.id];
                    stamp_g(A_lin, p, q, g_eq);
                    if (p > 0) z_lin[p - 1] -= i_hist;
                    if (q > 0) z_lin[q - 1] += i_hist;
                    ind_stamp[comp.id] = {g_eq, i_hist};
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
                    if (comp.type == ElectricalComponentType::VoltageSourceAC) {
                        z_lin[row] = comp.waveform.evaluate_sine(t_s);
                    } else if (comp.type == ElectricalComponentType::VoltageSourcePulse) {
                        z_lin[row] = comp.waveform.evaluate_pulse(t_s);
                    } else {
                        z_lin[row] = comp.value_primary;
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
                    const uint32_t cp = comp.ctrl_node_pos.empty() ? 0 : analog_netlist.get_node_index(comp.ctrl_node_pos);
                    const uint32_t cq = comp.ctrl_node_neg.empty() ? 0 : analog_netlist.get_node_index(comp.ctrl_node_neg);
                    if (cp > 0) A_lin.add(row, cp - 1, -comp.value_tertiary);
                    if (cq > 0) A_lin.add(row, cq - 1, comp.value_tertiary);
                    break;
                }
                default:
                    break;
            }
        }

        std::vector<double> x = x_prev;
        for (uint32_t nr = 0; nr < 30; ++nr) {
            DenseMatrix A = A_lin;
            std::vector<double> z = z_lin;
            NewtonCircuitSolver::stamp_nonlinear_devices(analog_netlist, x, A, z);
            auto sol = A.solve_lu(z);
            if (!sol.is_ok()) {
                return core::base::Result<TransientResult>::err(sol.error());
            }
            double max_dv = 0.0;
            for (size_t i = 0; i < num_nodes; ++i) {
                max_dv = std::max(max_dv, std::abs(sol.value()[i] - x[i]));
            }
            x = std::move(sol.value());
            if (max_dv < 1e-6) break;
        }
        x_prev = x;

        result.time_points.push_back(t_s);
        for (const auto& [name, node] : analog_netlist.nodes()) {
            result.node_voltages[name].push_back(node.is_ground ? 0.0 : x[node.index - 1]);
        }

        auto v_of = [&](uint32_t idx) -> double { return (idx == 0) ? 0.0 : x[idx - 1]; };
        for (const auto& comp : analog_netlist.components()) {
            if (comp.type == ElectricalComponentType::Capacitor && comp.pins.size() >= 2) {
                const uint32_t p = analog_netlist.get_node_index(comp.pins[0].connected_net);
                const uint32_t q = analog_netlist.get_node_index(comp.pins[1].connected_net);
                const double vn = v_of(p) - v_of(q);
                const auto [geq, ieq] = cap_stamp[comp.id];
                cap_i1[comp.id] = geq * vn - ieq;
                cap_v1[comp.id] = vn;
            } else if (comp.type == ElectricalComponentType::Inductor && comp.pins.size() >= 2) {
                const uint32_t p = analog_netlist.get_node_index(comp.pins[0].connected_net);
                const uint32_t q = analog_netlist.get_node_index(comp.pins[1].connected_net);
                const double vn = v_of(p) - v_of(q);
                const auto [geq, ih] = ind_stamp[comp.id];
                ind_i1[comp.id] = geq * vn + ih;
                ind_v1[comp.id] = vn;
            }
        }

        // 3) Sample A2D channels and advance digital event queue through t_ps
        for (const auto& a2d : a2d_channels_) {
            const uint32_t idx = analog_netlist.get_node_index(a2d.analog_node);
            const double va = v_of(idx);
            const StdLogic cur_dig = digital_engine.get_net_value(a2d.digital_net);
            const StdLogic next_dig = a2d.comparator.sample(va, cur_dig);
            if (next_dig != cur_dig) {
                digital_engine.schedule_driver_event(
                    digital_engine.current_time_ps() + a2d.prop_delay_ps,
                    a2d.digital_net,
                    "a2d_" + a2d.analog_node,
                    next_dig,
                    DelayModel::Transport);
            }
        }

        digital_engine.run_until(t_ps);
        if (hook) {
            hook(step, t_ps, digital_engine);
        }
    }

    result.success = true;
    return core::base::Result<TransientResult>::ok(std::move(result));
}

core::base::Result<DeltaSigmaResult> MixedSignalBridge::run_delta_sigma_modulator(
    const DeltaSigmaConfig& cfg) {
    using namespace core::domain;

    if (cfg.integrator_rc_s <= 0.0 || cfg.sample_period_s <= 0.0 || cfg.num_cycles == 0 || cfg.v_ref <= 0.0) {
        return core::base::Result<DeltaSigmaResult>::err(
            core::base::Error{31, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Invalid Delta-Sigma configuration parameters", ""});
    }

    // Build real MNA active op-amp inverting integrator:
    //   V_in -> R1 (10k) -> sum
    //   V_fb -> R2 (10k) -> sum
    //   VCVS (int_out = 1e5 * (0 - sum)) + C_int from sum to int_out
    CircuitNetlist netlist;
    const double R = 10000.0;
    const double C = cfg.integrator_rc_s / R;

    ElectricalComponent vin;
    vin.id = "V_IN";
    vin.type = ElectricalComponentType::VoltageSourceDC;
    vin.value_primary = cfg.v_input;
    vin.pins = {CircuitPin::make("p", "in"), CircuitPin::make("n", "0")};
    netlist.add_component(vin);

    ElectricalComponent vfb;
    vfb.id = "V_FB";
    vfb.type = ElectricalComponentType::VoltageSourceDC;
    vfb.value_primary = -cfg.v_ref;
    vfb.pins = {CircuitPin::make("p", "fb"), CircuitPin::make("n", "0")};
    netlist.add_component(vfb);

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = R;
    r1.pins = {CircuitPin::make("1", "in"), CircuitPin::make("2", "sum")};
    netlist.add_component(r1);

    ElectricalComponent r2;
    r2.id = "R2";
    r2.type = ElectricalComponentType::Resistor;
    r2.value_primary = R;
    r2.pins = {CircuitPin::make("1", "fb"), CircuitPin::make("2", "sum")};
    netlist.add_component(r2);

    ElectricalComponent cint;
    cint.id = "C_INT";
    cint.type = ElectricalComponentType::Capacitor;
    cint.value_primary = C;
    cint.pins = {CircuitPin::make("1", "sum"), CircuitPin::make("2", "int_out")};
    netlist.add_component(cint);

    ElectricalComponent opamp;
    opamp.id = "E_OP";
    opamp.type = ElectricalComponentType::VCVS;
    opamp.value_tertiary = 1e6;
    opamp.ctrl_node_pos = "0";
    opamp.ctrl_node_neg = "sum";
    opamp.pins = {CircuitPin::make("p", "int_out"), CircuitPin::make("n", "0")};
    netlist.add_component(opamp);

    // Digital clocked comparator + 1-bit feedback DAC on inverting integrator:
    // Because int_out = -integral((V_in + V_fb)/RC), if int_out < 0 (mean positive sum), comparator outputs '1'
    // driving V_fb = -V_ref (to oppose positive V_in and pull int_out back up!).
    DigitalEventEngine dig;
    dig.set_initial_net("cmp_out", StdLogic::Zero, "init");

    MixedSignalBridge bridge;
    // Comparator checks inverted sign: V_int < 0 => digital '1'
    AnalogToDigitalBridgeSpec cmp;
    cmp.analog_node = "int_out";
    cmp.digital_net = "cmp_raw";
    cmp.comparator = ThresholdComparatorAdc{-1e-6, 1e-6, 0.0};
    cmp.prop_delay_ps = 5;
    bridge.add_a2d(cmp);

    // Inverter from cmp_raw -> fb_bit (since inverting integrator moves down when V_in > 0)
    DigitalGateElement inv;
    inv.id = "INV_CMP";
    inv.type = core::domain::GateType::Not;
    inv.inputs = {"cmp_raw"};
    inv.output_net = "fb_bit";
    inv.rise_delay_ps = 10;
    inv.fall_delay_ps = 10;
    inv.delay_model = DelayModel::Transport;
    dig.add_gate(inv);
    dig.set_initial_net("cmp_raw", StdLogic::Zero, "init");
    dig.set_initial_net("fb_bit", StdLogic::One, "init");

    // 1-bit R2R DAC in [-v_ref, +v_ref]: code 0 -> -v_ref, code 1 -> +v_ref
    // Note R2RDacModel with 1 bit outputs v_gnd + (v_ref - v_gnd) * (bit/2).
    // Instead, we set V_FB via the hook from sampled DFF bit so V_FB is exact +/- v_ref.
    DeltaSigmaResult out;
    out.bitstream.reserve(cfg.num_cycles);

    size_t ones_count = 0;
    StdLogic lat_bit = StdLogic::One;
    auto hook = [&](uint64_t /*step*/, uint64_t /*time_ps*/, DigitalEventEngine& engine) {
        // Clock the 1-bit comparator latch on current integrator output
        const StdLogic d = engine.get_net_value("fb_bit");
        lat_bit = (d == StdLogic::One || d == StdLogic::H) ? StdLogic::One : StdLogic::Zero;
        out.bitstream.push_back(lat_bit);
        if (lat_bit == StdLogic::One) ++ones_count;

        // If bit == 1, feedback is -v_ref to push int_out back up (balancing V_in > 0);
        // Wait: in non-inverting signed balance: V_in + V_fb_signed = 0 => V_fb = -v_ref when bit == 1?
        // Let's check: bit=1 means int_out <= 0 (meaning V_in + V_fb > 0 integrated downward!).
        // To push int_out UPWARD (back >= 0), we want V_in + V_fb < 0, i.e., V_fb = -v_ref!
        const double vfb_next = (lat_bit == StdLogic::One) ? -cfg.v_ref : cfg.v_ref;
        for (auto& comp : netlist.components()) {
            if (comp.id == "V_FB") {
                comp.value_primary = vfb_next;
                break;
            }
        }
    };

    const double t_stop = cfg.num_cycles * cfg.sample_period_s;
    auto sim_res = bridge.run_cosimulation(netlist, dig, t_stop, cfg.sample_period_s, hook);
    if (!sim_res.is_ok()) {
        return core::base::Result<DeltaSigmaResult>::err(sim_res.error());
    }

    out.time_points = sim_res.value().time_points;
    if (const auto* trace = sim_res.value().get_voltage_trace("int_out")) {
        out.integrator_voltage = *trace;
    }

    // Since bit=1 occurs when countering positive V_in with -V_ref:
    // Mean(bit * (+v_ref) + (1-bit) * (-v_ref)) = (2 * density - 1) * v_ref == +v_input
    out.ones_density = static_cast<double>(ones_count) / static_cast<double>(std::max<size_t>(1, out.bitstream.size()));
    out.average_digital_voltage = (2.0 * out.ones_density - 1.0) * cfg.v_ref;
    return core::base::Result<DeltaSigmaResult>::ok(std::move(out));
}

} // namespace shunya::pcb
