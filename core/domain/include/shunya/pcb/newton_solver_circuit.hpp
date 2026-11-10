#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace shunya::pcb {

/**
 * @brief Nonlinear MNA Newton-Raphson solver with SPICE pnjlim voltage limiting and linear tail continuation.
 */
class NewtonCircuitSolver {
public:
    /**
     * @brief SPICE-style p-n junction voltage limiting preventing exponential overflow across iterations.
     */
    [[nodiscard]] static double limit_junction_voltage(
        double v_new, double v_old, double vt, double n, double is) noexcept {
        const double nvt = std::max(1e-6, n * vt);
        const double vcrit = nvt * std::log(std::max(1e-12, nvt / (std::sqrt(2.0) * std::max(1e-24, is))));
        if (v_new > vcrit && std::abs(v_new - v_old) > 2.0 * nvt) {
            if (v_old > 0.0) {
                const double arg = 1.0 + (v_new - v_old) / nvt;
                if (arg > 0.0) {
                    v_new = v_old + nvt * std::log(arg);
                } else {
                    v_new = vcrit;
                }
            } else {
                v_new = nvt * std::log(std::max(1e-12, v_new / nvt));
            }
        }
        return std::clamp(v_new, -1000.0, 100.0);
    }

    /**
     * @brief Safe Shockley diode evaluation with smooth linear continuation beyond exp(40) to prevent NaN/Inf.
     */
    static void evaluate_diode_safe(
        double vd, double is, double vt, double n,
        double& out_current, double& out_conductance) noexcept {
        const double nvt = std::max(1e-6, n * vt);
        const double x = vd / nvt;
        constexpr double kExpMax = 40.0;
        constexpr double kGmin = 1e-12;
        if (x <= kExpMax) {
            const double ex = (x < -40.0) ? std::exp(-40.0) : std::exp(x);
            out_current = is * (ex - 1.0) + kGmin * vd;
            out_conductance = (is / nvt) * ex + kGmin;
        } else {
            const double ex_max = std::exp(kExpMax);
            const double g_max = (is / nvt) * ex_max + kGmin;
            const double i_max = is * (ex_max - 1.0) + kGmin * (kExpMax * nvt);
            out_conductance = g_max;
            out_current = i_max + g_max * (vd - kExpMax * nvt);
        }
    }

    /**
     * @brief Evaluates all nonlinear components from trial node vector x and stamps linearized companion terms into MNA (A, z).
     */
    static void stamp_nonlinear_devices(
        const core::domain::CircuitNetlist& netlist,
        const std::vector<double>& x_trial,
        core::domain::DenseMatrix& A,
        std::vector<double>& z) noexcept {

        auto v_node = [&](uint32_t idx) noexcept -> double {
            return (idx == 0) ? 0.0 : x_trial[idx - 1];
        };

        auto stamp_g = [&](uint32_t p, uint32_t q, double g) noexcept {
            if (p > 0) A.add(p - 1, p - 1, g);
            if (q > 0) A.add(q - 1, q - 1, g);
            if (p > 0 && q > 0) {
                A.add(p - 1, q - 1, -g);
                A.add(q - 1, p - 1, -g);
            }
        };

        auto stamp_rhs_current = [&](uint32_t p, uint32_t q, double ieq) noexcept {
            // ieq flows in parallel from q -> p (so i_total = g*v - ieq leaving p => RHS += ieq at p)
            if (p > 0) z[p - 1] += ieq;
            if (q > 0) z[q - 1] -= ieq;
        };

        for (const auto& comp : netlist.components()) {
            if (comp.type == core::domain::ElectricalComponentType::Diode && comp.pins.size() >= 2) {
                const uint32_t p = netlist.get_node_index(comp.pins[0].connected_net); // Anode
                const uint32_t q = netlist.get_node_index(comp.pins[1].connected_net); // Cathode
                const double vd = v_node(p) - v_node(q);
                double id = 0.0;
                double gd = 0.0;
                evaluate_diode_safe(vd, comp.is_saturation_current, comp.thermal_voltage, comp.ideality_factor, id, gd);
                stamp_g(p, q, gd);
                // Companion: id ≈ gd * vd_new + (id_0 - gd * vd_0) = gd * vd_new - ieq
                const double ieq = gd * vd - id;
                stamp_rhs_current(p, q, ieq);
            } else if (comp.type == core::domain::ElectricalComponentType::TransistorMOSFET && comp.pins.size() >= 3) {
                // pins[0] = Drain, pins[1] = Gate, pins[2] = Source
                const uint32_t d = netlist.get_node_index(comp.pins[0].connected_net);
                const uint32_t g = netlist.get_node_index(comp.pins[1].connected_net);
                const uint32_t s = netlist.get_node_index(comp.pins[2].connected_net);
                const double vgs = v_node(g) - v_node(s);
                const double vds = v_node(d) - v_node(s);
                double ids = 0.0, gm = 0.0, gds = 0.0;
                core::domain::NonLinearDeviceState::evaluate_mosfet_nmos(
                    vgs, vds, comp.mos_vth, comp.mos_kp, comp.mos_lambda, ids, gm, gds);
                gds = std::max(gds, 1e-12);
                // gds between d and s
                stamp_g(d, s, gds);
                // gm * (Vg - Vs) enters s / leaves d
                if (d > 0 && g > 0) A.add(d - 1, g - 1, gm);
                if (d > 0 && s > 0) A.add(d - 1, s - 1, -gm);
                if (s > 0 && g > 0) A.add(s - 1, g - 1, -gm);
                if (s > 0) A.add(s - 1, s - 1, gm);
                const double ieq = gm * vgs + gds * vds - ids;
                stamp_rhs_current(d, s, ieq);
            } else if (comp.type == core::domain::ElectricalComponentType::TransistorBJT && comp.pins.size() >= 3) {
                // pins[0] = Collector, pins[1] = Base, pins[2] = Emitter
                const uint32_t c = netlist.get_node_index(comp.pins[0].connected_net);
                const uint32_t b = netlist.get_node_index(comp.pins[1].connected_net);
                const uint32_t e = netlist.get_node_index(comp.pins[2].connected_net);
                const double vbe = v_node(b) - v_node(e);
                const double vce = v_node(c) - v_node(e);
                double ib = 0.0, ic = 0.0, gbe = 0.0, gm = 0.0, gce = 0.0;
                core::domain::NonLinearDeviceState::evaluate_bjt_npn(
                    vbe, vce, comp.is_saturation_current, comp.thermal_voltage, comp.bjt_beta_forward,
                    ib, ic, gbe, gm, gce);
                stamp_g(b, e, gbe);
                stamp_g(c, e, gce);
                if (c > 0 && b > 0) A.add(c - 1, b - 1, gm);
                if (c > 0 && e > 0) A.add(c - 1, e - 1, -gm);
                if (e > 0 && b > 0) A.add(e - 1, b - 1, -gm);
                if (e > 0) A.add(e - 1, e - 1, gm);
                const double ieq_be = gbe * vbe - ib;
                const double ieq_ce = gm * vbe + gce * vce - ic;
                stamp_rhs_current(b, e, ieq_be);
                stamp_rhs_current(c, e, ieq_ce);
            }
        }
    }
};

} // namespace shunya::pcb
