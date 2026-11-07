#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <memory>

#include "shunya/core/base/units.hpp"
#include "shunya/core/base/expression_evaluator.hpp"
#include "shunya/core/domain/parameter_reference_manager.hpp"

namespace shunya::gui {

class ShunyaQuantityInputController : public QObject {
    Q_OBJECT

public:
    explicit ShunyaQuantityInputController(QObject* parent = nullptr);
    ~ShunyaQuantityInputController() override;

    void setParameterReferenceManager(std::shared_ptr<core::domain::ParameterReferenceManager> param_mgr);

    Q_INVOKABLE QVariantMap parseQuantity(const QString& input, const QString& fallbackUnit = "");
    Q_INVOKABLE QVariantMap evaluateExpression(const QString& formula, const QVariantMap& context = {});
    Q_INVOKABLE QString convertUnit(double value, const QString& fromUnit, const QString& toUnit);
    Q_INVOKABLE QStringList getCompatibleUnits(const QString& unitSymbol);
    Q_INVOKABLE QString formatQuantity(double value, const QString& unit, int precision = 4);
    Q_INVOKABLE bool isValidFormula(const QString& formula);

signals:
    void expressionEvaluated(const QString& formula, double result);

private:
    std::shared_ptr<core::domain::ParameterReferenceManager> param_mgr_;
};

} // namespace shunya::gui
