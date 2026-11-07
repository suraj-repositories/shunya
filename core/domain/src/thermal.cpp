#include "shunya/core/domain/thermal.hpp"

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace shunya::core::domain::thermal {

namespace {

// Dense linear solver with partial pivoting: A x = b
bool solve_linear_system(std::vector<std::vector<double>>& A, std::vector<double>& b, std::vector<double>& x) {
    const std::size_t n = b.size();
    x.assign(n, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        // Find pivot
        std::size_t max_row = i;
        double max_val = std::abs(A[i][i]);
        for (std::size_t k = i + 1; k < n; ++k) {
            const double val = std::abs(A[k][i]);
            if (val > max_val) {
                max_val = val;
                max_row = k;
            }
        }

        if (max_val < 1e-15) {
            return false; // Singular matrix
        }

        if (max_row != i) {
            std::swap(A[i], A[max_row]);
            std::swap(b[i], b[max_row]);
        }

        // Eliminate below
        for (std::size_t k = i + 1; k < n; ++k) {
            const double factor = A[k][i] / A[i][i];
            for (std::size_t j = i; j < n; ++j) {
                A[k][j] -= factor * A[i][j];
            }
            b[k] -= factor * b[i];
        }
    }

    // Back substitution
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        double sum = b[i];
        for (std::size_t j = static_cast<std::size_t>(i) + 1; j < n; ++j) {
            sum -= A[i][j] * x[j];
        }
        x[i] = sum / A[i][i];
    }

    return true;
}

base::Error make_thermal_error(std::string msg) {
    base::Error err;
    err.code = 1;
    err.domain = base::ErrorDomain::Simulation;
    err.severity = base::Severity::Error;
    err.message = std::move(msg);
    return err;
}

} // namespace

// =============================================================================
// ThermalNetwork
// =============================================================================

std::size_t ThermalNetwork::add_node(const ThermalNode& node) {
    ThermalNode copy = node;
    copy.id = nodes_.size();
    nodes_.push_back(copy);
    return copy.id;
}

void ThermalNetwork::add_resistor(const ThermalResistor& res) {
    resistors_.push_back(res);
}

double ThermalNetwork::total_internal_energy() const noexcept {
    double total = 0.0;
    for (const auto& n : nodes_) {
        total += n.internal_energy();
    }
    return total;
}

double ThermalNetwork::compute_resistor_heat_flow(std::size_t res_idx) const noexcept {
    if (res_idx >= resistors_.size()) return 0.0;
    const auto& res = resistors_[res_idx];
    if (res.node_a >= nodes_.size() || res.node_b >= nodes_.size()) return 0.0;
    const double ta = nodes_[res.node_a].state.temperature;
    const double tb = nodes_[res.node_b].state.temperature;
    return res.conductance * (ta - tb);
}

// -----------------------------------------------------------------------------
// Steady-State Solver
// -----------------------------------------------------------------------------

base::Result<void> ThermalNetwork::solve_steady_state(const ThermalSimulationConfig& config) {
    const std::size_t n = nodes_.size();
    if (n == 0) return {};

    for (int iter = 0; iter < config.max_iterations; ++iter) {
        std::vector<std::vector<double>> A(n, std::vector<double>(n, 0.0));
        std::vector<double> b(n, 0.0);

        // 1. Assemble inter-node conductances
        for (const auto& res : resistors_) {
            if (res.node_a >= n || res.node_b >= n) continue;
            const double g = res.conductance;
            A[res.node_a][res.node_a] += g;
            A[res.node_b][res.node_b] += g;
            A[res.node_a][res.node_b] -= g;
            A[res.node_b][res.node_a] -= g;
        }

        // 2. Assemble boundary conditions & sources
        for (std::size_t i = 0; i < n; ++i) {
            const auto& node = nodes_[i];
            b[i] += node.state.heat_source;

            if (node.boundary.type == BoundaryType::FixedTemperature) {
                // Clear row and enforce T_i = T_fixed
                for (std::size_t j = 0; j < n; ++j) {
                    A[i][j] = 0.0;
                }
                A[i][i] = 1.0;
                b[i] = node.boundary.fixed_temperature;
            } else if (node.boundary.type == BoundaryType::HeatFlux) {
                b[i] += node.boundary.heat_flux;
            } else if (node.boundary.type == BoundaryType::Convection) {
                const double g_conv = node.boundary.convection_h * node.boundary.surface_area;
                A[i][i] += g_conv;
                b[i] += g_conv * node.boundary.ambient_temperature;
            } else if (node.boundary.type == BoundaryType::Radiation && config.enable_radiation) {
                // Newton-Raphson linearization of Stefan-Boltzmann:
                // q(T) = eps * sigma * A * (T^4 - T_amb^4)
                // q(T) ~ 4 * eps * sigma * A * (T_k)^3 * T - 3 * eps * sigma * A * (T_k)^4 - eps * sigma * A * T_amb^4
                const double t_curr = std::max(node.state.temperature, 1.0);
                const double t_amb = std::max(node.boundary.ambient_temperature, 1.0);
                const double coeff = node.boundary.emissivity * STEFAN_BOLTZMANN * node.boundary.surface_area;
                const double t_cubed = t_curr * t_curr * t_curr;
                const double t_fourth = t_cubed * t_curr;
                const double t_amb_fourth = t_amb * t_amb * t_amb * t_amb;

                A[i][i] += 4.0 * coeff * t_cubed;
                b[i] += 3.0 * coeff * t_fourth + coeff * t_amb_fourth;
            }
        }

        std::vector<double> t_new(n, 0.0);
        if (!solve_linear_system(A, b, t_new)) {
            return base::Result<void>::err(make_thermal_error("Singular matrix during steady-state solve"));
        }

        // Check convergence
        double max_diff = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            max_diff = std::max(max_diff, std::abs(t_new[i] - nodes_[i].state.temperature));
            nodes_[i].state.temperature = t_new[i];
        }

        if (max_diff < config.convergence_tol) {
            return {};
        }
    }

    return {};
}

// -----------------------------------------------------------------------------
// Transient Solver (Implicit Backward Euler)
// -----------------------------------------------------------------------------

base::Result<void> ThermalNetwork::step_transient(double dt, const ThermalSimulationConfig& config) {
    const std::size_t n = nodes_.size();
    if (n == 0 || dt <= 0.0) return {};

    std::vector<std::vector<double>> A(n, std::vector<double>(n, 0.0));
    std::vector<double> b(n, 0.0);

    // 1. Inter-node conduction conductances
    for (const auto& res : resistors_) {
        if (res.node_a >= n || res.node_b >= n) continue;
        const double g = res.conductance;
        A[res.node_a][res.node_a] += g;
        A[res.node_b][res.node_b] += g;
        A[res.node_a][res.node_b] -= g;
        A[res.node_b][res.node_a] -= g;
    }

    // 2. Node capacitances and boundaries
    for (std::size_t i = 0; i < n; ++i) {
        const auto& node = nodes_[i];

        if (node.boundary.type == BoundaryType::FixedTemperature) {
            for (std::size_t j = 0; j < n; ++j) {
                A[i][j] = 0.0;
            }
            A[i][i] = 1.0;
            b[i] = node.boundary.fixed_temperature;
            continue;
        }

        const double c_th = node.thermal_mass();
        const double c_dt = c_th / dt;

        A[i][i] += c_dt;
        b[i] += c_dt * node.state.temperature + node.state.heat_source;

        if (node.boundary.type == BoundaryType::HeatFlux) {
            b[i] += node.boundary.heat_flux;
        } else if (node.boundary.type == BoundaryType::Convection) {
            const double g_conv = node.boundary.convection_h * node.boundary.surface_area;
            A[i][i] += g_conv;
            b[i] += g_conv * node.boundary.ambient_temperature;
        } else if (node.boundary.type == BoundaryType::Radiation && config.enable_radiation) {
            // Newton-Raphson linearization of Stefan-Boltzmann
            const double t_curr = std::max(node.state.temperature, 1.0);
            const double t_amb = std::max(node.boundary.ambient_temperature, 1.0);
            const double coeff = node.boundary.emissivity * STEFAN_BOLTZMANN * node.boundary.surface_area;
            const double t_cubed = t_curr * t_curr * t_curr;
            const double t_fourth = t_cubed * t_curr;
            const double t_amb_fourth = t_amb * t_amb * t_amb * t_amb;

            A[i][i] += 4.0 * coeff * t_cubed;
            b[i] += 3.0 * coeff * t_fourth + coeff * t_amb_fourth;
        }
    }

    std::vector<double> t_new(n, 0.0);
    if (!solve_linear_system(A, b, t_new)) {
        return base::Result<void>::err(make_thermal_error("Singular matrix during transient step"));
    }

    for (std::size_t i = 0; i < n; ++i) {
        nodes_[i].state.temperature = t_new[i];
    }

    return {};
}

// =============================================================================
// ThermalProbe
// =============================================================================

ThermalProbe::ThermalProbe(std::size_t node_id, std::string label)
    : node_id_(node_id), label_(std::move(label)) {}

void ThermalProbe::record(double timestamp, double temp_k, double heat_flow) {
    history_.push_back(ThermalProbeSample{
        .timestamp = timestamp,
        .temperature_k = temp_k,
        .temperature_c = temp_k - 273.15,
        .heat_flow = heat_flow
    });
}

std::optional<ThermalProbeSample> ThermalProbe::latest_sample() const noexcept {
    if (history_.empty()) return std::nullopt;
    return history_.back();
}

// =============================================================================
// ThermalBenchmarkFactory
// =============================================================================

ThermalNetwork ThermalBenchmarkFactory::create_1d_slab_conduction(
    double length, double area, double k, double t_left, double t_right, std::size_t num_divisions) {
    ThermalNetwork net;
    const std::size_t n = std::max(num_divisions, std::size_t{2});
    const double dx = length / static_cast<double>(n - 1);
    const double node_vol = area * dx;

    ThermalMaterial mat;
    mat.name = "Slab Material";
    mat.thermal_conductivity = k;

    for (std::size_t i = 0; i < n; ++i) {
        ThermalNode node;
        node.label = "node_" + std::to_string(i);
        node.position = {static_cast<double>(i) * dx, 0.0, 0.0};
        node.volume = node_vol;
        node.material = mat;
        node.state.temperature = 0.5 * (t_left + t_right);

        if (i == 0) {
            node.boundary.type = BoundaryType::FixedTemperature;
            node.boundary.fixed_temperature = t_left;
            node.state.temperature = t_left;
        } else if (i == n - 1) {
            node.boundary.type = BoundaryType::FixedTemperature;
            node.boundary.fixed_temperature = t_right;
            node.state.temperature = t_right;
        }
        net.add_node(node);
    }

    for (std::size_t i = 0; i < n - 1; ++i) {
        net.add_resistor(ThermalResistor::create_conduction(i, i + 1, k, area, dx));
    }

    return net;
}

ThermalNetwork ThermalBenchmarkFactory::create_lumped_cooling_benchmark(
    double mass, double cp, double surface_area, double h_conv, double initial_t, double ambient_t) {
    ThermalNetwork net;

    ThermalNode node;
    node.label = "lumped_body";
    node.volume = 1e-3;
    node.material.density = mass / node.volume;
    node.material.specific_heat = cp;
    node.state.temperature = initial_t;

    node.boundary.type = BoundaryType::Convection;
    node.boundary.surface_area = surface_area;
    node.boundary.convection_h = h_conv;
    node.boundary.ambient_temperature = ambient_t;

    net.add_node(node);
    return net;
}

ThermalNetwork ThermalBenchmarkFactory::create_composite_wall_benchmark(
    double k1, double l1, double r_contact, double k2, double l2, double area, double t1, double t2) {
    ThermalNetwork net;

    // Node 0: Left side of layer 1 (T1)
    ThermalNode n0;
    n0.boundary.type = BoundaryType::FixedTemperature;
    n0.boundary.fixed_temperature = t1;
    n0.state.temperature = t1;
    net.add_node(n0);

    // Node 1: Right side of layer 1 (contact interface left)
    ThermalNode n1;
    n1.state.temperature = 0.5 * (t1 + t2);
    net.add_node(n1);

    // Node 2: Left side of layer 2 (contact interface right)
    ThermalNode n2;
    n2.state.temperature = 0.5 * (t1 + t2);
    net.add_node(n2);

    // Node 3: Right side of layer 2 (T2)
    ThermalNode n3;
    n3.boundary.type = BoundaryType::FixedTemperature;
    n3.boundary.fixed_temperature = t2;
    n3.state.temperature = t2;
    net.add_node(n3);

    // Resistors: Layer 1 conduction, Contact resistance, Layer 2 conduction
    net.add_resistor(ThermalResistor::create_conduction(0, 1, k1, area, l1));
    net.add_resistor(ThermalResistor::create_contact(1, 2, r_contact));
    net.add_resistor(ThermalResistor::create_conduction(2, 3, k2, area, l2));

    return net;
}

ThermalNetwork ThermalBenchmarkFactory::create_radiation_cooling_benchmark(
    double mass, double cp, double area, double emissivity, double initial_t, double ambient_t) {
    ThermalNetwork net;

    ThermalNode node;
    node.label = "radiating_body";
    node.volume = 1e-3;
    node.material.density = mass / node.volume;
    node.material.specific_heat = cp;
    node.state.temperature = initial_t;

    node.boundary.type = BoundaryType::Radiation;
    node.boundary.surface_area = area;
    node.boundary.emissivity = emissivity;
    node.boundary.ambient_temperature = ambient_t;

    net.add_node(node);
    return net;
}

} // namespace shunya::core::domain::thermal
