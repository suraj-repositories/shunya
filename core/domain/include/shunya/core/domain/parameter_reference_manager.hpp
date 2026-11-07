#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/expression_evaluator.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <optional>

namespace shunya::core::domain {

struct ParameterRecord {
    std::string name;
    std::string formula;
    double evaluated_value = 0.0;
    std::string unit;
    std::unordered_set<std::string> dependencies;
    std::unordered_set<std::string> dependents;
    bool is_valid = true;
    std::string error_message;
};

class ParameterReferenceManager {
public:
    using ChangeListener = std::function<void(const std::string& name, double new_value)>;

    ParameterReferenceManager();

    [[nodiscard]] base::Result<void> set_parameter(
        std::string_view name,
        std::string_view formula_or_value,
        std::string_view unit = ""
    );

    [[nodiscard]] base::Result<void> set_numeric_parameter(
        std::string_view name,
        double value,
        std::string_view unit = ""
    );

    [[nodiscard]] base::Result<double> get_value(std::string_view name) const;
    [[nodiscard]] base::Result<std::string> get_formula(std::string_view name) const;
    [[nodiscard]] base::Result<std::string> get_unit(std::string_view name) const;
    [[nodiscard]] bool has_parameter(std::string_view name) const noexcept;
    [[nodiscard]] base::Result<void> remove_parameter(std::string_view name);

    [[nodiscard]] std::vector<ParameterRecord> all_parameters() const;
    [[nodiscard]] std::unordered_set<std::string> dependencies_of(std::string_view name) const;
    [[nodiscard]] std::unordered_set<std::string> dependents_of(std::string_view name) const;

    [[nodiscard]] base::Result<double> evaluate_expression(std::string_view expr) const;

    void add_change_listener(ChangeListener listener) {
        listeners_.push_back(std::move(listener));
    }

    void clear() noexcept;

private:
    [[nodiscard]] bool would_create_cycle(
        const std::string& target,
        const std::unordered_set<std::string>& new_dependencies
    ) const;

    void reevaluate_dependents(const std::string& changed_param);

    std::unordered_map<std::string, ParameterRecord> parameters_;
    std::vector<ChangeListener> listeners_;
};

} // namespace shunya::core::domain
