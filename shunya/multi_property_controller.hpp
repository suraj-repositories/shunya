#pragma once

#include <QAbstractListModel>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "shunya/core/domain/multi_property_analyzer.hpp"
#include "shunya/core/domain/command_stack.hpp"

namespace shunya::gui {

class ShunyaMultiPropertyController : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int selectedEntityCount READ selectedEntityCount NOTIFY selectionChanged)
    Q_PROPERTY(int propertyCount READ propertyCount NOTIFY propertyCountChanged)
    Q_PROPERTY(QString selectionDescription READ selectionDescription NOTIFY selectionChanged)
    Q_PROPERTY(bool hasMultiSelection READ hasMultiSelection NOTIFY selectionChanged)

public:
    enum MultiPropertyRoles {
        PropertyIdRole = Qt::UserRole + 1,
        NameRole,
        DisplayNameRole,
        CategoryRole,
        TypeRole,
        ValueRole,
        UnitRole,
        AllowedUnitsRole,
        IsMixedRole,
        IsReadOnlyRole
    };
    Q_ENUM(MultiPropertyRoles)

    explicit ShunyaMultiPropertyController(QObject* parent = nullptr);
    ~ShunyaMultiPropertyController() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    int selectedEntityCount() const { return static_cast<int>(sheets_.size()); }
    int propertyCount() const { return static_cast<int>(intersected_props_.size()); }
    QString selectionDescription() const { return selection_description_; }
    bool hasMultiSelection() const { return sheets_.size() > 1; }

    void setCommandStack(core::domain::CommandStack* stack) { command_stack_ = stack; }
    void setSelectedSheets(std::vector<std::shared_ptr<core::domain::PropertySheet>> sheets, const QString& description = "");

    Q_INVOKABLE bool setBatchPropertyValue(const QString& propertyId, const QVariant& newValue);
    Q_INVOKABLE void selectDemoMultiSet(const QString& setType);

signals:
    void selectionChanged();
    void propertyCountChanged();
    void batchPropertyUpdated(const QString& propertyId, const QVariant& newValue);

private:
    void update_intersection();
    QVariant parameter_value_to_qvariant(const core::domain::ParameterValue& val, const QString& visualType, bool isMixed) const;
    core::domain::ParameterValue qvariant_to_parameter_value(const QVariant& val, const QString& visualType) const;

    std::vector<std::shared_ptr<core::domain::PropertySheet>> sheets_;
    std::vector<core::domain::IntersectedProperty> intersected_props_;
    QString selection_description_{"No Multi-Selection"};
    core::domain::CommandStack* command_stack_{nullptr};
};

} // namespace shunya::gui
