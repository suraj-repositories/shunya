#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"

#include <memory>
#include <string>
#include <vector>

namespace shunya::mat {

/**
 * @brief Abstract interface for modular material formation scenario strategies.
 * (Roadmap task PH4-03.01.001)
 */
class IFormationScenarioHandler {
public:
    virtual ~IFormationScenarioHandler() = default;

    [[nodiscard]] virtual FormationScenario scenario() const noexcept = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string description() const = 0;

    /**
     * @brief Validates scenario-specific physical/chemical constraints before calculation.
     */
    [[nodiscard]] virtual core::base::Result<void> validate_preconditions(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems) const = 0;

    /**
     * @brief Computes specialized scenario behaviors (alloys, compounds, solutions, layers, composites, mixtures).
     */
    virtual void process_properties(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems,
        const std::vector<ElementBehaviorReport>& behaviors,
        MixtureEvaluationResult& result) const = 0;
};

// --- Specialized Handlers for all 6 Formation Scenarios ---

class PhysicalMixtureHandler final : public IFormationScenarioHandler {
public:
    [[nodiscard]] FormationScenario scenario() const noexcept override { return FormationScenario::PhysicalMixture; }
    [[nodiscard]] std::string name() const override { return "Physical Mixture"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] core::base::Result<void> validate_preconditions(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems) const override;
    void process_properties(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems,
        const std::vector<ElementBehaviorReport>& behaviors,
        MixtureEvaluationResult& result) const override;
};

class AlloyHandler final : public IFormationScenarioHandler {
public:
    [[nodiscard]] FormationScenario scenario() const noexcept override { return FormationScenario::Alloy; }
    [[nodiscard]] std::string name() const override { return "Alloy"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] core::base::Result<void> validate_preconditions(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems) const override;
    void process_properties(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems,
        const std::vector<ElementBehaviorReport>& behaviors,
        MixtureEvaluationResult& result) const override;
};

class SolutionHandler final : public IFormationScenarioHandler {
public:
    [[nodiscard]] FormationScenario scenario() const noexcept override { return FormationScenario::Solution; }
    [[nodiscard]] std::string name() const override { return "Solution"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] core::base::Result<void> validate_preconditions(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems) const override;
    void process_properties(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems,
        const std::vector<ElementBehaviorReport>& behaviors,
        MixtureEvaluationResult& result) const override;
};

class ChemicalCompoundHandler final : public IFormationScenarioHandler {
public:
    [[nodiscard]] FormationScenario scenario() const noexcept override { return FormationScenario::ChemicalCompound; }
    [[nodiscard]] std::string name() const override { return "Chemical Compound"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] core::base::Result<void> validate_preconditions(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems) const override;
    void process_properties(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems,
        const std::vector<ElementBehaviorReport>& behaviors,
        MixtureEvaluationResult& result) const override;
};

class LayeredMaterialHandler final : public IFormationScenarioHandler {
public:
    [[nodiscard]] FormationScenario scenario() const noexcept override { return FormationScenario::LayeredMaterial; }
    [[nodiscard]] std::string name() const override { return "Layered Material"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] core::base::Result<void> validate_preconditions(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems) const override;
    void process_properties(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems,
        const std::vector<ElementBehaviorReport>& behaviors,
        MixtureEvaluationResult& result) const override;
};

class CompositeMaterialHandler final : public IFormationScenarioHandler {
public:
    [[nodiscard]] FormationScenario scenario() const noexcept override { return FormationScenario::CompositeMaterial; }
    [[nodiscard]] std::string name() const override { return "Composite Material"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] core::base::Result<void> validate_preconditions(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems) const override;
    void process_properties(
        const MixtureExperiment& experiment,
        const std::vector<const matdb::ElementRecord*>& elems,
        const std::vector<ElementBehaviorReport>& behaviors,
        MixtureEvaluationResult& result) const override;
};

/**
 * @brief Dispatcher and registry for modular material formation scenario strategies.
 */
class FormationScenarioDispatcher {
public:
    [[nodiscard]] static const IFormationScenarioHandler& get_handler(FormationScenario scenario);
    [[nodiscard]] static bool is_scenario_supported(FormationScenario scenario) noexcept;
    [[nodiscard]] static std::vector<FormationScenario> available_scenarios();
};

} // namespace shunya::mat
