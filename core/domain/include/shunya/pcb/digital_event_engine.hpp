#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/digital_logic.hpp"
#include "shunya/pcb/ieee1164_resolver.hpp"

#include <cstdint>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::pcb {

enum class DelayModel : uint8_t {
    Inertial,
    Transport
};

struct DigitalGateElement {
    std::string id;
    core::domain::GateType type{core::domain::GateType::And};
    std::vector<std::string> inputs;
    std::string output_net;
    uint64_t rise_delay_ps{100};
    uint64_t fall_delay_ps{100};
    DelayModel delay_model{DelayModel::Inertial};
};

struct DigitalDffElement {
    std::string id;
    std::string clk_net;
    std::string d_net;
    std::string q_net;
    std::string qbar_net;
    std::string reset_net; // Active high async reset if non-empty
    uint64_t clk_to_q_ps{150};
    StdLogic stored_q{StdLogic::Zero};
    StdLogic last_clk{StdLogic::Zero};
};

struct WaveformSample {
    uint64_t time_ps{0};
    StdLogic value{StdLogic::U};
};

/**
 * @brief IEEE 1164 discrete-event digital logic simulator with inertial/transport delays and multi-driver bus resolution.
 */
class DigitalEventEngine {
public:
    DigitalEventEngine() = default;

    void set_initial_net(const std::string& net, StdLogic val, const std::string& driver_id = "__ext");
    void add_gate(DigitalGateElement gate);
    void add_dff(DigitalDffElement dff);

    void schedule_driver_event(
        uint64_t time_ps,
        std::string target_net,
        std::string driver_id,
        StdLogic new_val,
        DelayModel model = DelayModel::Transport);

    void schedule_external_pulse(
        uint64_t start_ps,
        uint64_t width_ps,
        const std::string& net,
        StdLogic pulse_val,
        StdLogic return_val,
        const std::string& driver_id = "__ext");

    void run_until(uint64_t end_time_ps);

    [[nodiscard]] StdLogic get_net_value(std::string_view net) const noexcept;
    [[nodiscard]] uint64_t current_time_ps() const noexcept { return current_time_ps_; }
    [[nodiscard]] const std::vector<WaveformSample>& waveform(std::string_view net) const noexcept;
    void reset();

private:
    struct QueuedEvent {
        uint64_t time_ps{0};
        uint64_t seq{0};
        std::string target_net;
        std::string driver_id;
        StdLogic value{StdLogic::U};
        bool cancelled{false};
    };

    struct EventPtrGreater {
        bool operator()(const QueuedEvent* a, const QueuedEvent* b) const noexcept {
            if (a->time_ps != b->time_ps) return a->time_ps > b->time_ps;
            return a->seq > b->seq;
        }
    };

    void update_driver_and_resolve(const std::string& net, const std::string& driver_id, StdLogic val);
    void evaluate_fanout(const std::string& changed_net);

    uint64_t current_time_ps_{0};
    uint64_t next_seq_{1};
    std::vector<std::unique_ptr<QueuedEvent>> event_arena_;
    std::priority_queue<QueuedEvent*, std::vector<QueuedEvent*>, EventPtrGreater> pq_;

    // net -> (driver_id -> latest driver value)
    std::unordered_map<std::string, std::unordered_map<std::string, StdLogic>> net_drivers_;
    std::unordered_map<std::string, StdLogic> resolved_nets_;
    std::unordered_map<std::string, std::vector<WaveformSample>> waveforms_;

    std::vector<DigitalGateElement> gates_;
    std::vector<DigitalDffElement> dffs_;
    std::unordered_map<std::string, std::vector<size_t>> gate_fanout_;
    std::unordered_map<std::string, std::vector<size_t>> dff_fanout_;
    static const std::vector<WaveformSample> empty_waveform_;
};

} // namespace shunya::pcb
