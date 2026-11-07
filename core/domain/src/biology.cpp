#include "shunya/core/domain/biology.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::core::domain::bio {

// =============================================================================
// CellColonySimulator Implementation
// =============================================================================

CellColonySimulator::CellColonySimulator(BioSimConfig config)
    : config_(config), rng_state_(config.random_seed ? config.random_seed : 42) {}

uint64_t CellColonySimulator::next_random() {
    rng_state_ ^= (rng_state_ >> 12);
    rng_state_ ^= (rng_state_ << 25);
    rng_state_ ^= (rng_state_ >> 27);
    return rng_state_ * 0x2545F4914F6CDD1DULL;
}

double CellColonySimulator::next_uniform() {
    return static_cast<double>(next_random() >> 11) * (1.0 / 9007199254740992.0);
}

void CellColonySimulator::add_cell(double x, double y, double mass) {
    CellAgent cell;
    cell.id = next_cell_id_++;
    cell.lineage_id = cell.id;
    cell.x = std::clamp(x, 0.0, config_.domain_width);
    cell.y = std::clamp(y, 0.0, config_.domain_height);
    cell.mass = mass;
    cell.internal_energy = 1.0;
    cell.phase = CellCyclePhase::G1;
    cell.age = 0.0;
    cell.is_alive = true;
    cells_.push_back(cell);
}

base::Result<void> CellColonySimulator::step(double dt) {
    if (dt <= 0.0) {
        return base::Result<void>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::Simulation,
            .message = "Time step dt must be positive",
            .details = {}
        });
    }

    // Monod nutrient growth rate: mu = mu_max * S / (K_s + S)
    double mu = 0.0;
    if (substrate_ > 0.0) {
        mu = config_.mu_max * substrate_ / (config_.k_s + substrate_);
    }

    std::vector<CellAgent> new_daughters;
    double substrate_consumed = 0.0;

    for (auto& cell : cells_) {
        if (!cell.is_alive) continue;

        // 1. Nutrient uptake & mass accumulation
        double dm = mu * cell.mass * dt;
        cell.mass += dm;
        substrate_consumed += 0.01 * dm;
        cell.age += dt;

        // 2. Random motility (Brownian walk)
        double step_size = 0.05 * std::sqrt(dt);
        double dx = (next_uniform() * 2.0 - 1.0) * step_size;
        double dy = (next_uniform() * 2.0 - 1.0) * step_size;
        cell.x = std::clamp(cell.x + dx, 0.0, config_.domain_width);
        cell.y = std::clamp(cell.y + dy, 0.0, config_.domain_height);

        // 3. Cell cycle phase update
        if (cell.mass < 1.3) {
            cell.phase = CellCyclePhase::G1;
        } else if (cell.mass < 1.7) {
            cell.phase = CellCyclePhase::S;
        } else if (cell.mass < config_.division_mass) {
            cell.phase = CellCyclePhase::G2;
        } else {
            cell.phase = CellCyclePhase::M;
        }

        // 4. Stochastic apoptosis/death
        if (next_uniform() < (config_.death_rate * dt)) {
            cell.is_alive = false;
            cell.phase = CellCyclePhase::Apoptotic;
            continue;
        }

        // 5. Mitosis / Cell division
        if (cell.mass >= config_.division_mass) {
            cell.mass *= 0.5;
            cell.phase = CellCyclePhase::G1;

            CellAgent daughter = cell;
            daughter.id = next_cell_id_++;
            daughter.age = 0.0;
            daughter.x = std::clamp(cell.x + 0.01, 0.0, config_.domain_width);
            daughter.y = std::clamp(cell.y + 0.01, 0.0, config_.domain_height);
            new_daughters.push_back(daughter);
        }
    }

    // Deplete nutrient
    substrate_ = std::max(0.0, substrate_ - substrate_consumed);

    // Append new daughter cells
    for (auto& d : new_daughters) {
        cells_.push_back(d);
    }

    return base::Result<void>::ok();
}

PopulationMetrics CellColonySimulator::calculate_metrics() const noexcept {
    PopulationMetrics metrics;
    double total_mass = 0.0;

    for (const auto& cell : cells_) {
        if (cell.is_alive) {
            metrics.live_count++;
            total_mass += cell.mass;
        } else {
            metrics.dead_count++;
        }
    }

    metrics.total_biomass = total_mass;
    metrics.mean_cell_mass = (metrics.live_count > 0) ? (total_mass / static_cast<double>(metrics.live_count)) : 0.0;
    return metrics;
}

void CellColonySimulator::reset(uint64_t new_seed) noexcept {
    cells_.clear();
    next_cell_id_ = 1;
    rng_state_ = (new_seed != 0) ? new_seed : 42;
    substrate_ = 1.0;
}

// =============================================================================
// PopulationValidationBenchmark Implementation
// =============================================================================

double PopulationValidationBenchmark::analytical_logistic_growth(
    double n0, double k_capacity, double r_growth, double t) noexcept {
    if (n0 <= 0.0 || k_capacity <= 0.0) return 0.0;
    double ratio = (k_capacity - n0) / n0;
    return k_capacity / (1.0 + ratio * std::exp(-r_growth * t));
}

std::pair<double, double> PopulationValidationBenchmark::lotka_volterra_step(
    double prey,
    double predator,
    const LotkaVolterraParams& params,
    double dt) noexcept {
    double d_prey = (params.alpha * prey - params.beta * prey * predator) * dt;
    double d_predator = (params.delta * prey * predator - params.gamma * predator) * dt;

    double new_prey = std::max(0.0, prey + d_prey);
    double new_predator = std::max(0.0, predator + d_predator);
    return {new_prey, new_predator};
}

} // namespace shunya::core::domain::bio
