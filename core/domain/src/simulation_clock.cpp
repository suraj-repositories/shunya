#include "shunya/core/domain/simulation_clock.hpp"

namespace shunya::core::domain {

SimulationClock::SimulationClock() noexcept
    : physical_time_(0.0), step_count_(0), time_scale_(1.0), wall_clock_running_(false),
      last_resume_tp_{}, accumulated_wall_time_(0.0) {}

SimulationClock::SimulationClock(double initial_physical_time) noexcept
    : physical_time_(initial_physical_time), step_count_(0), time_scale_(1.0),
      wall_clock_running_(false), last_resume_tp_{}, accumulated_wall_time_(0.0) {}

void SimulationClock::reset(double initial_physical_time) noexcept {
    physical_time_ = initial_physical_time;
    step_count_ = 0;
    wall_clock_running_ = false;
    accumulated_wall_time_ = std::chrono::duration<double>(0.0);
}

void SimulationClock::start_wall_clock() noexcept {
    accumulated_wall_time_ = std::chrono::duration<double>(0.0);
    last_resume_tp_ = std::chrono::steady_clock::now();
    wall_clock_running_ = true;
}

void SimulationClock::pause_wall_clock() noexcept {
    if (wall_clock_running_) {
        const auto now = std::chrono::steady_clock::now();
        accumulated_wall_time_ += (now - last_resume_tp_);
        wall_clock_running_ = false;
    }
}

void SimulationClock::resume_wall_clock() noexcept {
    if (!wall_clock_running_) {
        last_resume_tp_ = std::chrono::steady_clock::now();
        wall_clock_running_ = true;
    }
}

void SimulationClock::stop_wall_clock() noexcept {
    pause_wall_clock();
}

bool SimulationClock::is_wall_clock_running() const noexcept {
    return wall_clock_running_;
}

void SimulationClock::advance(double dt) noexcept {
    if (dt > 0.0) {
        physical_time_ += dt;
        ++step_count_;
    }
}

double SimulationClock::elapsed_wall_time_seconds() const noexcept {
    if (wall_clock_running_) {
        const auto now = std::chrono::steady_clock::now();
        const auto current_slice = now - last_resume_tp_;
        return (accumulated_wall_time_ + current_slice).count();
    }
    return accumulated_wall_time_.count();
}

double SimulationClock::real_time_factor() const noexcept {
    const double elapsed_wall = elapsed_wall_time_seconds();
    if (elapsed_wall <= 1e-9) {
        return 0.0;
    }
    return physical_time_ / elapsed_wall;
}

} // namespace shunya::core::domain
