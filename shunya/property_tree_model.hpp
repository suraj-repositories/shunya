#pragma once

#include <QAbstractListModel>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "shunya/core/domain/property_node.hpp"
#include "shunya/core/domain/parameter_reference_manager.hpp"

namespace shunya::gui {

struct DisplayRow {
    bool is_category = false;
    QString category_name;
    QString category_display_name;
    bool is_expanded = true;

    // Property item fields
    QString id;
    QString name;
    QString display_name;
    QString description;
    QString visual_type; // "number", "quantity", "vec3", "bool", "text", "enum", "color"
    QVariant value;
    QString unit_symbol;
    QStringList allowed_units;
    QString formula;
    QVariant min_value;
    QVariant max_value;
    QStringList enum_options;
    bool is_read_only = false;
};

class ShunyaPropertyTreeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int propertyCount READ propertyCount NOTIFY propertyCountChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
    Q_PROPERTY(QString inspectedTitle READ inspectedTitle NOTIFY inspectedTitleChanged)

public:
    enum PropertyRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DisplayNameRole,
        CategoryRole,
        CategoryDisplayNameRole,
        TypeRole,
        ValueRole,
        UnitRole,
        AllowedUnitsRole,
        FormulaRole,
        MinRole,
        MaxRole,
        EnumOptionsRole,
        IsReadOnlyRole,
        IsCategoryRole,
        IsExpandedRole
    };
    Q_ENUM(PropertyRoles)

    explicit ShunyaPropertyTreeModel(QObject* parent = nullptr);
    ~ShunyaPropertyTreeModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return filter_text_; }
    void setFilterText(const QString& filter);

    int propertyCount() const;
    bool hasSelection() const { return sheet_ != nullptr; }
    QString inspectedTitle() const { return inspected_title_; }

    void setPropertySheet(std::shared_ptr<core::domain::PropertySheet> sheet, const QString& title = "");
    std::shared_ptr<core::domain::PropertySheet> propertySheet() const { return sheet_; }

    void setParameterReferenceManager(std::shared_ptr<core::domain::ParameterReferenceManager> param_mgr);

    Q_INVOKABLE bool setPropertyValue(const QString& propertyId, const QVariant& newValue);
    Q_INVOKABLE bool setPropertyFormula(const QString& propertyId, const QString& formula);
    Q_INVOKABLE bool setPropertyUnit(const QString& propertyId, const QString& newUnit);
    Q_INVOKABLE void toggleCategory(const QString& categoryName);
    Q_INVOKABLE void inspectDemoObject(const QString& demoType = "Resistor");

signals:
    void filterTextChanged();
    void propertyCountChanged();
    void hasSelectionChanged();
    void inspectedTitleChanged();
    void propertyValueChanged(const QString& id, const QVariant& value);

private:
    void rebuild_display_rows();
    QVariant parameter_value_to_qvariant(const core::domain::ParameterValue& val, const QString& visualType) const;
    core::domain::ParameterValue qvariant_to_parameter_value(const QVariant& val, const QString& visualType) const;

    std::shared_ptr<core::domain::PropertySheet> sheet_;
    std::shared_ptr<core::domain::ParameterReferenceManager> param_mgr_;
    std::vector<DisplayRow> display_rows_;
    QString filter_text_;
    QString inspected_title_{"No Entity Selected"};
    std::unordered_map<std::string, bool> category_expansion_state_;
};

} // namespace shunya::gui
