#include "shunya/pcb/digital_event_engine.hpp"

#include <algorithm>

namespace shunya::pcb {

const std::vector<WaveformSample> DigitalEventEngine::empty_waveform_{};

void DigitalEventEngine::set_initial_net(const std::string& net, StdLogic val, const std::string& driver_id) {
    update_driver_and_resolve(net, driver_id, val);
}

void DigitalEventEngine::add_gate(DigitalGateElement gate) {
    const size_t idx = gates_.size();
    for (const auto& in : gate.inputs) {
        gate_fanout_[in].push_back(idx);
    }
    if (resolved_nets_.find(gate.output_net) == resolved_nets_.end()) {
        resolved_nets_[gate.output_net] = StdLogic::U;
        waveforms_[gate.output_net].push_back({0, StdLogic::U});
    }
    gates_.push_back(std::move(gate));

    const auto& g = gates_.back();
    std::vector<StdLogic> in_vals;
    in_vals.reserve(g.inputs.size());
    for (const auto& in_name : g.inputs) {
        in_vals.push_back(get_net_value(in_name));
    }
    update_driver_and_resolve(g.output_net, g.id, Ieee1164Resolver::evaluate_gate(g.type, in_vals));
}

void DigitalEventEngine::add_dff(DigitalDffElement dff) {
    const size_t idx = dffs_.size();
    if (!dff.clk_net.empty()) dff_fanout_[dff.clk_net].push_back(idx);
    if (!dff.reset_net.empty()) dff_fanout_[dff.reset_net].push_back(idx);

    update_driver_and_resolve(dff.q_net, dff.id + "_q", dff.stored_q);
    if (!dff.qbar_net.empty()) {
        update_driver_and_resolve(dff.qbar_net, dff.id + "_qbar", Ieee1164Resolver::eval_not(dff.stored_q));
    }
    dffs_.push_back(std::move(dff));
}

void DigitalEventEngine::schedule_driver_event(
    uint64_t time_ps,
    std::string target_net,
    std::string driver_id,
    StdLogic new_val,
    DelayModel model) {

    if (model == DelayModel::Inertial) {
        // Cancel any active pending transition on the same (net, driver) scheduled up to time_ps
        for (const auto& ev_ptr : event_arena_) {
            if (!ev_ptr->cancelled &&
                ev_ptr->time_ps >= current_time_ps_ &&
                ev_ptr->time_ps <= time_ps &&
                ev_ptr->target_net == target_net &&
                ev_ptr->driver_id == driver_id) {
                ev_ptr->cancelled = true;
            }
        }
    }

    auto ev = std::make_unique<QueuedEvent>();
    ev->time_ps = time_ps;
    ev->seq = next_seq_++;
    ev->target_net = std::move(target_net);
    ev->driver_id = std::move(driver_id);
    ev->value = new_val;
    ev->cancelled = false;

    QueuedEvent* raw = ev.get();
    event_arena_.push_back(std::move(ev));
    pq_.push(raw);
}

void DigitalEventEngine::schedule_external_pulse(
    uint64_t start_ps,
    uint64_t width_ps,
    const std::string& net,
    StdLogic pulse_val,
    StdLogic return_val,
    const std::string& driver_id) {
    schedule_driver_event(start_ps, net, driver_id, pulse_val, DelayModel::Transport);
    schedule_driver_event(start_ps + width_ps, net, driver_id, return_val, DelayModel::Transport);
}

void DigitalEventEngine::update_driver_and_resolve(
    const std::string& net,
    const std::string& driver_id,
    StdLogic val) {

    net_drivers_[net][driver_id] = val;

    std::vector<StdLogic> all_drivers;
    all_drivers.reserve(net_drivers_[net].size());
    for (const auto& [did, dval] : net_drivers_[net]) {
        all_drivers.push_back(dval);
    }
    const StdLogic next_resolved = Ieee1164Resolver::resolve_bus(all_drivers);

    auto it = resolved_nets_.find(net);
    if (it == resolved_nets_.end() || it->second != next_resolved) {
        resolved_nets_[net] = next_resolved;
        auto& wf = waveforms_[net];
        if (!wf.empty() && wf.back().time_ps == current_time_ps_) {
            wf.back().value = next_resolved;
        } else {
            wf.push_back({current_time_ps_, next_resolved});
        }
        evaluate_fanout(net);
    }
}

void DigitalEventEngine::evaluate_fanout(const std::string& changed_net) {
    if (auto it = gate_fanout_.find(changed_net); it != gate_fanout_.end()) {
        for (size_t gi : it->second) {
            const auto& g = gates_[gi];
            std::vector<StdLogic> in_vals;
            in_vals.reserve(g.inputs.size());
            for (const auto& in_name : g.inputs) {
                in_vals.push_back(get_net_value(in_name));
            }
            const StdLogic new_out = Ieee1164Resolver::evaluate_gate(g.type, in_vals);
            const StdLogic old_drv = (net_drivers_[g.output_net].count(g.id))
                ? net_drivers_[g.output_net][g.id]
                : StdLogic::U;

            uint64_t delay_ps = g.rise_delay_ps;
            if ((old_drv == StdLogic::One || old_drv == StdLogic::H) &&
                (new_out == StdLogic::Zero || new_out == StdLogic::L)) {
                delay_ps = g.fall_delay_ps;
            }
            // Schedule unconditionally for inertial glitch rejection to cancel opposing pending events
            schedule_driver_event(
                current_time_ps_ + delay_ps,
                g.output_net,
                g.id,
                new_out,
                g.delay_model);
        }
    }

    if (auto it = dff_fanout_.find(changed_net); it != dff_fanout_.end()) {
        for (size_t di : it->second) {
            auto& dff = dffs_[di];
            const StdLogic rst = dff.reset_net.empty() ? StdLogic::Zero : get_net_value(dff.reset_net);
            if (rst == StdLogic::One || rst == StdLogic::H) {
                if (dff.stored_q != StdLogic::Zero) {
                    dff.stored_q = StdLogic::Zero;
                    schedule_driver_event(current_time_ps_ + dff.clk_to_q_ps, dff.q_net, dff.id + "_q", StdLogic::Zero, DelayModel::Transport);
                    if (!dff.qbar_net.empty()) {
                        schedule_driver_event(current_time_ps_ + dff.clk_to_q_ps, dff.qbar_net, dff.id + "_qbar", StdLogic::One, DelayModel::Transport);
                    }
                }
                dff.last_clk = get_net_value(dff.clk_net);
                continue;
            }

            const StdLogic clk_now = get_net_value(dff.clk_net);
            const bool rising =
                (dff.last_clk == StdLogic::Zero || dff.last_clk == StdLogic::L) &&
                (clk_now == StdLogic::One || clk_now == StdLogic::H);
            dff.last_clk = clk_now;

            if (rising) {
                StdLogic d_val = get_net_value(dff.d_net);
                if (d_val == StdLogic::L) d_val = StdLogic::Zero;
                else if (d_val == StdLogic::H) d_val = StdLogic::One;
                else if (d_val != StdLogic::Zero && d_val != StdLogic::One) d_val = StdLogic::X;

                dff.stored_q = d_val;
                schedule_driver_event(current_time_ps_ + dff.clk_to_q_ps, dff.q_net, dff.id + "_q", d_val, DelayModel::Transport);
                if (!dff.qbar_net.empty()) {
                    schedule_driver_event(current_time_ps_ + dff.clk_to_q_ps, dff.qbar_net, dff.id + "_qbar", Ieee1164Resolver::eval_not(d_val), DelayModel::Transport);
                }
            }
        }
    }
}

void DigitalEventEngine::run_until(uint64_t end_time_ps) {
    while (!pq_.empty()) {
        QueuedEvent* top = pq_.top();
        if (top->time_ps > end_time_ps) {
            break;
        }
        pq_.pop();
        if (top->cancelled) {
            continue;
        }
        current_time_ps_ = top->time_ps;
        update_driver_and_resolve(top->target_net, top->driver_id, top->value);
    }
    current_time_ps_ = end_time_ps;
}

StdLogic DigitalEventEngine::get_net_value(std::string_view net) const noexcept {
    auto it = resolved_nets_.find(std::string(net));
    return (it != resolved_nets_.end()) ? it->second : StdLogic::U;
}

const std::vector<WaveformSample>& DigitalEventEngine::waveform(std::string_view net) const noexcept {
    auto it = waveforms_.find(std::string(net));
    return (it != waveforms_.end()) ? it->second : empty_waveform_;
}

void DigitalEventEngine::reset() {
    current_time_ps_ = 0;
    next_seq_ = 1;
    while (!pq_.empty()) pq_.pop();
    event_arena_.clear();
    net_drivers_.clear();
    resolved_nets_.clear();
    waveforms_.clear();
}

} // namespace shunya::pcb
