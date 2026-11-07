#include "shunya/core/domain/parameter_reference_manager.hpp"
#include <queue>
#include <sstream>

namespace shunya::core::domain {

ParameterReferenceManager::ParameterReferenceManager() = default;

bool ParameterReferenceManager::would_create_cycle(
    const std::string& target,
    const std::unordered_set<std::string>& new_dependencies
) const {
    if (new_dependencies.count(target) > 0) return true;

    // Check if any node in new_dependencies can reach target through its own dependencies
    std::unordered_set<std::string> visited;
    std::queue<std::string> queue;

    for (const auto& dep : new_dependencies) {
        queue.push(dep);
        visited.insert(dep);
    }

    while (!queue.empty()) {
        std::string curr = queue.front();
        queue.pop();

        if (curr == target) return true;

        auto it = parameters_.find(curr);
        if (it != parameters_.end()) {
            for (const auto& next_dep : it->second.dependencies) {
                if (next_dep == target) return true;
                if (visited.count(next_dep) == 0) {
                    visited.insert(next_dep);
                    queue.push(next_dep);
                }
            }
        }
    }

    return false;
}

base::Result<void> ParameterReferenceManager::set_parameter(
    std::string_view name,
    std::string_view formula_or_value,
    std::string_view unit
) {
    std::string param_name(name);
    std::string formula(formula_or_value);

    // Extract dependencies from formula
    auto extracted = base::ExpressionEvaluator::extract_variables(formula);
    std::unordered_set<std::string> new_deps(extracted.begin(), extracted.end());

    // Cycle detection check
    if (would_create_cycle(param_name, new_deps)) {
        return base::Result<void>::err({
            .code = 3040,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Circular dependency detected when defining parameter '" + param_name + "' with formula '" + formula + "'",
            .details = ""
        });
    }

    // Clean up old dependencies if parameter already existed
    auto old_it = parameters_.find(param_name);
    if (old_it != parameters_.end()) {
        for (const auto& old_dep : old_it->second.dependencies) {
            auto dep_it = parameters_.find(old_dep);
            if (dep_it != parameters_.end()) {
                dep_it->second.dependents.erase(param_name);
            }
        }
    }

    // Create / update record
    ParameterRecord rec;
    rec.name = param_name;
    rec.formula = formula;
    rec.unit = std::string(unit);
    rec.dependencies = new_deps;
    if (old_it != parameters_.end()) {
        rec.dependents = std::move(old_it->second.dependents);
    }

    // Register dependency links
    for (const auto& dep : new_deps) {
        parameters_[dep].dependents.insert(param_name);
    }

    // Evaluate the expression
    base::ExpressionEvaluator evaluator([this](std::string_view var) -> base::Result<double> {
        auto it = parameters_.find(std::string(var));
        if (it != parameters_.end() && it->second.is_valid) {
            return base::Result<double>::ok(it->second.evaluated_value);
        }
        return base::Result<double>::err({
            .code = 3041,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter not found or invalid: " + std::string(var),
            .details = ""
        });
    });

    auto eval_res = evaluator.evaluate(formula);
    if (eval_res.is_ok()) {
        rec.evaluated_value = eval_res.value();
        rec.is_valid = true;
        rec.error_message.clear();
    } else {
        rec.is_valid = false;
        rec.error_message = eval_res.error().message;
    }

    parameters_[param_name] = std::move(rec);

    // Re-evaluate any dependents of this parameter
    reevaluate_dependents(param_name);

    // Notify listeners
    if (parameters_[param_name].is_valid) {
        for (const auto& listener : listeners_) {
            if (listener) listener(param_name, parameters_[param_name].evaluated_value);
        }
    }

    if (!parameters_[param_name].is_valid) {
        return base::Result<void>::err({
            .code = 3042,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter '" + param_name + "' evaluation failed: " + parameters_[param_name].error_message,
            .details = ""
        });
    }

    return base::Result<void>::ok();
}

base::Result<void> ParameterReferenceManager::set_numeric_parameter(
    std::string_view name,
    double value,
    std::string_view unit
) {
    std::ostringstream ss;
    ss << value;
    return set_parameter(name, ss.str(), unit);
}

void ParameterReferenceManager::reevaluate_dependents(const std::string& changed_param) {
    auto it = parameters_.find(changed_param);
    if (it == parameters_.end()) return;

    // 1. Collect all transitive dependents
    std::unordered_set<std::string> all_descendants;
    std::queue<std::string> q;
    for (const auto& dep : it->second.dependents) {
        q.push(dep);
        all_descendants.insert(dep);
    }
    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();
        auto p_it = parameters_.find(curr);
        if (p_it != parameters_.end()) {
            for (const auto& next_dep : p_it->second.dependents) {
                if (all_descendants.insert(next_dep).second) {
                    q.push(next_dep);
                }
            }
        }
    }

    if (all_descendants.empty()) return;

    // 2. Compute in-degree within descendants subgraph
    std::unordered_map<std::string, int> in_degree;
    for (const auto& name : all_descendants) {
        in_degree[name] = 0;
    }
    for (const auto& name : all_descendants) {
        auto p_it = parameters_.find(name);
        if (p_it != parameters_.end()) {
            for (const auto& dep_of_name : p_it->second.dependencies) {
                if (all_descendants.count(dep_of_name) > 0) {
                    in_degree[name]++;
                }
            }
        }
    }

    // 3. Queue nodes with in-degree 0
    std::queue<std::string> ready_queue;
    for (const auto& [name, deg] : in_degree) {
        if (deg == 0) {
            ready_queue.push(name);
        }
    }

    // 4. Evaluate in topological order
    base::ExpressionEvaluator evaluator([this](std::string_view var) -> base::Result<double> {
        auto p_it = parameters_.find(std::string(var));
        if (p_it != parameters_.end() && p_it->second.is_valid) {
            return base::Result<double>::ok(p_it->second.evaluated_value);
        }
        return base::Result<double>::err({
            .code = 3043,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter not available: " + std::string(var),
            .details = ""
        });
    });

    while (!ready_queue.empty()) {
        std::string curr = ready_queue.front();
        ready_queue.pop();

        auto p_it = parameters_.find(curr);
        if (p_it != parameters_.end()) {
            auto eval_res = evaluator.evaluate(p_it->second.formula);
            if (eval_res.is_ok()) {
                p_it->second.evaluated_value = eval_res.value();
                p_it->second.is_valid = true;
                p_it->second.error_message.clear();
                for (const auto& listener : listeners_) {
                    if (listener) listener(curr, p_it->second.evaluated_value);
                }
            } else {
                p_it->second.is_valid = false;
                p_it->second.error_message = eval_res.error().message;
            }

            for (const auto& next_dep : p_it->second.dependents) {
                if (all_descendants.count(next_dep) > 0) {
                    in_degree[next_dep]--;
                    if (in_degree[next_dep] == 0) {
                        ready_queue.push(next_dep);
                    }
                }
            }
        }
    }
}

base::Result<double> ParameterReferenceManager::get_value(std::string_view name) const {
    auto it = parameters_.find(std::string(name));
    if (it == parameters_.end()) {
        return base::Result<double>::err({
            .code = 3044,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter not found: " + std::string(name),
            .details = ""
        });
    }
    if (!it->second.is_valid) {
        return base::Result<double>::err({
            .code = 3045,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter '" + std::string(name) + "' is invalid: " + it->second.error_message,
            .details = ""
        });
    }
    return base::Result<double>::ok(it->second.evaluated_value);
}

base::Result<std::string> ParameterReferenceManager::get_formula(std::string_view name) const {
    auto it = parameters_.find(std::string(name));
    if (it == parameters_.end()) {
        return base::Result<std::string>::err({
            .code = 3044,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter not found: " + std::string(name),
            .details = ""
        });
    }
    return base::Result<std::string>::ok(it->second.formula);
}

base::Result<std::string> ParameterReferenceManager::get_unit(std::string_view name) const {
    auto it = parameters_.find(std::string(name));
    if (it == parameters_.end()) {
        return base::Result<std::string>::err({
            .code = 3044,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter not found: " + std::string(name),
            .details = ""
        });
    }
    return base::Result<std::string>::ok(it->second.unit);
}

bool ParameterReferenceManager::has_parameter(std::string_view name) const noexcept {
    return parameters_.find(std::string(name)) != parameters_.end();
}

base::Result<void> ParameterReferenceManager::remove_parameter(std::string_view name) {
    std::string param_name(name);
    auto it = parameters_.find(param_name);
    if (it == parameters_.end()) {
        return base::Result<void>::err({
            .code = 3044,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter not found: " + param_name,
            .details = ""
        });
    }

    for (const auto& dep : it->second.dependencies) {
        auto dep_it = parameters_.find(dep);
        if (dep_it != parameters_.end()) {
            dep_it->second.dependents.erase(param_name);
        }
    }

    std::unordered_set<std::string> dependents = it->second.dependents;
    parameters_.erase(it);

    for (const auto& dep : dependents) {
        reevaluate_dependents(dep);
    }

    return base::Result<void>::ok();
}

std::vector<ParameterRecord> ParameterReferenceManager::all_parameters() const {
    std::vector<ParameterRecord> res;
    res.reserve(parameters_.size());
    for (const auto& [_, rec] : parameters_) {
        res.push_back(rec);
    }
    return res;
}

std::unordered_set<std::string> ParameterReferenceManager::dependencies_of(std::string_view name) const {
    auto it = parameters_.find(std::string(name));
    if (it != parameters_.end()) {
        return it->second.dependencies;
    }
    return {};
}

std::unordered_set<std::string> ParameterReferenceManager::dependents_of(std::string_view name) const {
    auto it = parameters_.find(std::string(name));
    if (it != parameters_.end()) {
        return it->second.dependents;
    }
    return {};
}

base::Result<double> ParameterReferenceManager::evaluate_expression(std::string_view expr) const {
    base::ExpressionEvaluator evaluator([this](std::string_view var) -> base::Result<double> {
        return get_value(var);
    });
    return evaluator.evaluate(expr);
}

void ParameterReferenceManager::clear() noexcept {
    parameters_.clear();
}

} // namespace shunya::core::domain
