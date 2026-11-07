#include "shunya/core/domain/simulation_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_sim_error(std::string msg, int32_t code = 2600) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Simulation,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

std::string get_iso_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

uint32_t compute_crc32(std::string_view data) {
    uint32_t crc = 0xFFFFFFFF;
    for (unsigned char b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

std::string compute_hash_hex(std::string_view data) {
    uint32_t crc = compute_crc32(data);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << crc;
    return ss.str();
}

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string unescape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '"': out += '"'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case '/': out += '/'; ++i; break;
                case 'b': out += '\b'; ++i; break;
                case 'f': out += '\f'; ++i; break;
                case 'n': out += '\n'; ++i; break;
                case 'r': out += '\r'; ++i; break;
                case 't': out += '\t'; ++i; break;
                default: out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace

// =============================================================================
// SIM-01: Simulation Lifecycle States
// =============================================================================

std::optional<SimulationState> simulation_state_from_string(std::string_view str) noexcept {
    if (str == "Stopped") return SimulationState::Stopped;
    if (str == "Configuring") return SimulationState::Configuring;
    if (str == "Running") return SimulationState::Running;
    if (str == "Paused") return SimulationState::Paused;
    if (str == "Stepping") return SimulationState::Stepping;
    if (str == "Completed") return SimulationState::Completed;
    if (str == "Failed") return SimulationState::Failed;
    if (str == "Cancelled") return SimulationState::Cancelled;
    return std::nullopt;
}

bool can_transition(SimulationState from, SimulationState to) noexcept {
    if (from == to) return true;

    switch (from) {
        case SimulationState::Stopped:
            return to == SimulationState::Configuring || to == SimulationState::Running;

        case SimulationState::Configuring:
            return to == SimulationState::Running ||
                   to == SimulationState::Paused ||
                   to == SimulationState::Stopped;

        case SimulationState::Running:
            return to == SimulationState::Paused ||
                   to == SimulationState::Stepping ||
                   to == SimulationState::Completed ||
                   to == SimulationState::Failed ||
                   to == SimulationState::Cancelled ||
                   to == SimulationState::Stopped;

        case SimulationState::Paused:
            return to == SimulationState::Running ||
                   to == SimulationState::Stepping ||
                   to == SimulationState::Stopped ||
                   to == SimulationState::Cancelled;

        case SimulationState::Stepping:
            return to == SimulationState::Running ||
                   to == SimulationState::Paused ||
                   to == SimulationState::Completed ||
                   to == SimulationState::Failed ||
                   to == SimulationState::Cancelled ||
                   to == SimulationState::Stopped;

        case SimulationState::Completed:
        case SimulationState::Failed:
        case SimulationState::Cancelled:
            return to == SimulationState::Stopped || to == SimulationState::Configuring;
    }
    return false;
}

// =============================================================================
// SIM-02: Configuration Parsing & Serialization
// =============================================================================

std::optional<TimeStepPolicy> time_step_policy_from_string(std::string_view str) noexcept {
    if (str == "Fixed") return TimeStepPolicy::Fixed;
    if (str == "Adaptive") return TimeStepPolicy::Adaptive;
    return std::nullopt;
}

std::optional<SolverType> solver_type_from_string(std::string_view str) noexcept {
    if (str == "EulerExplicit") return SolverType::EulerExplicit;
    if (str == "RungeKutta2") return SolverType::RungeKutta2;
    if (str == "RungeKutta4") return SolverType::RungeKutta4;
    if (str == "AdaptiveRK45") return SolverType::AdaptiveRK45;
    if (str == "Verlet") return SolverType::Verlet;
    if (str == "ImplicitEuler") return SolverType::ImplicitEuler;
    if (str == "Custom") return SolverType::Custom;
    return std::nullopt;
}

base::ValidationReport SimulationConfig::validate() const {
    base::ValidationReport report;

    if (end_time <= start_time) {
        report.add_error("end_time", "end_time must be strictly greater than start_time", 2601);
    }
    if (initial_time_step <= 0.0) {
        report.add_error("initial_time_step", "initial_time_step must be positive", 2602);
    }
    if (min_time_step <= 0.0) {
        report.add_error("min_time_step", "min_time_step must be positive", 2603);
    }
    if (max_time_step < min_time_step) {
        report.add_error("max_time_step", "max_time_step cannot be less than min_time_step", 2604);
    }
    if (initial_time_step < min_time_step || initial_time_step > max_time_step) {
        report.add_error("initial_time_step", "initial_time_step must be within [min_time_step, max_time_step]", 2605);
    }
    if (relative_tolerance <= 0.0) {
        report.add_error("relative_tolerance", "relative_tolerance must be positive", 2606);
    }
    if (absolute_tolerance <= 0.0) {
        report.add_error("absolute_tolerance", "absolute_tolerance must be positive", 2607);
    }
    if (output_sampling_interval <= 0.0) {
        report.add_error("output_sampling_interval", "output_sampling_interval must be positive", 2608);
    }
    if (max_steps == 0) {
        report.add_error("max_steps", "max_steps must be greater than zero", 2609);
    }

    return report;
}

std::string SimulationConfig::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"start_time\": " << start_time << ",\n";
    ss << "  \"end_time\": " << end_time << ",\n";
    ss << "  \"time_step_policy\": \"" << time_step_policy_to_string(time_step_policy) << "\",\n";
    ss << "  \"initial_time_step\": " << initial_time_step << ",\n";
    ss << "  \"min_time_step\": " << min_time_step << ",\n";
    ss << "  \"max_time_step\": " << max_time_step << ",\n";
    ss << "  \"solver_type\": \"" << solver_type_to_string(solver_type) << "\",\n";
    ss << "  \"relative_tolerance\": " << relative_tolerance << ",\n";
    ss << "  \"absolute_tolerance\": " << absolute_tolerance << ",\n";
    ss << "  \"random_seed\": " << random_seed << ",\n";
    ss << "  \"output_sampling_interval\": " << output_sampling_interval << ",\n";
    ss << "  \"max_steps\": " << max_steps << "\n";
    ss << "}";
    return ss.str();
}

base::Result<SimulationConfig> SimulationConfig::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::optional<std::string> {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str();
        return std::nullopt;
    };

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stod(m[1].str());
        return std::nullopt;
    };

    auto extract_u64 = [&](const std::string& key) -> std::optional<uint64_t> {
        std::regex re("\"" + key + R"re("\s*:\s*([0-9]+))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stoull(m[1].str());
        return std::nullopt;
    };

    SimulationConfig cfg;
    if (auto v = extract_num("start_time")) cfg.start_time = *v;
    if (auto v = extract_num("end_time")) cfg.end_time = *v;
    if (auto v = extract_str("time_step_policy")) {
        if (auto p = time_step_policy_from_string(*v)) cfg.time_step_policy = *p;
    }
    if (auto v = extract_num("initial_time_step")) cfg.initial_time_step = *v;
    if (auto v = extract_num("min_time_step")) cfg.min_time_step = *v;
    if (auto v = extract_num("max_time_step")) cfg.max_time_step = *v;
    if (auto v = extract_str("solver_type")) {
        if (auto sol = solver_type_from_string(*v)) cfg.solver_type = *sol;
    }
    if (auto v = extract_num("relative_tolerance")) cfg.relative_tolerance = *v;
    if (auto v = extract_num("absolute_tolerance")) cfg.absolute_tolerance = *v;
    if (auto v = extract_u64("random_seed")) cfg.random_seed = *v;
    if (auto v = extract_num("output_sampling_interval")) cfg.output_sampling_interval = *v;
    if (auto v = extract_u64("max_steps")) cfg.max_steps = *v;

    auto report = cfg.validate();
    if (!report.is_valid()) {
        return make_sim_error(report.issues()[0].message, 2600);
    }

    return cfg;
}

// =============================================================================
// SIM-05: TimeStepController
// =============================================================================

TimeStepController::TimeStepController(const SimulationConfig& config)
    : config_(config) {
    diagnostics_.current_dt = config_.initial_time_step;
}

void TimeStepController::configure(const SimulationConfig& config) {
    config_ = config;
    diagnostics_.current_dt = config_.initial_time_step;
}

double TimeStepController::compute_next_dt(double current_dt, double suggested_dt, double local_error) noexcept {
    if (config_.time_step_policy == TimeStepPolicy::Fixed) {
        diagnostics_.current_dt = config_.initial_time_step;
        return config_.initial_time_step;
    }

    double next_dt = current_dt > 0.0 ? current_dt : config_.initial_time_step;

    if (suggested_dt > 0.0) {
        next_dt = suggested_dt;
    } else if (local_error > 0.0) {
        // Standard adaptive step sizing based on error norm
        const double tol = config_.relative_tolerance;
        double factor = 0.9 * std::pow(tol / local_error, 0.2);
        factor = std::clamp(factor, 0.2, 2.0);
        next_dt = current_dt * factor;
    }

    next_dt = std::clamp(next_dt, config_.min_time_step, config_.max_time_step);
    diagnostics_.current_dt = next_dt;
    return next_dt;
}

base::Result<void> TimeStepController::handle_rejected_step(double& dt, std::string_view reason) {
    ++diagnostics_.total_steps_attempted;
    ++diagnostics_.steps_rejected;
    diagnostics_.last_rejection_reason = std::string(reason);

    // Reduce step by half
    const double candidate_dt = dt * 0.5;
    if (candidate_dt < config_.min_time_step) {
        return make_sim_error(
            "Rejected step: required time step (" + std::to_string(candidate_dt) +
            ") falls below min_time_step (" + std::to_string(config_.min_time_step) + ")",
            2605
        );
    }

    dt = candidate_dt;
    diagnostics_.current_dt = dt;
    return {};
}

void TimeStepController::record_accepted_step(double dt) noexcept {
    ++diagnostics_.total_steps_attempted;
    ++diagnostics_.steps_accepted;
    diagnostics_.current_dt = dt;
    if (dt < diagnostics_.min_dt_encountered) diagnostics_.min_dt_encountered = dt;
    if (dt > diagnostics_.max_dt_encountered) diagnostics_.max_dt_encountered = dt;
}

void TimeStepController::reset() noexcept {
    diagnostics_.reset();
    diagnostics_.current_dt = config_.initial_time_step;
}

// =============================================================================
// SIM-06: SimulationCheckpoint
// =============================================================================

std::string SimulationCheckpoint::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"checkpoint_id\": \"" << checkpoint_id.to_string() << "\",\n";
    ss << "  \"physical_time\": " << physical_time << ",\n";
    ss << "  \"step_count\": " << step_count << ",\n";
    ss << "  \"wall_timestamp\": \"" << escape_json(wall_timestamp) << "\",\n";
    ss << "  \"participant_states\": {\n";
    size_t i = 0;
    for (const auto& [name, state_json] : participant_states) {
        ss << "    \"" << escape_json(name) << "\": \"" << escape_json(state_json) << "\"";
        if (++i < participant_states.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

base::Result<SimulationCheckpoint> SimulationCheckpoint::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::optional<std::string> {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str();
        return std::nullopt;
    };

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stod(m[1].str());
        return std::nullopt;
    };

    auto extract_u64 = [&](const std::string& key) -> std::optional<uint64_t> {
        std::regex re("\"" + key + R"re("\s*:\s*([0-9]+))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stoull(m[1].str());
        return std::nullopt;
    };

    SimulationCheckpoint cp;
    if (auto v = extract_str("checkpoint_id")) {
        auto uuid_res = base::Uuid::from_string(*v);
        if (uuid_res.has_value()) cp.checkpoint_id = uuid_res.value();
    }
    if (auto v = extract_num("physical_time")) cp.physical_time = *v;
    if (auto v = extract_u64("step_count")) cp.step_count = *v;
    if (auto v = extract_str("wall_timestamp")) cp.wall_timestamp = unescape_json(*v);

    // Parse participant states map
    std::regex pair_re(R"re("([^"]+)"\s*:\s*"([^"]*)")re");
    auto it_begin = std::sregex_iterator(s.begin(), s.end(), pair_re);
    auto it_end = std::sregex_iterator();
    for (auto it = it_begin; it != it_end; ++it) {
        std::string key = (*it)[1].str();
        if (key != "checkpoint_id" && key != "wall_timestamp") {
            cp.participant_states[key] = unescape_json((*it)[2].str());
        }
    }

    return cp;
}

// =============================================================================
// SIM-07: SimulationEventQueue
// =============================================================================

void SimulationEventQueue::sort_events() {
    std::sort(events_.begin(), events_.end(), [](const SimulationEvent& a, const SimulationEvent& b) {
        if (std::abs(a.scheduled_time - b.scheduled_time) > 1e-9) {
            return a.scheduled_time < b.scheduled_time;
        }
        return a.priority > b.priority;
    });
}

void SimulationEventQueue::schedule_event(SimulationEvent event) {
    events_.push_back(std::move(event));
    sort_events();
}

void SimulationEventQueue::post_user_event(std::string name, std::function<base::Result<void>(double)> handler, double current_time) {
    SimulationEvent ev;
    ev.name = std::move(name);
    ev.scheduled_time = current_time;
    ev.priority = 100; // elevated user priority
    ev.is_recurring = false;
    ev.handler = std::move(handler);
    schedule_event(std::move(ev));
}

std::optional<double> SimulationEventQueue::next_event_time() const noexcept {
    if (events_.empty()) return std::nullopt;
    return events_.front().scheduled_time;
}

bool SimulationEventQueue::has_pending_events_up_to(double t) const noexcept {
    if (events_.empty()) return false;
    return events_.front().scheduled_time <= (t + 1e-9);
}

base::Result<size_t> SimulationEventQueue::dispatch_events_up_to(double t) {
    size_t count = 0;
    std::vector<SimulationEvent> recurring_rescheduled;

    while (!events_.empty() && events_.front().scheduled_time <= (t + 1e-9)) {
        SimulationEvent ev = std::move(events_.front());
        events_.erase(events_.begin());

        if (ev.handler) {
            auto res = ev.handler(ev.scheduled_time);
            if (!res.is_ok()) {
                return make_sim_error("Event '" + ev.name + "' execution failed: " + res.error().message, res.error().code);
            }
        }
        ++count;

        if (ev.is_recurring && ev.recurrence_interval > 0.0) {
            ev.scheduled_time += ev.recurrence_interval;
            recurring_rescheduled.push_back(std::move(ev));
        }
    }

    for (auto& ev : recurring_rescheduled) {
        events_.push_back(std::move(ev));
    }
    if (!recurring_rescheduled.empty()) {
        sort_events();
    }

    return count;
}

double SimulationEventQueue::calculate_substep_for_events(double current_t, double target_dt) const noexcept {
    if (events_.empty()) return target_dt;
    const double next_t = events_.front().scheduled_time;
    if (next_t > current_t + 1e-9 && next_t < current_t + target_dt) {
        return next_t - current_t;
    }
    return target_dt;
}

// =============================================================================
// SIM-08: Run Isolation
// =============================================================================

base::Result<std::unique_ptr<IsolatedRunEnvironment>> IsolatedRunEnvironment::create(
    const Project& project,
    const std::vector<std::shared_ptr<ComponentRuntimeInstance>>& components
) {
    auto env = std::make_unique<IsolatedRunEnvironment>();
    env->project_id_ = project.manifest().id;
    env->project_name_ = project.manifest().name;
    env->project_version_ = project.manifest().schema_version;
    env->project_json_snapshot_ = project.manifest().to_json();
    env->content_hash_ = compute_hash_hex(env->project_json_snapshot_);

    for (const auto& comp : components) {
        if (comp) {
            // Clone component runtime instance state
            auto cloned = std::make_shared<ComponentRuntimeInstance>(
                comp->definition(), comp->instance_name()
            );
            (void)cloned->restore_state_json(comp->serialize_state_json());
            env->isolated_components_.push_back(std::move(cloned));
        }
    }

    return env;
}

// =============================================================================
// SIM-09: Run Manifest
// =============================================================================

std::string RunManifest::compute_manifest_hash() const {
    std::ostringstream ss;
    ss << run_id.to_string() << ":" << project_id.to_string() << ":"
       << project_content_hash << ":" << solver_name << ":" << seed << ":"
       << start_time_iso << ":" << simulation_state_to_string(final_state);
    return compute_hash_hex(ss.str());
}

std::string RunManifest::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"run_id\": \"" << run_id.to_string() << "\",\n";
    ss << "  \"project_id\": \"" << project_id.to_string() << "\",\n";
    ss << "  \"project_content_hash\": \"" << project_content_hash << "\",\n";
    ss << "  \"solver_name\": \"" << solver_name << "\",\n";
    ss << "  \"seed\": " << seed << ",\n";
    ss << "  \"start_time_iso\": \"" << start_time_iso << "\",\n";
    ss << "  \"completion_time_iso\": \"" << completion_time_iso << "\",\n";
    ss << "  \"final_state\": \"" << simulation_state_to_string(final_state) << "\",\n";
    ss << "  \"host_os\": \"" << escape_json(host_os) << "\",\n";
    ss << "  \"compiler_info\": \"" << escape_json(compiler_info) << "\",\n";
    ss << "  \"engine_version\": \"" << engine_version.as_string() << "\",\n";
    ss << "  \"manifest_hash\": \"" << compute_manifest_hash() << "\",\n";
    ss << "  \"diagnostics\": {\n";
    ss << "    \"total_steps_attempted\": " << diagnostics.total_steps_attempted << ",\n";
    ss << "    \"steps_accepted\": " << diagnostics.steps_accepted << ",\n";
    ss << "    \"steps_rejected\": " << diagnostics.steps_rejected << ",\n";
    ss << "    \"current_dt\": " << diagnostics.current_dt << "\n";
    ss << "  },\n";
    ss << "  \"config\": " << config.to_json() << "\n";
    ss << "}";
    return ss.str();
}

base::Result<RunManifest> RunManifest::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::optional<std::string> {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str();
        return std::nullopt;
    };

    auto extract_u64 = [&](const std::string& key) -> std::optional<uint64_t> {
        std::regex re("\"" + key + R"re("\s*:\s*([0-9]+))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stoull(m[1].str());
        return std::nullopt;
    };

    RunManifest manifest;
    if (auto v = extract_str("run_id")) {
        auto uuid_res = base::Uuid::from_string(*v);
        if (uuid_res.has_value()) manifest.run_id = uuid_res.value();
    }
    if (auto v = extract_str("project_id")) {
        auto uuid_res = base::Uuid::from_string(*v);
        if (uuid_res.has_value()) manifest.project_id = uuid_res.value();
    }
    if (auto v = extract_str("project_content_hash")) manifest.project_content_hash = *v;
    if (auto v = extract_str("solver_name")) manifest.solver_name = *v;
    if (auto v = extract_u64("seed")) manifest.seed = *v;
    if (auto v = extract_str("start_time_iso")) manifest.start_time_iso = *v;
    if (auto v = extract_str("completion_time_iso")) manifest.completion_time_iso = *v;
    if (auto v = extract_str("final_state")) {
        if (auto st = simulation_state_from_string(*v)) manifest.final_state = *st;
    }
    if (auto v = extract_str("host_os")) manifest.host_os = unescape_json(*v);
    if (auto v = extract_str("compiler_info")) manifest.compiler_info = unescape_json(*v);
    if (auto v = extract_str("engine_version")) {
        auto ver_res = base::Version::parse(*v);
        if (ver_res.is_ok()) manifest.engine_version = ver_res.value();
    }

    if (auto v = extract_u64("total_steps_attempted")) manifest.diagnostics.total_steps_attempted = *v;
    if (auto v = extract_u64("steps_accepted")) manifest.diagnostics.steps_accepted = *v;
    if (auto v = extract_u64("steps_rejected")) manifest.diagnostics.steps_rejected = *v;

    // Extract nested config
    std::regex config_re(R"re("config"\s*:\s*(\{[\s\S]*\}))re");
    std::smatch m;
    if (std::regex_search(s, m, config_re)) {
        auto cfg_res = SimulationConfig::from_json(m[1].str());
        if (cfg_res.is_ok()) manifest.config = cfg_res.value();
    }

    return manifest;
}

// =============================================================================
// SimulationRuntime Orchestrator Implementation
// =============================================================================

SimulationRuntime::SimulationRuntime()
    : state_(SimulationState::Stopped), config_(), clock_(), step_controller_(config_) {}

SimulationRuntime::SimulationRuntime(SimulationConfig config)
    : state_(SimulationState::Stopped), config_(std::move(config)), clock_(config_.start_time), step_controller_(config_) {}

SimulationRuntime::~SimulationRuntime() {
    (void)stop();
}

base::Result<void> SimulationRuntime::transition_to(SimulationState target_state) {
    if (!can_transition(state_, target_state)) {
        return make_sim_error(
            "Illegal state transition from " + std::string(simulation_state_to_string(state_)) +
            " to " + std::string(simulation_state_to_string(target_state)),
            2610
        );
    }
    SimulationState old = state_;
    state_ = target_state;
    if (state_callback_) {
        state_callback_(old, state_);
    }
    return {};
}

void SimulationRuntime::sort_participants() {
    std::sort(participants_.begin(), participants_.end(),
              [](const std::shared_ptr<ISimulationParticipant>& a, const std::shared_ptr<ISimulationParticipant>& b) {
                  if (a->execution_order() != b->execution_order()) {
                      return a->execution_order() < b->execution_order();
                  }
                  return a->name() < b->name();
              });
}

base::Result<void> SimulationRuntime::configure(const SimulationConfig& config) {
    if (is_simulation_active(state_)) {
        return make_sim_error("Cannot reconfigure simulation while active", 2611);
    }

    auto report = config.validate();
    if (!report.is_valid()) {
        return make_sim_error(report.issues()[0].message, 2612);
    }

    auto res = transition_to(SimulationState::Configuring);
    if (!res.is_ok()) return res;

    config_ = config;
    clock_.reset(config_.start_time);
    step_controller_.configure(config_);
    cancellation_token_.reset();
    last_error_.clear();

    for (auto& participant : participants_) {
        auto init_res = participant->initialize(config_);
        if (!init_res.is_ok()) {
            (void)transition_to(SimulationState::Failed);
            last_error_ = init_res.error().message;
            return init_res;
        }
    }

    return {};
}

base::Result<void> SimulationRuntime::add_participant(std::shared_ptr<ISimulationParticipant> participant) {
    if (!participant) {
        return make_sim_error("Cannot add null simulation participant", 2613);
    }
    if (is_simulation_active(state_)) {
        return make_sim_error("Cannot add participant while simulation is active", 2614);
    }

    for (const auto& existing : participants_) {
        if (existing->name() == participant->name()) {
            return make_sim_error("Participant already exists: " + participant->name(), 2615);
        }
    }

    participants_.push_back(std::move(participant));
    sort_participants();
    return {};
}

void SimulationRuntime::remove_participant(const std::string& name) {
    if (is_simulation_active(state_)) return;
    participants_.erase(
        std::remove_if(participants_.begin(), participants_.end(),
                       [&](const std::shared_ptr<ISimulationParticipant>& p) { return p->name() == name; }),
        participants_.end()
    );
}

std::shared_ptr<ISimulationParticipant> SimulationRuntime::find_participant(const std::string& name) const {
    for (const auto& p : participants_) {
        if (p->name() == name) return p;
    }
    return nullptr;
}

base::Result<void> SimulationRuntime::isolate_project(
    const Project& project,
    const std::vector<std::shared_ptr<ComponentRuntimeInstance>>& components
) {
    auto env_res = IsolatedRunEnvironment::create(project, components);
    if (!env_res.is_ok()) return env_res.error();
    isolated_env_ = std::move(env_res.value());
    return {};
}

base::Result<void> SimulationRuntime::start() {
    if (state_ == SimulationState::Stopped) {
        auto cfg_res = configure(config_);
        if (!cfg_res.is_ok()) return cfg_res;
    }

    auto res = transition_to(SimulationState::Running);
    if (!res.is_ok()) return res;

    run_start_iso_ = get_iso_timestamp();
    clock_.start_wall_clock();
    return {};
}

base::Result<void> SimulationRuntime::pause() {
    if (state_ != SimulationState::Running && state_ != SimulationState::Stepping) {
        return make_sim_error("Can only pause a running simulation", 2616);
    }

    clock_.pause_wall_clock();
    return transition_to(SimulationState::Paused);
}

base::Result<void> SimulationRuntime::resume() {
    if (state_ != SimulationState::Paused) {
        return make_sim_error("Can only resume a paused simulation", 2617);
    }

    clock_.resume_wall_clock();
    return transition_to(SimulationState::Running);
}

base::Result<void> SimulationRuntime::step(std::optional<double> override_dt) {
    if (state_ != SimulationState::Running && state_ != SimulationState::Paused && state_ != SimulationState::Stepping) {
        return make_sim_error("Cannot step simulation in current state", 2618);
    }

    if (cancellation_token_.is_cancelled()) {
        (void)transition_to(SimulationState::Cancelled);
        clock_.stop_wall_clock();
        run_completion_iso_ = get_iso_timestamp();
        return make_sim_error("Simulation cancelled by user", 2619);
    }

    const double t = clock_.physical_time();
    if (t >= config_.end_time) {
        (void)transition_to(SimulationState::Completed);
        clock_.stop_wall_clock();
        run_completion_iso_ = get_iso_timestamp();
        return {};
    }

    SimulationState original_state = state_;
    state_ = SimulationState::Stepping;

    // Determine target dt
    double dt = override_dt.value_or(step_controller_.diagnostics().current_dt);
    if (dt <= 0.0) dt = config_.initial_time_step;

    // Bound by end time
    if (t + dt > config_.end_time) {
        dt = config_.end_time - t;
    }

    // Bound by upcoming discrete events
    dt = event_queue_.calculate_substep_for_events(t, dt);

    // Coordinate domain engines
    bool step_accepted = false;
    double suggested_next_dt = 0.0;
    double max_local_error = 0.0;

    while (!step_accepted) {
        // Pre-step
        for (auto& p : participants_) {
            auto pre_res = p->pre_step(t, dt);
            if (!pre_res.is_ok()) {
                last_error_ = "Pre-step error in " + p->name() + ": " + pre_res.error().message;
                (void)transition_to(SimulationState::Failed);
                clock_.stop_wall_clock();
                return pre_res;
            }
        }

        // Stepping
        step_accepted = true;
        for (auto& p : participants_) {
            auto step_res = p->step(t, dt);
            if (!step_res.is_ok()) {
                last_error_ = "Step error in " + p->name() + ": " + step_res.error().message;
                (void)transition_to(SimulationState::Failed);
                clock_.stop_wall_clock();
                return step_res.error();
            }

            const auto& result = step_res.value();
            if (!result.step_accepted) {
                step_accepted = false;
                auto reject_res = step_controller_.handle_rejected_step(dt, result.message);
                if (!reject_res.is_ok()) {
                    last_error_ = reject_res.error().message;
                    (void)transition_to(SimulationState::Failed);
                    clock_.stop_wall_clock();
                    return reject_res;
                }
                break; // Retry step loop with smaller dt
            }

            if (result.suggested_dt > 0.0) {
                suggested_next_dt = (suggested_next_dt == 0.0)
                    ? result.suggested_dt
                    : std::min(suggested_next_dt, result.suggested_dt);
            }
            if (result.estimated_error > max_local_error) {
                max_local_error = result.estimated_error;
            }
        }
    }

    // Post-step
    for (auto& p : participants_) {
        auto post_res = p->post_step(t, dt);
        if (!post_res.is_ok()) {
            last_error_ = "Post-step error in " + p->name() + ": " + post_res.error().message;
            (void)transition_to(SimulationState::Failed);
            clock_.stop_wall_clock();
            return post_res;
        }
    }

    // Advance physical clock
    clock_.advance(dt);
    step_controller_.record_accepted_step(dt);

    // Compute next step size for controller
    (void)step_controller_.compute_next_dt(dt, suggested_next_dt, max_local_error);

    // Dispatch discrete events matching new physical time
    auto dispatch_res = event_queue_.dispatch_events_up_to(clock_.physical_time());
    if (!dispatch_res.is_ok()) {
        last_error_ = dispatch_res.error().message;
        (void)transition_to(SimulationState::Failed);
        clock_.stop_wall_clock();
        return dispatch_res.error();
    }

    // Check completion condition
    if (clock_.physical_time() >= (config_.end_time - 1e-9) || clock_.step_count() >= config_.max_steps) {
        (void)transition_to(SimulationState::Completed);
        clock_.stop_wall_clock();
        run_completion_iso_ = get_iso_timestamp();
    } else {
        state_ = original_state;
    }

    return {};
}

base::Result<void> SimulationRuntime::run_until(double target_time) {
    if (state_ == SimulationState::Stopped || state_ == SimulationState::Configuring) {
        auto start_res = start();
        if (!start_res.is_ok()) return start_res;
    } else if (state_ == SimulationState::Paused) {
        auto resume_res = resume();
        if (!resume_res.is_ok()) return resume_res;
    }

    while (state_ == SimulationState::Running && clock_.physical_time() < target_time) {
        if (cancellation_token_.is_cancelled()) {
            (void)transition_to(SimulationState::Cancelled);
            clock_.stop_wall_clock();
            run_completion_iso_ = get_iso_timestamp();
            return make_sim_error("Simulation cancelled", 2619);
        }

        auto step_res = step();
        if (!step_res.is_ok()) return step_res;
    }

    return {};
}

base::Result<void> SimulationRuntime::run_all() {
    return run_until(config_.end_time);
}

base::Result<void> SimulationRuntime::stop() {
    clock_.stop_wall_clock();
    run_completion_iso_ = get_iso_timestamp();

    for (auto& p : participants_) {
        (void)p->terminate();
    }

    return transition_to(SimulationState::Stopped);
}

base::Result<void> SimulationRuntime::reset() {
    clock_.reset(config_.start_time);
    step_controller_.reset();
    event_queue_.clear();
    cancellation_token_.reset();
    last_error_.clear();
    checkpoints_.clear();

    for (auto& p : participants_) {
        auto res = p->reset();
        if (!res.is_ok()) return res;
    }

    return transition_to(SimulationState::Stopped);
}

void SimulationRuntime::cancel() noexcept {
    cancellation_token_.request_cancellation();
}

base::Result<SimulationCheckpoint> SimulationRuntime::create_checkpoint() {
    SimulationCheckpoint cp;
    cp.checkpoint_id = base::Uuid::generate_random();
    cp.physical_time = clock_.physical_time();
    cp.step_count = clock_.step_count();
    cp.wall_timestamp = get_iso_timestamp();

    for (const auto& p : participants_) {
        cp.participant_states[p->name()] = p->serialize_state();
    }

    checkpoints_.push_back(cp);
    return cp;
}

base::Result<void> SimulationRuntime::restore_checkpoint(const SimulationCheckpoint& checkpoint) {
    if (state_ == SimulationState::Running) {
        auto pause_res = pause();
        if (!pause_res.is_ok()) return pause_res;
    }

    clock_.set_physical_time(checkpoint.physical_time);
    clock_.set_step_count(checkpoint.step_count);

    for (const auto& [name, state_json] : checkpoint.participant_states) {
        auto p = find_participant(name);
        if (p) {
            auto restore_res = p->restore_state(state_json);
            if (!restore_res.is_ok()) {
                return restore_res;
            }
        }
    }

    return {};
}

double SimulationRuntime::progress_ratio() const noexcept {
    const double span = config_.end_time - config_.start_time;
    if (span <= 1e-9) return 1.0;
    const double progress = (clock_.physical_time() - config_.start_time) / span;
    return std::clamp(progress, 0.0, 1.0);
}

double SimulationRuntime::estimated_remaining_wall_time_seconds() const noexcept {
    const double ratio = progress_ratio();
    if (ratio <= 1e-4) return 0.0;
    const double elapsed_wall = clock_.elapsed_wall_time_seconds();
    const double total_estimated_wall = elapsed_wall / ratio;
    const double remaining = total_estimated_wall - elapsed_wall;
    return (remaining >= 0.0) ? remaining : 0.0;
}

double SimulationRuntime::stepping_throughput() const noexcept {
    const double wall_sec = clock_.elapsed_wall_time_seconds();
    if (wall_sec <= 1e-9) return 0.0;
    return static_cast<double>(clock_.step_count()) / wall_sec;
}

RunManifest SimulationRuntime::generate_run_manifest() const {
    RunManifest manifest;
    manifest.run_id = base::Uuid::generate_random();
    if (isolated_env_) {
        manifest.project_id = isolated_env_->project_id();
        manifest.project_content_hash = isolated_env_->content_hash();
    }
    manifest.solver_name = std::string(solver_type_to_string(config_.solver_type));
    manifest.seed = config_.random_seed;
    manifest.config = config_;
    manifest.start_time_iso = run_start_iso_.empty() ? get_iso_timestamp() : run_start_iso_;
    manifest.completion_time_iso = run_completion_iso_.empty() ? get_iso_timestamp() : run_completion_iso_;
    manifest.final_state = state_;
    manifest.diagnostics = step_controller_.diagnostics();

#if defined(_WIN32)
    manifest.host_os = "Windows";
#elif defined(__linux__)
    manifest.host_os = "Linux";
#elif defined(__APPLE__)
    manifest.host_os = "macOS";
#else
    manifest.host_os = "Unknown";
#endif

#if defined(__clang__)
    manifest.compiler_info = "Clang " __clang_version__;
#elif defined(__GNUC__)
    manifest.compiler_info = "GCC " __VERSION__;
#elif defined(_MSC_VER)
    manifest.compiler_info = "MSVC " + std::to_string(_MSC_VER);
#else
    manifest.compiler_info = "Generic C++20";
#endif

    manifest.engine_version = base::Version(1, 0, 0, "final");
    return manifest;
}

} // namespace shunya::core::domain
