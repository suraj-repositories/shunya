#include "shunya/cosim/master_coordinator.hpp"
#include "shunya/physim/rigid_body.hpp"

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

// Multi-Circuit Support (PH4-10.01.005)
void MasterCoSimulationCoordinator::add_circuit(
    const std::string& circuit_id,
    const std::string& target_subassembly_id) {
    named_circuits_.try_emplace(circuit_id, cirsim::TransientSimulationEngine{});
    if (!target_subassembly_id.empty()) {
        circuit_assembly_map_[circuit_id] = target_subassembly_id;
    }
}

bool MasterCoSimulationCoordinator::has_circuit(const std::string& circuit_id) const noexcept {
    return named_circuits_.find(circuit_id) != named_circuits_.end();
}

size_t MasterCoSimulationCoordinator::circuit_count() const noexcept {
    return named_circuits_.size();
}

std::vector<std::string> MasterCoSimulationCoordinator::circuit_ids() const {
    std::vector<std::string> ids;
    ids.reserve(named_circuits_.size());
    for (const auto& [id, _] : named_circuits_) {
        ids.push_back(id);
    }
    return ids;
}

cirsim::TransientSimulationEngine* MasterCoSimulationCoordinator::find_circuit(
    const std::string& circuit_id) noexcept {
    const auto it = named_circuits_.find(circuit_id);
    return (it != named_circuits_.end()) ? &it->second : nullptr;
}

// =============================================================================
// StateInterpolator Implementation (PH4-10.01.006)
// =============================================================================

InterpolatedRigidBodyState StateInterpolator::interpolate(
    const physim::RigidBodyState& state_t0,
    const physim::RigidBodyState& state_t1,
    double alpha) noexcept {
    const double a = std::clamp(alpha, 0.0, 1.0);
    InterpolatedRigidBodyState res;
    
    // Linear interpolation of position and velocities
    res.position = state_t0.position * (1.0 - a) + state_t1.position * a;
    res.linear_velocity = state_t0.linear_velocity() * (1.0 - a) + state_t1.linear_velocity() * a;
    res.angular_velocity = state_t0.angular_velocity() * (1.0 - a) + state_t1.angular_velocity() * a;
    
    // Spherical linear interpolation (SLERP) for rotation quaternion
    res.orientation = physim::Quaterniond::slerp(state_t0.orientation, state_t1.orientation, a);
    return res;
}

physim::RigidBodyState StateInterpolator::compensate_latency(
    const physim::RigidBodyState& current_state,
    double latency_s) noexcept {
    using namespace shunya::physim;
    physim::RigidBodyState compensated = current_state;
    if (latency_s <= 0.0) {
        return compensated;
    }
    
    // 1st order Taylor expansion prediction
    compensated.position += current_state.linear_velocity() * latency_s;
    
    const Vector3d omega = current_state.angular_velocity();
    const Quaterniond w_quat{0.0, omega.x, omega.y, omega.z};
    const Quaterniond dq = (w_quat * current_state.orientation) * (0.5 * latency_s);
    compensated.orientation = (current_state.orientation + dq).normalized();
    
    return compensated;
}

} // namespace shunya::cosim
