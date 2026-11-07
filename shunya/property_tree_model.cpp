#include "property_tree_model.hpp"
#include <QDebug>
#include <chrono>

namespace shunya::gui {

ShunyaPropertyTreeModel::ShunyaPropertyTreeModel(QObject* parent)
    : QAbstractListModel(parent)
{
    // Initialize default demo inspection
    inspectDemoObject("Resistor");
}

ShunyaPropertyTreeModel::~ShunyaPropertyTreeModel() = default;

QHash<int, QByteArray> ShunyaPropertyTreeModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "propertyId";
    roles[NameRole] = "propertyName";
    roles[DisplayNameRole] = "displayName";
    roles[CategoryRole] = "category";
    roles[CategoryDisplayNameRole] = "categoryDisplayName";
    roles[TypeRole] = "visualType";
    roles[ValueRole] = "value";
    roles[UnitRole] = "unitSymbol";
    roles[AllowedUnitsRole] = "allowedUnits";
    roles[FormulaRole] = "formula";
    roles[MinRole] = "minValue";
    roles[MaxRole] = "maxValue";
    roles[EnumOptionsRole] = "enumOptions";
    roles[IsReadOnlyRole] = "isReadOnly";
    roles[IsCategoryRole] = "isCategory";
    roles[IsExpandedRole] = "isExpanded";
    return roles;
}

int ShunyaPropertyTreeModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(display_rows_.size());
}

int ShunyaPropertyTreeModel::propertyCount() const {
    int count = 0;
    for (const auto& row : display_rows_) {
        if (!row.is_category) ++count;
    }
    return count;
}

QVariant ShunyaPropertyTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(display_rows_.size())) {
        return QVariant();
    }

    const auto& row = display_rows_[index.row()];

    switch (role) {
        case IdRole:                  return row.id;
        case NameRole:                return row.name;
        case DisplayNameRole:         return row.display_name;
        case CategoryRole:            return row.category_name;
        case CategoryDisplayNameRole: return row.category_display_name;
        case TypeRole:                return row.visual_type;
        case ValueRole:               return row.value;
        case UnitRole:                return row.unit_symbol;
        case AllowedUnitsRole:        return row.allowed_units;
        case FormulaRole:             return row.formula;
        case MinRole:                 return row.min_value;
        case MaxRole:                 return row.max_value;
        case EnumOptionsRole:         return row.enum_options;
        case IsReadOnlyRole:          return row.is_read_only;
        case IsCategoryRole:          return row.is_category;
        case IsExpandedRole:          return row.is_expanded;
        default:                      return QVariant();
    }
}

bool ShunyaPropertyTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(display_rows_.size())) {
        return false;
    }

    auto& row = display_rows_[index.row()];
    if (row.is_category || row.is_read_only) return false;

    if (role == ValueRole) {
        return setPropertyValue(row.id, value);
    }
    if (role == FormulaRole) {
        return setPropertyFormula(row.id, value.toString());
    }
    if (role == UnitRole) {
        return setPropertyUnit(row.id, value.toString());
    }

    return false;
}

void ShunyaPropertyTreeModel::setFilterText(const QString& filter) {
    if (filter_text_ == filter) return;
    filter_text_ = filter;
    emit filterTextChanged();
    rebuild_display_rows();
}

void ShunyaPropertyTreeModel::setPropertySheet(std::shared_ptr<core::domain::PropertySheet> sheet, const QString& title) {
    sheet_ = std::move(sheet);
    inspected_title_ = title.isEmpty() ? "Inspected Entity" : title;

    if (sheet_) {
        sheet_->add_listener([this](const std::string& id, const core::domain::ParameterValue& val) {
            QString qid = QString::fromStdString(id);
            for (size_t i = 0; i < display_rows_.size(); ++i) {
                if (!display_rows_[i].is_category && display_rows_[i].id == qid) {
                    display_rows_[i].value = parameter_value_to_qvariant(val, display_rows_[i].visual_type);
                    QModelIndex idx = index(static_cast<int>(i), 0);
                    emit dataChanged(idx, idx, {ValueRole});
                    emit propertyValueChanged(qid, display_rows_[i].value);
                    break;
                }
            }
        });
    }

    emit hasSelectionChanged();
    emit inspectedTitleChanged();
    rebuild_display_rows();
}

void ShunyaPropertyTreeModel::setParameterReferenceManager(std::shared_ptr<core::domain::ParameterReferenceManager> param_mgr) {
    param_mgr_ = std::move(param_mgr);
    if (param_mgr_) {
        param_mgr_->add_change_listener([this](const std::string& /*name*/, double /*val*/) {
            if (sheet_) {
                sheet_->refresh_values();
            }
        });
    }
}

bool ShunyaPropertyTreeModel::setPropertyValue(const QString& propertyId, const QVariant& newValue) {
    if (!sheet_) return false;

    std::string id_str = propertyId.toStdString();
    auto* prop = sheet_->find_property_mut(id_str);
    if (!prop || prop->is_read_only) return false;

    core::domain::ParameterValue pv = qvariant_to_parameter_value(newValue, QString::fromStdString(std::string(core::domain::property_visual_type_to_string(prop->visual_type))));
    auto res = sheet_->set_property_value(id_str, pv);
    if (res.is_err()) {
        qWarning() << "Failed to set property:" << propertyId << QString::fromStdString(res.error().message);
        return false;
    }

    // Refresh row in display_rows_
    for (size_t i = 0; i < display_rows_.size(); ++i) {
        if (!display_rows_[i].is_category && display_rows_[i].id == propertyId) {
            display_rows_[i].value = newValue;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, {ValueRole});
            emit propertyValueChanged(propertyId, newValue);
            break;
        }
    }

    return true;
}

bool ShunyaPropertyTreeModel::setPropertyFormula(const QString& propertyId, const QString& formula) {
    if (!sheet_) return false;

    std::string id_str = propertyId.toStdString();
    auto res = sheet_->set_property_formula(id_str, formula.toStdString());
    if (res.is_err()) return false;

    // Evaluate formula if param_mgr_ or formula string given
    if (param_mgr_ && !formula.trimmed().isEmpty()) {
        auto eval_res = param_mgr_->evaluate_expression(formula.toStdString());
        if (eval_res.is_ok()) {
            setPropertyValue(propertyId, eval_res.value());
        }
    }

    for (size_t i = 0; i < display_rows_.size(); ++i) {
        if (!display_rows_[i].is_category && display_rows_[i].id == propertyId) {
            display_rows_[i].formula = formula;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, {FormulaRole});
            break;
        }
    }

    return true;
}

bool ShunyaPropertyTreeModel::setPropertyUnit(const QString& propertyId, const QString& newUnit) {
    if (!sheet_) return false;

    std::string id_str = propertyId.toStdString();
    auto res = sheet_->set_property_unit(id_str, newUnit.toStdString());
    if (res.is_err()) return false;

    for (size_t i = 0; i < display_rows_.size(); ++i) {
        if (!display_rows_[i].is_category && display_rows_[i].id == propertyId) {
            display_rows_[i].unit_symbol = newUnit;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, {UnitRole});
            break;
        }
    }

    return true;
}

void ShunyaPropertyTreeModel::toggleCategory(const QString& categoryName) {
    std::string cat = categoryName.toStdString();
    bool current = true;
    if (category_expansion_state_.find(cat) != category_expansion_state_.end()) {
        current = category_expansion_state_[cat];
    }
    category_expansion_state_[cat] = !current;
    rebuild_display_rows();
}

void ShunyaPropertyTreeModel::rebuild_display_rows() {
    beginResetModel();
    display_rows_.clear();

    if (!sheet_) {
        endResetModel();
        emit propertyCountChanged();
        return;
    }

    QString filter = filter_text_.trimmed().toLower();

    for (const auto& group : sheet_->categories()) {
        std::string cat_name = group.name;
        bool is_expanded = true;
        if (category_expansion_state_.find(cat_name) != category_expansion_state_.end()) {
            is_expanded = category_expansion_state_[cat_name];
        }

        // Collect matching items
        std::vector<DisplayRow> matching_items;
        for (const auto& item : group.items) {
            QString name = QString::fromStdString(item.name);
            QString disp = QString::fromStdString(item.display_name);

            if (!filter.isEmpty()) {
                if (!name.toLower().contains(filter) && !disp.toLower().contains(filter)) {
                    continue;
                }
            }

            DisplayRow r;
            r.is_category = false;
            r.category_name = QString::fromStdString(group.name);
            r.category_display_name = QString::fromStdString(group.display_name);
            r.id = QString::fromStdString(item.id);
            r.name = name;
            r.display_name = disp;
            r.description = QString::fromStdString(item.description);
            r.visual_type = QString::fromStdString(std::string(core::domain::property_visual_type_to_string(item.visual_type)));
            r.value = parameter_value_to_qvariant(item.value, r.visual_type);
            r.unit_symbol = QString::fromStdString(item.unit_symbol);
            for (const auto& u : item.allowed_units) {
                r.allowed_units.append(QString::fromStdString(u));
            }
            r.formula = QString::fromStdString(item.formula);
            if (item.min_value.has_value()) r.min_value = item.min_value.value();
            if (item.max_value.has_value()) r.max_value = item.max_value.value();
            for (const auto& opt : item.enum_options) {
                r.enum_options.append(QString::fromStdString(opt));
            }
            r.is_read_only = item.is_read_only;

            matching_items.push_back(std::move(r));
        }

        // Only show category header if category has items (or filter is empty)
        if (!matching_items.empty() || filter.isEmpty()) {
            DisplayRow cat_row;
            cat_row.is_category = true;
            cat_row.category_name = QString::fromStdString(group.name);
            cat_row.category_display_name = QString::fromStdString(group.display_name.empty() ? group.name : group.display_name);
            cat_row.is_expanded = is_expanded;
            display_rows_.push_back(std::move(cat_row));

            if (is_expanded) {
                for (auto& item_row : matching_items) {
                    display_rows_.push_back(std::move(item_row));
                }
            }
        }
    }

    endResetModel();
    emit propertyCountChanged();
}

QVariant ShunyaPropertyTreeModel::parameter_value_to_qvariant(
    const core::domain::ParameterValue& val,
    const QString& visualType
) const {
    if (visualType == "vec3") {
        try {
            auto v = val.as_vec3();
            return QVariantList{v.x, v.y, v.z};
        } catch (...) {
            return QVariantList{0.0, 0.0, 0.0};
        }
    }

    switch (val.type()) {
        case core::domain::ParameterType::Double:
            return val.as_double();
        case core::domain::ParameterType::Int64:
            return static_cast<qint64>(val.as_int64());
        case core::domain::ParameterType::Bool:
            return val.as_bool();
        case core::domain::ParameterType::String:
            return QString::fromStdString(val.as_string());
        case core::domain::ParameterType::Quantity:
            return val.as_quantity().value;
        case core::domain::ParameterType::Vec3: {
            auto v = val.as_vec3();
            return QVariantList{v.x, v.y, v.z};
        }
        case core::domain::ParameterType::Enum:
            return QString::fromStdString(val.as_string());
    }
    return QString::fromStdString(val.to_string());
}

core::domain::ParameterValue ShunyaPropertyTreeModel::qvariant_to_parameter_value(
    const QVariant& val,
    const QString& visualType
) const {
    if (visualType == "vec3") {
        QVariantList list = val.toList();
        double x = list.size() > 0 ? list[0].toDouble() : 0.0;
        double y = list.size() > 1 ? list[1].toDouble() : 0.0;
        double z = list.size() > 2 ? list[2].toDouble() : 0.0;
        return core::domain::ParameterValue(core::base::math::Vec3(x, y, z));
    }
    if (visualType == "bool") {
        return core::domain::ParameterValue(val.toBool());
    }
    if (visualType == "number" || visualType == "quantity") {
        return core::domain::ParameterValue(val.toDouble());
    }
    if (visualType == "text" || visualType == "enum" || visualType == "color") {
        return core::domain::ParameterValue(val.toString().toStdString());
    }
    return core::domain::ParameterValue(val.toString().toStdString());
}

void ShunyaPropertyTreeModel::inspectDemoObject(const QString& demoType) {
    auto def = std::make_shared<core::domain::ElementDefinition>(
        demoType.toStdString(),
        demoType.toStdString(),
        core::domain::ElementCategory::Device
    );

    if (demoType == "Resistor") {
        core::domain::ParameterSchema res_schema;
        res_schema.name = "resistance";
        res_schema.display_name = "Resistance";
        res_schema.type = core::domain::ParameterType::Quantity;
        res_schema.unit = "kOhm";
        res_schema.default_value = core::domain::ParameterValue(1.0); // 1.0 kOhm
        res_schema.min_value = 0.0;
        def->add_parameter(res_schema);

        core::domain::ParameterSchema pwr_schema;
        pwr_schema.name = "power_rating";
        pwr_schema.display_name = "Power Rating";
        pwr_schema.type = core::domain::ParameterType::Quantity;
        pwr_schema.unit = "W";
        pwr_schema.default_value = core::domain::ParameterValue(0.25);
        def->add_parameter(pwr_schema);

        core::domain::ParameterSchema tol_schema;
        tol_schema.name = "tolerance";
        tol_schema.display_name = "Tolerance";
        tol_schema.type = core::domain::ParameterType::Double;
        tol_schema.unit = "%";
        tol_schema.default_value = core::domain::ParameterValue(5.0);
        def->add_parameter(tol_schema);
    } else {
        core::domain::ParameterSchema mass_schema;
        mass_schema.name = "mass";
        mass_schema.display_name = "Mass";
        mass_schema.type = core::domain::ParameterType::Quantity;
        mass_schema.unit = "kg";
        mass_schema.default_value = core::domain::ParameterValue(2.5);
        def->add_parameter(mass_schema);
    }

    auto instance = std::make_shared<core::domain::ElementInstance>(
        def,
        (demoType == "Resistor" ? "R1" : "Body1")
    );

    auto sheet = core::domain::PropertySheet::create_for_element_instance(instance);
    setPropertySheet(sheet, QString("%1 (%2)").arg(QString::fromStdString(instance->name())).arg(demoType));
}

} // namespace shunya::gui
