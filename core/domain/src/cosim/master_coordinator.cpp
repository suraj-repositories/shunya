#include "shunya/cosim/master_coordinator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cosim {

size_t MasterCoSimulationCoordinator::sub_cycle_ratio() const noexcept {
    const double micro = std::max(1e-9, config_.micro_time_step_s);
    const double ratio = std::round(current_macro_dt_ / micro);
    return static_cast<size_t>(std::clamp(ratio, 1.0, static_cast<double>(config_.max_sub_cycle_ratio)));
}

void MasterCoSimulationCoordinator::capture_checkpoint() {
    last_checkpoint_.time_s = current_time_s_;
    last_checkpoint_.macro_dt_s = current_macro_dt_;
    last_checkpoint_.telemetry = telemetry_pipe_.get_latest();
    
    // Save physics state snapshot
    physics_controller_.save_named_snapshot("__cosim_checkpoint__");
    
    has_checkpoint_ = true;
}

bool MasterCoSimulationCoordinator::rollback() {
    if (!has_checkpoint_) {
        return false;
    }
    
    current_time_s_ = last_checkpoint_.time_s;
    current_macro_dt_ = last_checkpoint_.macro_dt_s;
    telemetry_pipe_.write(last_checkpoint_.telemetry);
    physics_controller_.restore_named_snapshot("__cosim_checkpoint__");
    
    ++rollback_count_;
    return true;
}

void MasterCoSimulationCoordinator::negotiate_step(bool reduce_step) {
    if (reduce_step) {
        current_macro_dt_ = std::max(config_.min_macro_step_s, current_macro_dt_ * 0.5);
    } else {
        current_macro_dt_ = std::min(config_.max_macro_step_s, current_macro_dt_ * 1.25);
    }
}

CoSimStepResult MasterCoSimulationCoordinator::step(double duration_s) {
    const double step_dt = (duration_s > 0.0) ? duration_s : current_macro_dt_;
    current_macro_dt_ = std::clamp(step_dt, config_.min_macro_step_s, config_.max_macro_step_s);
    
    // Capture state checkpoint before stepping
    capture_checkpoint();
    
    const size_t micro_steps = sub_cycle_ratio();
    const double micro_dt = current_macro_dt_ / static_cast<double>(micro_steps);
    
    // Micro-stepping circuit loop with electromechanical feedback
    CoSimTelemetryPacket current_telemetry = telemetry_pipe_.get_latest();
    
    for (size_t i = 0; i < micro_steps; ++i) {
        const double micro_t = current_time_s_ + static_cast<double>(i + 1) * micro_dt;
        current_telemetry.timestamp_s = micro_t;
        
        // Coupled back-EMF and electromagnetic torque:
        // Tau = K_t * I, V_bemf = K_e * omega
        constexpr double Kt = 0.05; // Torque constant (N*m / A)
        constexpr double Ke = 0.05; // Back-EMF constant (V / (rad/s))
        constexpr double Ra = 1.0;  // Armature resistance (Ohms)
        constexpr double V_supply = 12.0; // Nominal DC supply
        
        current_telemetry.motor_back_emf_v = Ke * current_telemetry.shaft_angular_velocity_rad_s;
        current_telemetry.motor_armature_current_a = std::max(0.0, (V_supply - current_telemetry.motor_back_emf_v) / Ra);
        current_telemetry.motor_mechanical_torque_nm = Kt * current_telemetry.motor_armature_current_a;
        
        // Heat dissipation: dTemp = (I^2 * R * dt) / C_th
        constexpr double C_th = 50.0; // Thermal capacitance (J/K)
        current_telemetry.component_temperature_k += 
            (current_telemetry.motor_armature_current_a * current_telemetry.motor_armature_current_a * Ra * micro_dt) / C_th;
        
        telemetry_pipe_.write(current_telemetry);
    }
    
    // Advance physics macro step
    physics_controller_.step_forward(current_macro_dt_);
    
    // Check physics state for divergence or excessive impulse
    bool divergence_detected = false;
    for (const auto& body : physics_controller_.bodies()) {
        const auto& st = body.state();
        if (std::isnan(st.position.x) || std::isnan(st.position.y) || std::isnan(st.position.z) ||
            std::isinf(st.linear_velocity().length()) || std::isinf(st.angular_velocity().length())) {
            divergence_detected = true;
            break;
        }
        if (st.linear_velocity().length() > config_.collision_impulse_rollback_threshold) {
            divergence_detected = true;
            break;
        }
    }
    
    if (divergence_detected) {
        rollback();
        negotiate_step(true); // Halve the macro step size
        return CoSimStepResult{
            false,
            0.0,
            micro_steps,
            true,
            "Numerical instability or collision impulse threshold exceeded; rolled back."
        };
    }
    
    // Update telemetry shaft velocity from physics bodies if available
    if (!physics_controller_.bodies().empty()) {
        const auto& body0 = physics_controller_.bodies().front();
        current_telemetry.shaft_angular_velocity_rad_s = body0.state().angular_velocity().length();
        constexpr double Kt = 0.05;
        constexpr double Ke = 0.05;
        constexpr double Ra = 1.0;
        constexpr double V_supply = 12.0;
        current_telemetry.motor_back_emf_v = Ke * current_telemetry.shaft_angular_velocity_rad_s;
        current_telemetry.motor_armature_current_a = std::max(0.0, (V_supply - current_telemetry.motor_back_emf_v) / Ra);
        current_telemetry.motor_mechanical_torque_nm = Kt * current_telemetry.motor_armature_current_a;
        telemetry_pipe_.write(current_telemetry);
    }
    
    current_time_s_ += current_macro_dt_;
    
    return CoSimStepResult{
        true,
        current_macro_dt_,
        micro_steps,
        false,
        "Step completed successfully."
    };
}

} // namespace shunya::cosim
