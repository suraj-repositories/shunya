#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/units.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace shunya::core::domain::thermal {

using Vector3d = base::math::Vector3<double>;

inline constexpr double STEFAN_BOLTZMANN = 5.670374419e-8; // W / (m^2 * K^4)
inline constexpr double ABSOLUTE_ZERO = 0.0; // Kelvin

// =============================================================================
// THM-01: Thermal State & Capacity
// =============================================================================

struct ThermalState {
    double temperature = 293.15; // Kelvin (20 °C)
    double heat_source = 0.0;    // Watts (internal heat generation)

    [[nodiscard]] double temperature_celsius() const noexcept {
        return temperature - 273.15;
    }

    void set_celsius(double temp_c) noexcept {
        temperature = temp_c + 273.15;
    }
};

struct ThermalMaterial {
    std::string name = "Generic Conductor";
    double density = 2700.0;              // kg / m^3 (e.g. Aluminum ~2700)
    double specific_heat = 900.0;          // J / (kg * K)
    double thermal_conductivity = 200.0;   // W / (m * K)
    double emissivity = 0.8;               // [0.0 - 1.0]

    // THM-07: Temperature-dependent material property curves
    std::function<double(double)> k_func = nullptr;
    std::function<double(double)> cp_func = nullptr;

    [[nodiscard]] double get_k(double temp_k) const noexcept {
        if (k_func) return k_func(temp_k);
        return thermal_conductivity;
    }

    [[nodiscard]] double get_cp(double temp_k) const noexcept {
        if (cp_func) return cp_func(temp_k);
        return specific_heat;
    }

    [[nodiscard]] double volumetric_heat_capacity(double temp_k = 293.15) const noexcept {
        return density * get_cp(temp_k);
    }
};

// =============================================================================
// THM-02, THM-03, THM-04, THM-05: Thermal Node, Boundary Conditions & Resistors
// =============================================================================

enum class BoundaryType {
    None,
    FixedTemperature, // Dirichlet: T = T_specified
    HeatFlux,         // Neumann: Q = Q_specified (Watts)
    Convection,       // Robin: q = h * A * (T - T_ambient)
    Radiation         // Stefan-Boltzmann: q = epsilon * sigma * A * (T^4 - T_ambient^4)
};

struct ThermalBoundaryCondition {
    BoundaryType type = BoundaryType::None;
    double fixed_temperature = 293.15;       // Kelvin
    double heat_flux = 0.0;                  // Watts
    double convection_h = 10.0;              // W / (m^2 * K)
    double ambient_temperature = 293.15;     // Kelvin
    double surface_area = 1.0;               // m^2
    double emissivity = 0.8;                 // Surface emissivity
};

struct ThermalNode {
    std::size_t id = 0;
    std::string label;
    Vector3d position{0.0, 0.0, 0.0};
    double volume = 1e-3; // m^3 (1 liter default)
    ThermalMaterial material;
    ThermalState state;
    ThermalBoundaryCondition boundary;

    [[nodiscard]] double thermal_mass() const noexcept {
        const double m = volume * material.density;
        return m * material.get_cp(state.temperature);
    }

    [[nodiscard]] double internal_energy() const noexcept {
        return thermal_mass() * state.temperature;
    }
};

enum class ResistorType {
    Conduction,
    ContactResistance,
    ConvectionFilm,
    RadiationExchange
};

struct ThermalResistor {
    std::size_t node_a = 0;
    std::size_t node_b = 0;
    ResistorType type = ResistorType::Conduction;
    double conductance = 1.0; // Watts / Kelvin (G = 1 / R_th)

    [[nodiscard]] double resistance() const noexcept {
        return (conductance > 1e-15) ? (1.0 / conductance) : 1e15;
    }

    [[nodiscard]] static ThermalResistor create_conduction(
        std::size_t na, std::size_t nb, double k, double area, double length) noexcept {
        ThermalResistor r;
        r.node_a = na;
        r.node_b = nb;
        r.type = ResistorType::Conduction;
        const double l_eff = std::max(length, 1e-9);
        r.conductance = (k * area) / l_eff;
        return r;
    }

    [[nodiscard]] static ThermalResistor create_contact(
        std::size_t na, std::size_t nb, double r_contact) noexcept {
        ThermalResistor r;
        r.node_a = na;
        r.node_b = nb;
        r.type = ResistorType::ContactResistance;
        r.conductance = (r_contact > 1e-12) ? (1.0 / r_contact) : 1e12;
        return r;
    }
};

// =============================================================================
// THM-06: Solvers: Steady-State and Transient Thermal Networks
// =============================================================================

struct ThermalSimulationConfig {
    double time_step = 0.1;           // seconds
    int max_iterations = 200;
    double convergence_tol = 1e-6;
    bool enable_radiation = true;
};

class ThermalNetwork {
public:
    ThermalNetwork() = default;

    std::size_t add_node(const ThermalNode& node);
    void add_resistor(const ThermalResistor& res);

    [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] std::size_t resistor_count() const noexcept { return resistors_.size(); }

    [[nodiscard]] ThermalNode& get_node(std::size_t id) { return nodes_.at(id); }
    [[nodiscard]] const ThermalNode& get_node(std::size_t id) const { return nodes_.at(id); }
    [[nodiscard]] const std::vector<ThermalNode>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<ThermalResistor>& resistors() const noexcept { return resistors_; }

    /// Solves steady-state temperature distribution: [G] {T} = {Q}
    [[nodiscard]] base::Result<void> solve_steady_state(const ThermalSimulationConfig& config = {});

    /// Advances transient solution by time dt using unconditionally stable implicit Euler
    [[nodiscard]] base::Result<void> step_transient(double dt, const ThermalSimulationConfig& config = {});

    /// Total system internal energy: sum(m_i * cp_i * T_i)
    [[nodiscard]] double total_internal_energy() const noexcept;

    /// Heat flux vector at a given resistor: Q_ab = G_ab * (T_a - T_b)
    [[nodiscard]] double compute_resistor_heat_flow(std::size_t res_idx) const noexcept;

private:
    std::vector<ThermalNode> nodes_;
    std::vector<ThermalResistor> resistors_;
};

// =============================================================================
// THM-07: Phase Change Model (Apparent Heat Capacity Method)
// =============================================================================

struct PhaseChangeMaterial {
    double solidus_temperature = 273.15; // K (0 °C)
    double liquidus_temperature = 274.15; // K (1 °C)
    double latent_heat = 333000.0;        // J / kg (Water ice melting ~333 kJ/kg)
    double base_specific_heat = 2000.0;   // J / (kg * K)

    [[nodiscard]] double effective_specific_heat(double temp_k) const noexcept {
        if (temp_k >= solidus_temperature && temp_k <= liquidus_temperature) {
            const double delta_t = std::max(liquidus_temperature - solidus_temperature, 1e-4);
            const double apparent_cp = latent_heat / delta_t;
            return base_specific_heat + apparent_cp;
        }
        return base_specific_heat;
    }
};

// =============================================================================
// THM-08: Thermal Visualization & Observability Probes
// =============================================================================

struct ThermalProbeSample {
    double timestamp = 0.0;
    double temperature_k = 293.15;
    double temperature_c = 20.0;
    double heat_flow = 0.0;
};

class ThermalProbe {
public:
    explicit ThermalProbe(std::size_t node_id, std::string label = "Probe");

    [[nodiscard]] std::size_t node_id() const noexcept { return node_id_; }
    [[nodiscard]] const std::string& label() const noexcept { return label_; }

    void record(double timestamp, double temp_k, double heat_flow = 0.0);
    [[nodiscard]] const std::vector<ThermalProbeSample>& samples() const noexcept { return history_; }
    [[nodiscard]] std::optional<ThermalProbeSample> latest_sample() const noexcept;
    void clear() noexcept { history_.clear(); }

private:
    std::size_t node_id_;
    std::string label_;
    std::vector<ThermalProbeSample> history_;
};

// =============================================================================
// THM-09: Validation Benchmark Suite Factory
// =============================================================================

class ThermalBenchmarkFactory {
public:
    /// 1D Steady-State Conduction Slab: T1 at left, T2 at right, length L, thermal conductivity k
    /// Analytical solution: T(x) = T1 + (T2 - T1)*x/L; Heat Rate Q = k*A*(T1 - T2)/L
    [[nodiscard]] static ThermalNetwork create_1d_slab_conduction(
        double length, double area, double k, double t_left, double t_right, std::size_t num_divisions = 10);

    /// Lumped Capacitance Cooling Benchmark:
    /// High thermal conductivity solid cooled by convection: T(t) = T_inf + (T0 - T_inf) * exp(-t / tau)
    /// where tau = (rho * V * cp) / (h * A)
    [[nodiscard]] static ThermalNetwork create_lumped_cooling_benchmark(
        double mass, double cp, double surface_area, double h_conv, double initial_t, double ambient_t);

    /// Composite Wall (Conduction + Thermal Contact Interface Resistance)
    [[nodiscard]] static ThermalNetwork create_composite_wall_benchmark(
        double k1, double l1, double r_contact, double k2, double l2, double area, double t1, double t2);

    /// Pure Stefan-Boltzmann Radiation Cooling into cold vacuum
    [[nodiscard]] static ThermalNetwork create_radiation_cooling_benchmark(
        double mass, double cp, double area, double emissivity, double initial_t, double ambient_t);
};

} // namespace shunya::core::domain::thermal
