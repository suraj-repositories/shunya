#pragma once

#include "shunya/physim/rigid_body.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace shunya::cosim {

// =============================================================================
// DC Motor Transducer (PH4-10.02.001)
// =============================================================================
struct DCMotorSpec {
    double Kt{0.05};            ///< Motor torque constant (N*m / A)
    double Ra{1.0};             ///< Armature resistance (Ohms)
    double La{1e-3};            ///< Armature inductance (H)
    double rotor_inertia{1e-4}; ///< Rotor inertia J_m (kg*m^2)
    double viscous_damping{1e-4};///< Viscous friction coefficient b_m (N*m*s / rad)
    double coulomb_friction{1e-3};///< Static/Coulomb breakaway torque (N*m)
};

class DCMotorTransducer {
public:
    explicit DCMotorTransducer(DCMotorSpec spec = {}) : spec_(spec) {}

    [[nodiscard]] double compute_torque(double current_a, double angular_velocity_rad_s) const noexcept {
        const double electromagnetic_torque = spec_.Kt * current_a;
        const double viscous_loss = spec_.viscous_damping * angular_velocity_rad_s;
        
        double friction_loss = 0.0;
        if (std::abs(angular_velocity_rad_s) > 1e-3) {
            friction_loss = (angular_velocity_rad_s > 0.0 ? spec_.coulomb_friction : -spec_.coulomb_friction);
        }
        
        return electromagnetic_torque - viscous_loss - friction_loss;
    }

    void apply_torque_to_body(
        physim::RigidBodyEntity& body,
        const physim::Vector3d& axle_axis,
        double current_a) const noexcept {
        const double omega = body.state().angular_velocity().dot(axle_axis.normalized());
        const double tau = compute_torque(current_a, omega);
        body.state().apply_torque(axle_axis.normalized() * tau);
    }

    [[nodiscard]] const DCMotorSpec& spec() const noexcept { return spec_; }
    void set_spec(const DCMotorSpec& sp) noexcept { spec_ = sp; }

private:
    DCMotorSpec spec_;
};

// =============================================================================
// Motor Back-EMF Coupling (PH4-10.02.002)
// =============================================================================
class MotorBackEmfCoupling {
public:
    [[nodiscard]] static constexpr double compute_back_emf(
        double angular_velocity_rad_s,
        double Ke) noexcept {
        return Ke * angular_velocity_rad_s;
    }

    [[nodiscard]] static constexpr double compute_armature_current(
        double v_supply,
        double angular_velocity_rad_s,
        double Ra,
        double Ke) noexcept {
        if (Ra <= 1e-9) return 0.0;
        const double bemf = compute_back_emf(angular_velocity_rad_s, Ke);
        return (v_supply - bemf) / Ra;
    }

    [[nodiscard]] static constexpr double no_load_speed(
        double v_supply,
        double Ke) noexcept {
        if (Ke <= 1e-9) return 0.0;
        return v_supply / Ke;
    }

    [[nodiscard]] static constexpr double stall_torque(
        double v_supply,
        double Ra,
        double Kt) noexcept {
        if (Ra <= 1e-9) return 0.0;
        return Kt * (v_supply / Ra);
    }
};

// =============================================================================
// Solenoid & Linear Reluctance Actuator (PH4-10.02.003)
// =============================================================================
struct SolenoidSpec {
    uint32_t turns{500};               ///< Number of coil turns N
    double pole_area_m2{2e-4};         ///< Pole face area A (m^2)
    double residual_gap_m{5e-4};       ///< Residual / minimum air gap x0 (m)
    double coil_resistance_ohms{5.0};  ///< Coil DC resistance (Ohms)
};

class SolenoidActuatorTransducer {
public:
    explicit SolenoidActuatorTransducer(SolenoidSpec spec = {}) : spec_(spec) {}

    [[nodiscard]] double compute_force(double current_a, double air_gap_m) const noexcept {
        // F = (mu0 * N^2 * A / (2 * (x + x0)^2)) * I^2
        constexpr double mu0 = 4.0 * std::numbers::pi * 1e-7; // Vacuum permeability
        const double x_eff = std::max(1e-6, air_gap_m + spec_.residual_gap_m);
        const double N = static_cast<double>(spec_.turns);
        const double num = mu0 * N * N * spec_.pole_area_m2;
        const double denom = 2.0 * x_eff * x_eff;
        return (num / denom) * (current_a * current_a);
    }

    [[nodiscard]] double compute_inductance(double air_gap_m) const noexcept {
        // L(x) = (mu0 * N^2 * A) / (x + x0)
        constexpr double mu0 = 4.0 * std::numbers::pi * 1e-7;
        const double x_eff = std::max(1e-6, air_gap_m + spec_.residual_gap_m);
        const double N = static_cast<double>(spec_.turns);
        return (mu0 * N * N * spec_.pole_area_m2) / x_eff;
    }

    void apply_force_to_body(
        physim::RigidBodyEntity& body,
        const physim::Vector3d& slider_axis,
        double current_a,
        double air_gap_m) const noexcept {
        const double f = compute_force(current_a, air_gap_m);
        body.state().apply_force_at_com(slider_axis.normalized() * f);
    }

    [[nodiscard]] const SolenoidSpec& spec() const noexcept { return spec_; }

private:
    SolenoidSpec spec_;
};

// =============================================================================
// Piezoelectric Transducer (PH4-10.02.004)
// =============================================================================
struct PiezoelectricSpec {
    double d33_c_per_n{350e-12};        ///< Piezoelectric charge coefficient d33 (m/V or C/N)
    double stiffness_n_per_m{1e7};      ///< Mechanical stiffness k_piezo (N/m)
    double capacitance_f{10e-9};        ///< Clamped electrical capacitance (10 nF)
    double acoustic_efficiency{0.15};   ///< Electro-acoustic efficiency factor
};

class PiezoelectricTransducer {
public:
    explicit PiezoelectricTransducer(PiezoelectricSpec spec = {}) : spec_(spec) {}

    [[nodiscard]] double compute_displacement(double voltage_v) const noexcept {
        return spec_.d33_c_per_n * voltage_v;
    }

    [[nodiscard]] double compute_blocking_force(double voltage_v) const noexcept {
        const double disp = compute_displacement(voltage_v);
        return spec_.stiffness_n_per_m * disp;
    }

    [[nodiscard]] double compute_acoustic_spl_db(
        double voltage_v,
        double frequency_hz,
        double distance_m) const noexcept {
        if (distance_m <= 0.0 || frequency_hz <= 0.0) return 0.0;
        
        // Approximate SPL from vibrational velocity and acoustic radiation
        const double disp_peak = std::abs(compute_displacement(voltage_v));
        const double omega = 2.0 * std::numbers::pi * frequency_hz;
        const double v_peak = omega * disp_peak;
        
        constexpr double rho_air = 1.225;
        constexpr double c_air = 343.0;
        constexpr double p_ref = 20e-6; // 20 uPa auditory threshold
        
        const double p_acoustic = (rho_air * c_air * v_peak * spec_.acoustic_efficiency) / std::max(0.1, distance_m);
        if (p_acoustic <= p_ref) return 0.0;
        
        return 20.0 * std::log10(p_acoustic / p_ref);
    }

    [[nodiscard]] const PiezoelectricSpec& spec() const noexcept { return spec_; }

private:
    PiezoelectricSpec spec_;
};

// =============================================================================
// Thermal Joule Heating Feedback & Linear Expansion (PH4-10.02.005)
// =============================================================================
struct JouleHeatingSpec {
    double resistance_ohms{1.0};           ///< Electrical resistance R
    double thermal_capacitance_j_k{10.0};  ///< Heat capacity C_th (J/K)
    double thermal_resistance_k_w{5.0};    ///< Thermal dissipation resistance R_th (K/W)
    double ambient_temperature_k{298.15};  ///< Ambient reference T0 (25 C)
    double thermal_expansion_coeff{16.5e-6};///< Linear thermal expansion alpha (1/K)
};

class ThermalJouleHeatingCoupling {
public:
    explicit ThermalJouleHeatingCoupling(JouleHeatingSpec spec = {})
        : spec_(spec), current_temperature_k_(spec.ambient_temperature_k) {}

    double step_thermal(double current_a, double dt_s) noexcept {
        if (dt_s <= 0.0) return current_temperature_k_;
        
        const double p_joule = current_a * current_a * spec_.resistance_ohms;
        const double p_dissipated = (current_temperature_k_ - spec_.ambient_temperature_k) / spec_.thermal_resistance_k_w;
        
        const double dT = ((p_joule - p_dissipated) / spec_.thermal_capacitance_j_k) * dt_s;
        current_temperature_k_ = std::max(spec_.ambient_temperature_k, current_temperature_k_ + dT);
        return current_temperature_k_;
    }

    [[nodiscard]] double compute_thermal_expansion(double original_length_m) const noexcept {
        const double delta_t = current_temperature_k_ - spec_.ambient_temperature_k;
        return original_length_m * spec_.thermal_expansion_coeff * delta_t;
    }

    [[nodiscard]] double compute_scale_factor() const noexcept {
        const double delta_t = current_temperature_k_ - spec_.ambient_temperature_k;
        return 1.0 + spec_.thermal_expansion_coeff * delta_t;
    }

    [[nodiscard]] double temperature_k() const noexcept { return current_temperature_k_; }
    void reset_temperature() noexcept { current_temperature_k_ = spec_.ambient_temperature_k; }

private:
    JouleHeatingSpec spec_;
    double current_temperature_k_{298.15};
};

// =============================================================================
// Automated Energy Balance Validator (PH4-10.02.006)
// =============================================================================
class EnergyBalanceAuditor {
public:
    EnergyBalanceAuditor() = default;

    void accumulate_step(
        double voltage_v,
        double current_a,
        double motor_torque_nm,
        double angular_velocity_rad_s,
        double linear_force_n,
        double linear_velocity_m_s,
        double component_resistance_ohms,
        double dt_s) noexcept {
        if (dt_s <= 0.0) return;
        
        // Electrical energy input: E_elec = sum(V * I * dt)
        electrical_energy_j_ += (voltage_v * current_a) * dt_s;
        
        // Mechanical work performed: W_mech = sum((tau * omega + F * v) * dt)
        mechanical_work_j_ += (motor_torque_nm * angular_velocity_rad_s + linear_force_n * linear_velocity_m_s) * dt_s;
        
        // Thermal power loss: Q_therm = sum(I^2 * R * dt)
        thermal_loss_j_ += (current_a * current_a * component_resistance_ohms) * dt_s;
    }

    [[nodiscard]] double electrical_energy_j() const noexcept { return electrical_energy_j_; }
    [[nodiscard]] double mechanical_work_j() const noexcept { return mechanical_work_j_; }
    [[nodiscard]] double thermal_losses_j() const noexcept { return thermal_loss_j_; }

    [[nodiscard]] double energy_residual_j() const noexcept {
        return electrical_energy_j_ - (mechanical_work_j_ + thermal_loss_j_);
    }

    [[nodiscard]] bool is_energy_conserved(double tolerance_j = 1e-2) const noexcept {
        return std::abs(energy_residual_j()) <= tolerance_j;
    }

    void reset() noexcept {
        electrical_energy_j_ = 0.0;
        mechanical_work_j_ = 0.0;
        thermal_loss_j_ = 0.0;
    }

private:
    double electrical_energy_j_{0.0};
    double mechanical_work_j_{0.0};
    double thermal_loss_j_{0.0};
};

} // namespace shunya::cosim
