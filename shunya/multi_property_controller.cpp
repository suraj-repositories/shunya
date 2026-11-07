#include "multi_property_controller.hpp"
#include <QDebug>

namespace shunya::gui {

ShunyaMultiPropertyController::ShunyaMultiPropertyController(QObject* parent)
    : QAbstractListModel(parent)
{
    selectDemoMultiSet("50_resistors_identical");
}

ShunyaMultiPropertyController::~ShunyaMultiPropertyController() = default;

QHash<int, QByteArray> ShunyaMultiPropertyController::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[PropertyIdRole] = "propertyId";
    roles[NameRole] = "propertyName";
    roles[DisplayNameRole] = "displayName";
    roles[CategoryRole] = "category";
    roles[TypeRole] = "visualType";
    roles[ValueRole] = "value";
    roles[UnitRole] = "unitSymbol";
    roles[AllowedUnitsRole] = "allowedUnits";
    roles[IsMixedRole] = "isMixed";
    roles[IsReadOnlyRole] = "isReadOnly";
    return roles;
}

int ShunyaMultiPropertyController::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(intersected_props_.size());
}

QVariant ShunyaMultiPropertyController::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(intersected_props_.size())) {
        return QVariant();
    }

    const auto& prop = intersected_props_[index.row()];
    QString vtype = QString::fromStdString(std::string(core::domain::property_visual_type_to_string(prop.visual_type)));

    switch (role) {
        case PropertyIdRole:    return QString::fromStdString(prop.property_id);
        case NameRole:          return QString::fromStdString(prop.name);
        case DisplayNameRole:   return QString::fromStdString(prop.display_name);
        case CategoryRole:      return QString::fromStdString(prop.category);
        case TypeRole:          return vtype;
        case ValueRole:         return parameter_value_to_qvariant(prop.unified_value, vtype, prop.is_mixed);
        case UnitRole:          return QString::fromStdString(prop.unit_symbol);
        case AllowedUnitsRole: {
            QStringList list;
            for (const auto& u : prop.allowed_units) list.append(QString::fromStdString(u));
            return list;
        }
        case IsMixedRole:       return prop.is_mixed;
        case IsReadOnlyRole:    return prop.is_read_only;
        default:                return QVariant();
    }
}

bool ShunyaMultiPropertyController::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(intersected_props_.size())) {
        return false;
    }

    if (role == ValueRole) {
        return setBatchPropertyValue(QString::fromStdString(intersected_props_[index.row()].property_id), value);
    }
    return false;
}

void ShunyaMultiPropertyController::setSelectedSheets(
    std::vector<std::shared_ptr<core::domain::PropertySheet>> sheets,
    const QString& description
) {
    sheets_ = std::move(sheets);
    selection_description_ = description.isEmpty() 
        ? QString("%1 Entities Selected").arg(sheets_.size()) 
        : description;

    update_intersection();
    emit selectionChanged();
}

void ShunyaMultiPropertyController::update_intersection() {
    beginResetModel();
    intersected_props_ = core::domain::PropertyIntersectionAnalyzer::analyze(sheets_);
    endResetModel();
    emit propertyCountChanged();
}

bool ShunyaMultiPropertyController::setBatchPropertyValue(const QString& propertyId, const QVariant& newValue) {
    if (sheets_.empty()) return false;

    std::string pid = propertyId.toStdString();

    // Verify property is in intersection and not read only
    const core::domain::IntersectedProperty* found = nullptr;
    for (const auto& p : intersected_props_) {
        if (p.property_id == pid) {
            found = &p;
            break;
        }
    }
    if (!found || found->is_read_only) return false;

    QString vtype = QString::fromStdString(std::string(core::domain::property_visual_type_to_string(found->visual_type)));
    core::domain::ParameterValue pv = qvariant_to_parameter_value(newValue, vtype);

    auto batch_cmd = std::make_unique<core::domain::BatchSetPropertyCommand>(
        "Modify " + pid + " across " + std::to_string(sheets_.size()) + " items"
    );

    for (const auto& sheet : sheets_) {
        if (!sheet) continue;
        const auto* prop = sheet->find_property(pid);
        if (prop) {
            batch_cmd->add_modification(sheet, pid, prop->value, pv);
        }
    }

    if (command_stack_) {
        auto push_res = command_stack_->push_and_execute(std::move(batch_cmd));
        if (push_res.is_err()) {
            qWarning() << "Failed to execute batch command:" << QString::fromStdString(push_res.error().message);
            return false;
        }
    } else {
        auto exec_res = batch_cmd->execute();
        if (exec_res.is_err()) return false;
    }

    update_intersection();
    emit batchPropertyUpdated(propertyId, newValue);
    return true;
}

QVariant ShunyaMultiPropertyController::parameter_value_to_qvariant(
    const core::domain::ParameterValue& val,
    const QString& visualType,
    bool isMixed
) const {
    if (isMixed) {
        return "<mixed>";
    }
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

core::domain::ParameterValue ShunyaMultiPropertyController::qvariant_to_parameter_value(
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
    return core::domain::ParameterValue(val.toString().toStdString());
}

void ShunyaMultiPropertyController::selectDemoMultiSet(const QString& setType) {
    if (setType == "empty") {
        setSelectedSheets({}, "No Entities Selected");
        return;
    }

    if (setType == "50_resistors_identical") {
        auto def = std::make_shared<core::domain::ElementDefinition>("Resistor", "Resistor", core::domain::ElementCategory::Device);
        core::domain::ParameterSchema res_schema;
        res_schema.name = "resistance";
        res_schema.type = core::domain::ParameterType::Quantity;
        res_schema.unit = "kOhm";
        res_schema.default_value = core::domain::ParameterValue(1.0);
        def->add_parameter(res_schema);

        std::vector<std::shared_ptr<core::domain::PropertySheet>> sheets;
        for (int i = 0; i < 50; ++i) {
            auto inst = std::make_shared<core::domain::ElementInstance>(def, "R" + std::to_string(i + 1));
            sheets.push_back(core::domain::PropertySheet::create_for_element_instance(inst));
        }
        setSelectedSheets(sheets, "50 Resistors (Homogenous / Identical)");
    } else if (setType == "50_resistors_divergent") {
        auto def = std::make_shared<core::domain::ElementDefinition>("Resistor", "Resistor", core::domain::ElementCategory::Device);
        core::domain::ParameterSchema res_schema;
        res_schema.name = "resistance";
        res_schema.type = core::domain::ParameterType::Quantity;
        res_schema.unit = "kOhm";
        res_schema.default_value = core::domain::ParameterValue(1.0);
        def->add_parameter(res_schema);

        std::vector<std::shared_ptr<core::domain::PropertySheet>> sheets;
        for (int i = 0; i < 50; ++i) {
            auto inst = std::make_shared<core::domain::ElementInstance>(def, "R" + std::to_string(i + 1));
            if (i % 2 == 1) {
                inst->set_parameter("resistance", core::domain::ParameterValue(2.2));
            }
            sheets.push_back(core::domain::PropertySheet::create_for_element_instance(inst));
        }
        setSelectedSheets(sheets, "50 Resistors (Divergent Values)");
    } else if (setType == "resistor_and_capacitor") {
        auto r_def = std::make_shared<core::domain::ElementDefinition>("Resistor", "Resistor", core::domain::ElementCategory::Device);
        core::domain::ParameterSchema res_schema;
        res_schema.name = "resistance";
        res_schema.type = core::domain::ParameterType::Quantity;
        res_schema.unit = "kOhm";
        res_schema.default_value = core::domain::ParameterValue(1.0);
        r_def->add_parameter(res_schema);

        auto c_def = std::make_shared<core::domain::ElementDefinition>("Capacitor", "Capacitor", core::domain::ElementCategory::Device);
        core::domain::ParameterSchema cap_schema;
        cap_schema.name = "capacitance";
        cap_schema.type = core::domain::ParameterType::Quantity;
        cap_schema.unit = "uF";
        cap_schema.default_value = core::domain::ParameterValue(10.0);
        c_def->add_parameter(cap_schema);

        auto r_inst = std::make_shared<core::domain::ElementInstance>(r_def, "R1");
        auto c_inst = std::make_shared<core::domain::ElementInstance>(c_def, "C1");

        std::vector<std::shared_ptr<core::domain::PropertySheet>> sheets = {
            core::domain::PropertySheet::create_for_element_instance(r_inst),
            core::domain::PropertySheet::create_for_element_instance(c_inst)
        };
        setSelectedSheets(sheets, "2 Entities (Resistor & Capacitor Heterogenous)");
    }
}

} // namespace shunya::gui
