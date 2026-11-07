#include "quantity_input_controller.hpp"
#include <iomanip>
#include <sstream>

namespace shunya::gui {

ShunyaQuantityInputController::ShunyaQuantityInputController(QObject* parent)
    : QObject(parent)
{
}

ShunyaQuantityInputController::~ShunyaQuantityInputController() = default;

void ShunyaQuantityInputController::setParameterReferenceManager(std::shared_ptr<core::domain::ParameterReferenceManager> param_mgr) {
    param_mgr_ = std::move(param_mgr);
}

QVariantMap ShunyaQuantityInputController::parseQuantity(const QString& input, const QString& fallbackUnit) {
    QVariantMap res;
    QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        res["success"] = false;
        res["error"] = "Input is empty";
        return res;
    }

    // Check if input is a formula expression starting with '='
    if (trimmed.startsWith('=')) {
        return evaluateExpression(trimmed);
    }

    // Try parsing as quantity string (e.g., "150 kPa", "25 mm", "3.5 kOhm")
    auto parse_res = core::base::units::parse_quantity(trimmed.toStdString());
    if (parse_res.is_ok()) {
        const auto& q = parse_res.value();
        res["success"] = true;
        res["canonicalValue"] = q.value;

        // Determine which unit symbol was present
        std::string unit_used;
        // Check if trimmed ends with known unit
        for (const auto& u : core::base::units::UnitRegistry::instance().get_units_for_dimension(q.dimension)) {
            if (trimmed.endsWith(QString::fromStdString(u))) {
                unit_used = u;
                break;
            }
        }
        if (unit_used.empty() && !fallbackUnit.isEmpty()) {
            unit_used = fallbackUnit.toStdString();
        }

        double disp_val = q.value;
        if (!unit_used.empty()) {
            auto from_res = core::base::units::UnitRegistry::instance().from_canonical(q, unit_used);
            if (from_res.is_ok()) {
                disp_val = from_res.value();
            }
        }

        res["displayValue"] = disp_val;
        res["unit"] = QString::fromStdString(unit_used);
        res["dimension"] = QString::fromStdString(q.dimension.symbol());
        return res;
    }

    // If unit parsing failed because it was just a plain number without unit and fallbackUnit provided
    bool ok_num = false;
    double plain_num = trimmed.toDouble(&ok_num);
    if (ok_num && !fallbackUnit.isEmpty()) {
        auto can_res = core::base::units::UnitRegistry::instance().to_canonical(plain_num, fallbackUnit.toStdString());
        if (can_res.is_ok()) {
            res["success"] = true;
            res["canonicalValue"] = can_res.value().value;
            res["displayValue"] = plain_num;
            res["unit"] = fallbackUnit;
            res["dimension"] = QString::fromStdString(can_res.value().dimension.symbol());
            return res;
        }
    } else if (ok_num) {
        res["success"] = true;
        res["canonicalValue"] = plain_num;
        res["displayValue"] = plain_num;
        res["unit"] = "";
        res["dimension"] = "1";
        return res;
    }

    res["success"] = false;
    res["error"] = QString::fromStdString(parse_res.error().message);
    return res;
}

QVariantMap ShunyaQuantityInputController::evaluateExpression(const QString& formula, const QVariantMap& context) {
    QVariantMap res;
    std::unordered_map<std::string, double> ctx;
    for (auto it = context.begin(); it != context.end(); ++it) {
        ctx[it.key().toStdString()] = it.value().toDouble();
    }

    core::base::ExpressionEvaluator evaluator([this, &ctx](std::string_view name) -> core::base::Result<double> {
        std::string s_name(name);
        auto it = ctx.find(s_name);
        if (it != ctx.end()) {
            return core::base::Result<double>::ok(it->second);
        }
        if (param_mgr_) {
            return param_mgr_->get_value(name);
        }
        return core::base::Result<double>::err({
            .code = 3060,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Unknown variable: " + s_name,
            .details = ""
        });
    });

    auto eval_res = evaluator.evaluate(formula.toStdString());
    if (eval_res.is_ok()) {
        res["success"] = true;
        res["value"] = eval_res.value();
        res["canonicalValue"] = eval_res.value();
        res["displayValue"] = eval_res.value();
        emit expressionEvaluated(formula, eval_res.value());
    } else {
        res["success"] = false;
        res["error"] = QString::fromStdString(eval_res.error().message);
    }

    return res;
}

QString ShunyaQuantityInputController::convertUnit(double value, const QString& fromUnit, const QString& toUnit) {
    if (fromUnit == toUnit) return QString::number(value, 'g', 6);

    auto can_res = core::base::units::UnitRegistry::instance().to_canonical(value, fromUnit.toStdString());
    if (can_res.is_err()) return QString::number(value, 'g', 6);

    auto target_res = core::base::units::UnitRegistry::instance().from_canonical(can_res.value(), toUnit.toStdString());
    if (target_res.is_err()) return QString::number(value, 'g', 6);

    return QString::number(target_res.value(), 'g', 6);
}

QStringList ShunyaQuantityInputController::getCompatibleUnits(const QString& unitSymbol) {
    QStringList result;
    auto compatible = core::base::units::UnitRegistry::instance().get_compatible_units(unitSymbol.toStdString());
    for (const auto& u : compatible) {
        result.append(QString::fromStdString(u));
    }
    return result;
}

QString ShunyaQuantityInputController::formatQuantity(double value, const QString& unit, int precision) {
    if (unit.isEmpty()) {
        return QString::number(value, 'f', precision);
    }
    return QString("%1 %2").arg(QString::number(value, 'f', precision)).arg(unit);
}

bool ShunyaQuantityInputController::isValidFormula(const QString& formula) {
    auto res = core::base::ExpressionEvaluator::validate_syntax(formula.toStdString());
    return res.is_ok();
}

} // namespace shunya::gui
