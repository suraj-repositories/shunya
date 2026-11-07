#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/units.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>

namespace shunya::core::base {

class ExpressionEvaluator {
public:
    using SymbolResolver = std::function<Result<double>(std::string_view name)>;

    ExpressionEvaluator() = default;
    explicit ExpressionEvaluator(SymbolResolver resolver) : resolver_(std::move(resolver)) {}

    void set_symbol_resolver(SymbolResolver resolver) { resolver_ = std::move(resolver); }

    /// Evaluates an algebraic expression (e.g. "=width * 2 + 5mm", "sin(pi/4) * radius", "sqrt(a^2 + b^2)")
    [[nodiscard]] Result<double> evaluate(std::string_view expr) const;

    /// Evaluates with an explicit one-off context map of variable values
    [[nodiscard]] Result<double> evaluate_with_context(
        std::string_view expr,
        const std::unordered_map<std::string, double>& context
    ) const;

    /// Extracts all variable identifiers referenced in the expression (excluding math functions and constants)
    [[nodiscard]] static std::vector<std::string> extract_variables(std::string_view expr);

    /// Checks if expression syntax is valid without evaluating variable values
    [[nodiscard]] static Result<void> validate_syntax(std::string_view expr);

private:
    SymbolResolver resolver_;
};

} // namespace shunya::core::base
