#include "shunya/core/domain/co_simulation.hpp"

#include <cmath>

namespace shunya::core::domain::cosm {

// =============================================================================
// EVT-01: EventChannel
// =============================================================================

base::Result<void> EventChannel::publish(const TimedEvent& evt) {
    if (last_published_time_.has_value() && evt.time < *last_published_time_) {
        return base::Result<void>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::Simulation,
            .message = "Event timestamp violates monotonicity on channel " + name_,
            .details = {}
        });
    }

    last_published_time_ = evt.time;
    events_.push_back(evt);

    for (auto& sub : subscribers_) {
        if (sub) {
            sub(evt);
        }
    }
    return base::Result<void>::ok();
}

// =============================================================================
// EVT-02: SampleBuffer & SignalRateConverter
// =============================================================================

void SampleBuffer::append(double time, double value, double derivative) {
    if (samples_.size() >= capacity_) {
        samples_.erase(samples_.begin());
    }
    samples_.push_back(SamplePoint{time, value, derivative});
}

std::optional<double> SampleBuffer::sample(double t, InterpolationMethod method) const {
    if (samples_.empty()) {
        return std::nullopt;
    }
    if (t <= samples_.front().time) {
        return samples_.front().value;
    }
    if (t >= samples_.back().time) {
        return samples_.back().value;
    }

    // Binary search for interval
    auto it = std::upper_bound(samples_.begin(), samples_.end(), t,
        [](double val, const SamplePoint& p) {
            return val < p.time;
        });

    if (it == samples_.end() || it == samples_.begin()) {
        return samples_.back().value;
    }

    const auto& p1 = *it;
    const auto& p0 = *(it - 1);

    if (method == InterpolationMethod::ZeroOrderHold) {
        return p0.value;
    }

    double dt = p1.time - p0.time;
    if (std::abs(dt) < 1e-14) {
        return p0.value;
    }

    double alpha = (t - p0.time) / dt;

    if (method == InterpolationMethod::Linear) {
        return p0.value + alpha * (p1.value - p0.value);
    }

    // Cubic Hermite
    double s = alpha;
    double s2 = s * s;
    double s3 = s2 * s;

    double h00 = 2.0 * s3 - 3.0 * s2 + 1.0;
    double h10 = s3 - 2.0 * s2 + s;
    double h01 = -2.0 * s3 + 3.0 * s2;
    double h11 = s3 - s2;

    double val = h00 * p0.value + h10 * dt * p0.derivative +
                 h01 * p1.value + h11 * dt * p1.derivative;
    return val;
}

std::vector<SamplePoint> SignalRateConverter::resample(
    const std::vector<SamplePoint>& input,
    double target_sample_period,
    InterpolationMethod method) {
    if (input.empty() || target_sample_period <= 0.0) {
        return {};
    }

    SampleBuffer buffer(input.size());
    for (const auto& p : input) {
        buffer.append(p.time, p.value, p.derivative);
    }

    std::vector<SamplePoint> output;
    double t_start = input.front().time;
    double t_end = input.back().time;

    for (double t = t_start; t <= t_end + 1e-12; t += target_sample_period) {
        auto val = buffer.sample(t, method);
        if (val.has_value()) {
            output.push_back(SamplePoint{t, *val, 0.0});
        }
    }
    return output;
}

// =============================================================================
// EVT-03: DomainCouplingSchedule
// =============================================================================

std::vector<double> DomainCouplingSchedule::generate_communication_points() const {
    std::vector<double> points;
    if (config_.communication_step <= 0.0 || config_.stop_time < config_.start_time) {
        return points;
    }

    for (double t = config_.start_time; t <= config_.stop_time + 1e-12; t += config_.communication_step) {
        points.push_back(t);
    }
    return points;
}

// =============================================================================
// EVT-04: ZeroCrossingDetector
// =============================================================================

base::Result<double> ZeroCrossingDetector::find_zero_crossing(
    const IndicatorFunction& g,
    double t0,
    double v0,
    double t1,
    double v1,
    double time_tolerance,
    size_t max_iterations) {
    if (!g) {
        return base::Result<double>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::Simulation,
            .message = "Indicator function not provided",
            .details = {}
        });
    }

    double g0 = g(t0, v0);
    double g1 = g(t1, v1);

    if (std::abs(g0) < 1e-12) {
        return base::Result<double>::ok(t0);
    }
    if (std::abs(g1) < 1e-12) {
        return base::Result<double>::ok(t1);
    }

    if (g0 * g1 > 0.0) {
        return base::Result<double>::err(base::Error{
            .code = 2,
            .domain = base::ErrorDomain::Simulation,
            .message = "No sign change detected in interval",
            .details = {}
        });
    }

    double a = t0;
    double b = t1;
    double va = v0;
    double vb = v1;

    for (size_t iter = 0; iter < max_iterations; ++iter) {
        double mid = 0.5 * (a + b);
        double v_mid = va + 0.5 * (vb - va);
        double g_mid = g(mid, v_mid);

        if (std::abs(g_mid) < 1e-12 || (b - a) < time_tolerance) {
            return base::Result<double>::ok(mid);
        }

        if (g0 * g_mid <= 0.0) {
            b = mid;
            vb = v_mid;
            g1 = g_mid;
        } else {
            a = mid;
            va = v_mid;
            g0 = g_mid;
        }
    }

    return base::Result<double>::ok(0.5 * (a + b));
}

// =============================================================================
// EVT-05: MockCoSimSlave
// =============================================================================

base::Result<void> MockCoSimSlave::initialize(double start_time, double /*stop_time*/) {
    current_time_ = start_time;
    state_ = 0.0;
    inputs_.clear();
    outputs_.clear();
    return base::Result<void>::ok();
}

base::Result<void> MockCoSimSlave::do_step(double current_time, double step_size) {
    current_time_ = current_time + step_size;
    double in_val = inputs_["u"];
    // Euler step on internal dynamics: d(state)/dt = in_val
    state_ += in_val * step_size;
    // Output calculation: y = state + feedthrough * input
    outputs_["y"] = state_ + feedthrough_gain_ * in_val;
    return base::Result<void>::ok();
}

base::Result<void> MockCoSimSlave::set_real(const std::string& port, double val) {
    inputs_[port] = val;
    // If output is feedthrough, update immediately
    if (port == "u") {
        outputs_["y"] = state_ + feedthrough_gain_ * val;
    }
    return base::Result<void>::ok();
}

base::Result<double> MockCoSimSlave::get_real(const std::string& port) const {
    auto it = outputs_.find(port);
    if (it != outputs_.end()) {
        return base::Result<double>::ok(it->second);
    }
    auto in_it = inputs_.find(port);
    if (in_it != inputs_.end()) {
        return base::Result<double>::ok(in_it->second);
    }
    return base::Result<double>::ok(0.0);
}

base::Result<void> MockCoSimSlave::terminate() {
    inputs_.clear();
    outputs_.clear();
    return base::Result<void>::ok();
}

// =============================================================================
// EVT-06: CoSimulationOrchestrator
// =============================================================================

void CoSimulationOrchestrator::add_slave(std::shared_ptr<ICoSimSlave> slave) {
    if (slave) {
        slaves_.push_back(std::move(slave));
    }
}

base::Result<void> CoSimulationOrchestrator::connect_ports(PortConnection conn) {
    connections_.push_back(std::move(conn));
    return base::Result<void>::ok();
}

base::Result<void> CoSimulationOrchestrator::initialize() {
    replay_log_.clear();
    const auto& cfg = schedule_.config();
    for (auto& slave : slaves_) {
        auto res = slave->initialize(cfg.start_time, cfg.stop_time);
        if (res.is_err()) {
            return res;
        }
    }
    return base::Result<void>::ok();
}

base::Result<void> CoSimulationOrchestrator::resolve_algebraic_loops(double /*current_time*/, double tol, size_t max_iters) {
    if (connections_.empty()) {
        return base::Result<void>::ok();
    }

    std::unordered_map<std::string, ICoSimSlave*> slave_map;
    for (auto& s : slaves_) {
        slave_map[s->name()] = s.get();
    }

    // Fixed point iteration for direct feedthrough algebraic loops
    for (size_t iter = 0; iter < max_iters; ++iter) {
        double max_delta = 0.0;
        for (const auto& conn : connections_) {
            auto src_it = slave_map.find(conn.source_slave);
            auto tgt_it = slave_map.find(conn.target_slave);
            if (src_it == slave_map.end() || tgt_it == slave_map.end()) {
                continue;
            }

            auto out_val = src_it->second->get_real(conn.source_port);
            if (out_val.is_err()) continue;

            auto old_in = tgt_it->second->get_real(conn.target_port);
            double delta = 0.0;
            if (old_in.is_ok()) {
                delta = std::abs(out_val.value() - old_in.value());
            }

            max_delta = std::max(max_delta, delta);
            (void)tgt_it->second->set_real(conn.target_port, out_val.value());
        }

        if (max_delta < tol) {
            return base::Result<void>::ok();
        }
    }

    return base::Result<void>::ok();
}

base::Result<void> CoSimulationOrchestrator::run_simulation() {
    auto comm_points = schedule_.generate_communication_points();
    if (comm_points.size() < 2) {
        return base::Result<void>::ok();
    }

    for (size_t i = 0; i < comm_points.size() - 1; ++i) {
        double t_curr = comm_points[i];
        double dt = comm_points[i + 1] - t_curr;

        // 1. Resolve algebraic coupling at communication point
        auto loop_res = resolve_algebraic_loops(t_curr);
        if (loop_res.is_err()) {
            return loop_res;
        }

        // 2. Record replay log
        ReplayRecord record;
        record.communication_time = t_curr;
        for (const auto& conn : connections_) {
            for (auto& s : slaves_) {
                if (s->name() == conn.source_slave) {
                    auto v = s->get_real(conn.source_port);
                    if (v.is_ok()) {
                        record.exchanged_values.emplace_back(conn.source_slave + "." + conn.source_port, v.value());
                    }
                }
            }
        }
        replay_log_.push_back(std::move(record));

        // 3. Advance each slave by macro step dt
        for (auto& slave : slaves_) {
            auto step_res = slave->do_step(t_curr, dt);
            if (step_res.is_err()) {
                return step_res;
            }
        }
    }

    return base::Result<void>::ok();
}

bool CoSimulationOrchestrator::verify_deterministic_replay(
    const std::vector<ReplayRecord>& expected_trace, double tol) const {
    if (replay_log_.size() != expected_trace.size()) {
        return false;
    }

    for (size_t i = 0; i < replay_log_.size(); ++i) {
        const auto& rec1 = replay_log_[i];
        const auto& rec2 = expected_trace[i];

        if (std::abs(rec1.communication_time - rec2.communication_time) > tol) {
            return false;
        }
        if (rec1.exchanged_values.size() != rec2.exchanged_values.size()) {
            return false;
        }
        for (size_t j = 0; j < rec1.exchanged_values.size(); ++j) {
            if (rec1.exchanged_values[j].first != rec2.exchanged_values[j].first) {
                return false;
            }
            if (std::abs(rec1.exchanged_values[j].second - rec2.exchanged_values[j].second) > tol) {
                return false;
            }
        }
    }
    return true;
}

} // namespace shunya::core::domain::cosm
