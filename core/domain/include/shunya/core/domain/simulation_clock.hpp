#pragma once

#include <chrono>
#include <cstdint>

namespace shunya::core::domain {

/**
 * @brief Simulation physical clock decoupled from wall-clock execution time.
 *
 * Implements SIM-03: physical time advancement, wall-clock timing,
 * step counting, time scale adjustment, and real-time factor (RTF) calculations.
 */
class SimulationClock {
public:
    SimulationClock() noexcept;
    explicit SimulationClock(double initial_physical_time) noexcept;

    void reset(double initial_physical_time = 0.0) noexcept;

    void start_wall_clock() noexcept;
    void pause_wall_clock() noexcept;
    void resume_wall_clock() noexcept;
    void stop_wall_clock() noexcept;
    [[nodiscard]] bool is_wall_clock_running() const noexcept;

    void advance(double dt) noexcept;

    [[nodiscard]] double physical_time() const noexcept { return physical_time_; }
    [[nodiscard]] uint64_t step_count() const noexcept { return step_count_; }

    [[nodiscard]] double elapsed_wall_time_seconds() const noexcept;
    [[nodiscard]] double real_time_factor() const noexcept;

    [[nodiscard]] double time_scale() const noexcept { return time_scale_; }
    void set_time_scale(double scale) noexcept { time_scale_ = (scale >= 0.0) ? scale : 0.0; }

    void set_physical_time(double t) noexcept { physical_time_ = t; }
    void set_step_count(uint64_t count) noexcept { step_count_ = count; }

private:
    double physical_time_{0.0};
    uint64_t step_count_{0};
    double time_scale_{1.0}; // 1.0 = real-time factor target, 0.0 = unthrottled

    bool wall_clock_running_{false};
    std::chrono::steady_clock::time_point last_resume_tp_{};
    std::chrono::duration<double> accumulated_wall_time_{0.0};
};

} // namespace shunya::core::domain
