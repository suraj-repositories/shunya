#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

namespace shunya::core::domain::cosm {

// =============================================================================
// EVT-01: Super-Dense Time & Typed Event Channels
// =============================================================================

struct SuperDenseTime {
    double physical_time{0.0}; // continuous simulation time in seconds
    uint32_t micro_step{0};    // discrete event step index at the same physical time

    [[nodiscard]] constexpr std::partial_ordering operator<=>(const SuperDenseTime& other) const noexcept {
        if (physical_time < other.physical_time) return std::partial_ordering::less;
        if (physical_time > other.physical_time) return std::partial_ordering::greater;
        if (micro_step < other.micro_step) return std::partial_ordering::less;
        if (micro_step > other.micro_step) return std::partial_ordering::greater;
        return std::partial_ordering::equivalent;
    }

    [[nodiscard]] constexpr bool operator==(const SuperDenseTime& other) const noexcept {
        return physical_time == other.physical_time && micro_step == other.micro_step;
    }
};

enum class ChannelType {
    Discrete,
    ContinuousSampling,
    Trigger,
    Diagnostic
};

using EventPayload = std::variant<double, int64_t, bool, std::string, std::vector<double>>;

struct TimedEvent {
    SuperDenseTime time{};
    std::string channel{};
    EventPayload payload{};
    uint8_t priority{128}; // 0 = highest priority, 255 = lowest
};

class EventChannel {
public:
    explicit EventChannel(std::string name, ChannelType type = ChannelType::Discrete)
        : name_(std::move(name)), type_(type) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] ChannelType type() const noexcept { return type_; }

    [[nodiscard]] base::Result<void> publish(const TimedEvent& evt);

    [[nodiscard]] const std::vector<TimedEvent>& events() const noexcept { return events_; }
    void clear() noexcept { events_.clear(); }

    using Subscriber = std::function<void(const TimedEvent&)>;
    void subscribe(Subscriber sub) { subscribers_.push_back(std::move(sub)); }

private:
    std::string name_;
    ChannelType type_;
    std::vector<TimedEvent> events_;
    std::vector<Subscriber> subscribers_;
    std::optional<SuperDenseTime> last_published_time_;
};

// =============================================================================
// EVT-02: Signal Sampling, Interpolation & Rate Conversion
// =============================================================================

enum class InterpolationMethod {
    ZeroOrderHold,
    Linear,
    CubicHermite
};

struct SamplePoint {
    double time{0.0};
    double value{0.0};
    double derivative{0.0};
};

class SampleBuffer {
public:
    explicit SampleBuffer(size_t capacity = 1000) : capacity_(capacity) {}

    void append(double time, double value, double derivative = 0.0);
    [[nodiscard]] std::optional<double> sample(double t, InterpolationMethod method = InterpolationMethod::Linear) const;

    [[nodiscard]] size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] bool empty() const noexcept { return samples_.empty(); }
    void clear() noexcept { samples_.clear(); }

    [[nodiscard]] const std::vector<SamplePoint>& samples() const noexcept { return samples_; }

private:
    size_t capacity_;
    std::vector<SamplePoint> samples_;
};

class SignalRateConverter {
public:
    static std::vector<SamplePoint> resample(
        const std::vector<SamplePoint>& input,
        double target_sample_period,
        InterpolationMethod method = InterpolationMethod::Linear);
};

// =============================================================================
// EVT-03: Domain Coupling Schedule & Communication Points
// =============================================================================

enum class CouplingMode {
    JacobiParallel,   // Loose coupling: slave models exchange values lagged by communication step
    GaussSeidelSerial // Sequential coupling: slave models execute in dependency order
};

struct CouplingScheduleConfig {
    double start_time{0.0};
    double stop_time{1.0};
    double communication_step{0.01}; // macro communication step (e.g. 10 ms)
    CouplingMode mode{CouplingMode::GaussSeidelSerial};
};

class DomainCouplingSchedule {
public:
    explicit DomainCouplingSchedule(CouplingScheduleConfig config) : config_(config) {}

    [[nodiscard]] std::vector<double> generate_communication_points() const;
    [[nodiscard]] const CouplingScheduleConfig& config() const noexcept { return config_; }

private:
    CouplingScheduleConfig config_;
};

// =============================================================================
// EVT-04: Discrete/Continuous Zero-Crossing Detector & Time Re-anchoring
// =============================================================================

class ZeroCrossingDetector {
public:
    using IndicatorFunction = std::function<double(double t, double state)>;

    // Bisects interval [t0, t1] where g(t0) * g(t1) <= 0 to find exact root within tolerance
    static base::Result<double> find_zero_crossing(
        const IndicatorFunction& g,
        double t0,
        double v0,
        double t1,
        double v1,
        double time_tolerance = 1e-9,
        size_t max_iterations = 64);
};

// =============================================================================
// EVT-05: Co-Simulation Slave Adapter Interface
// =============================================================================

class ICoSimSlave {
public:
    virtual ~ICoSimSlave() = default;

    [[nodiscard]] virtual const std::string& name() const noexcept = 0;
    [[nodiscard]] virtual base::Result<void> initialize(double start_time, double stop_time) = 0;
    [[nodiscard]] virtual base::Result<void> do_step(double current_time, double step_size) = 0;
    [[nodiscard]] virtual base::Result<void> set_real(const std::string& port, double val) = 0;
    [[nodiscard]] virtual base::Result<double> get_real(const std::string& port) const = 0;
    [[nodiscard]] virtual base::Result<void> terminate() = 0;
};

class MockCoSimSlave : public ICoSimSlave {
public:
    explicit MockCoSimSlave(std::string name, double direct_feedthrough_gain = 0.0)
        : name_(std::move(name)), feedthrough_gain_(direct_feedthrough_gain) {}

    [[nodiscard]] const std::string& name() const noexcept override { return name_; }

    [[nodiscard]] base::Result<void> initialize(double start_time, double stop_time) override;
    [[nodiscard]] base::Result<void> do_step(double current_time, double step_size) override;
    [[nodiscard]] base::Result<void> set_real(const std::string& port, double val) override;
    [[nodiscard]] base::Result<double> get_real(const std::string& port) const override;
    [[nodiscard]] base::Result<void> terminate() override;

    void set_direct_feedthrough(double gain) { feedthrough_gain_ = gain; }
    [[nodiscard]] double direct_feedthrough() const noexcept { return feedthrough_gain_; }
    [[nodiscard]] double internal_state() const noexcept { return state_; }
    void set_state(double s) noexcept { state_ = s; }

private:
    std::string name_;
    double feedthrough_gain_{0.0};
    double current_time_{0.0};
    double state_{0.0};
    std::unordered_map<std::string, double> inputs_;
    std::unordered_map<std::string, double> outputs_;
};

// =============================================================================
// EVT-06: Co-Simulation Orchestrator, Algebraic Loop Resolution & Replay
// =============================================================================

struct PortConnection {
    std::string source_slave;
    std::string source_port;
    std::string target_slave;
    std::string target_port;
};

struct ReplayRecord {
    double communication_time{0.0};
    std::vector<std::pair<std::string, double>> exchanged_values;
};

class CoSimulationOrchestrator {
public:
    explicit CoSimulationOrchestrator(DomainCouplingSchedule schedule)
        : schedule_(std::move(schedule)) {}

    void add_slave(std::shared_ptr<ICoSimSlave> slave);
    [[nodiscard]] base::Result<void> connect_ports(PortConnection conn);

    [[nodiscard]] base::Result<void> initialize();
    [[nodiscard]] base::Result<void> run_simulation();

    // Solves algebraic loops at communication points via fixed-point Picard iteration
    [[nodiscard]] base::Result<void> resolve_algebraic_loops(double current_time, double tol = 1e-6, size_t max_iters = 50);

    [[nodiscard]] const std::vector<ReplayRecord>& replay_log() const noexcept { return replay_log_; }
    [[nodiscard]] bool verify_deterministic_replay(const std::vector<ReplayRecord>& expected_trace, double tol = 1e-9) const;

private:
    DomainCouplingSchedule schedule_;
    std::vector<std::shared_ptr<ICoSimSlave>> slaves_;
    std::vector<PortConnection> connections_;
    std::vector<ReplayRecord> replay_log_;
};

} // namespace shunya::core::domain::cosm
