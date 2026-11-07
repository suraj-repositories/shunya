#include "shunya/core/domain/property_node.hpp"
#include <algorithm>

namespace shunya::core::domain {

void PropertySheet::add_category(std::string name, std::string display_name) {
    for (const auto& cat : categories_) {
        if (cat.name == name) return;
    }
    std::string disp = display_name.empty() ? name : std::move(display_name);
    categories_.push_back(PropertyCategoryGroup{std::move(name), std::move(disp), true, {}});
}

void PropertySheet::add_property(ReflectedPropertyItem item) {
    std::string cat_name = item.category.empty() ? "General" : item.category;
    item.category = cat_name;

    size_t cat_idx = categories_.size();
    for (size_t i = 0; i < categories_.size(); ++i) {
        if (categories_[i].name == cat_name) {
            cat_idx = i;
            break;
        }
    }

    if (cat_idx == categories_.size()) {
        categories_.push_back(PropertyCategoryGroup{cat_name, cat_name, true, {}});
    }

    size_t item_idx = categories_[cat_idx].items.size();
    id_to_indices_[item.id] = {cat_idx, item_idx};
    categories_[cat_idx].items.push_back(std::move(item));
}

const ReflectedPropertyItem* PropertySheet::find_property(std::string_view property_id) const noexcept {
    auto it = id_to_indices_.find(std::string(property_id));
    if (it != id_to_indices_.end()) {
        const auto& [cat_idx, item_idx] = it->second;
        if (cat_idx < categories_.size() && item_idx < categories_[cat_idx].items.size()) {
            return &categories_[cat_idx].items[item_idx];
        }
    }
    return nullptr;
}

ReflectedPropertyItem* PropertySheet::find_property_mut(std::string_view property_id) noexcept {
    auto it = id_to_indices_.find(std::string(property_id));
    if (it != id_to_indices_.end()) {
        const auto& [cat_idx, item_idx] = it->second;
        if (cat_idx < categories_.size() && item_idx < categories_[cat_idx].items.size()) {
            return &categories_[cat_idx].items[item_idx];
        }
    }
    return nullptr;
}

base::Result<void> PropertySheet::set_property_value(std::string_view property_id, const ParameterValue& value) {
    auto* prop = find_property_mut(property_id);
    if (!prop) {
        return base::Result<void>::err({
            .code = 3050,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Property not found: " + std::string(property_id),
            .details = ""
        });
    }

    if (prop->is_read_only) {
        return base::Result<void>::err({
            .code = 3051,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Cannot modify read-only property: " + std::string(property_id),
            .details = ""
        });
    }

    if (prop->setter) {
        auto set_res = prop->setter(value);
        if (set_res.is_err()) return set_res;
    }

    prop->value = value;

    for (const auto& listener : listeners_) {
        if (listener) listener(prop->id, prop->value);
    }

    return base::Result<void>::ok();
}

base::Result<void> PropertySheet::set_property_formula(std::string_view property_id, std::string_view formula) {
    auto* prop = find_property_mut(property_id);
    if (!prop) {
        return base::Result<void>::err({
            .code = 3050,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Property not found: " + std::string(property_id),
            .details = ""
        });
    }

    prop->formula = std::string(formula);
    return base::Result<void>::ok();
}

base::Result<void> PropertySheet::set_property_unit(std::string_view property_id, std::string_view new_unit) {
    auto* prop = find_property_mut(property_id);
    if (!prop) {
        return base::Result<void>::err({
            .code = 3050,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Property not found: " + std::string(property_id),
            .details = ""
        });
    }

    prop->unit_symbol = std::string(new_unit);
    return base::Result<void>::ok();
}

void PropertySheet::refresh_values() {
    for (auto& group : categories_) {
        for (auto& item : group.items) {
            if (item.getter) {
                item.value = item.getter();
                for (const auto& listener : listeners_) {
                    if (listener) listener(item.id, item.value);
                }
            }
        }
    }
}

std::shared_ptr<PropertySheet> PropertySheet::create_for_element_instance(
    const std::shared_ptr<ElementInstance>& instance
) {
    auto sheet = std::make_shared<PropertySheet>();
    if (!instance) return sheet;

    // 1. General Category
    sheet->add_category("General", "General Identity");

    ReflectedPropertyItem name_prop;
    name_prop.id = "general.name";
    name_prop.name = "name";
    name_prop.display_name = "Name";
    name_prop.category = "General";
    name_prop.visual_type = PropertyVisualType::Text;
    name_prop.value = ParameterValue(instance->name());
    name_prop.getter = [inst = instance]() -> ParameterValue { return ParameterValue(inst->name()); };
    name_prop.setter = [inst = instance](const ParameterValue& val) -> base::Result<void> {
        inst->set_name(val.as_string());
        return base::Result<void>::ok();
    };
    sheet->add_property(std::move(name_prop));

    ReflectedPropertyItem id_prop;
    id_prop.id = "general.id";
    id_prop.name = "id";
    id_prop.display_name = "UUID";
    id_prop.category = "General";
    id_prop.visual_type = PropertyVisualType::Text;
    id_prop.is_read_only = true;
    id_prop.value = ParameterValue(instance->id().to_string());
    id_prop.getter = [inst = instance]() -> ParameterValue { return ParameterValue(inst->id().to_string()); };
    sheet->add_property(std::move(id_prop));

    // 2. Transform Category
    sheet->add_category("Transform", "Spatial Transform");

    ReflectedPropertyItem pos_prop;
    pos_prop.id = "transform.position";
    pos_prop.name = "position";
    pos_prop.display_name = "Position";
    pos_prop.category = "Transform";
    pos_prop.visual_type = PropertyVisualType::Vector3;
    pos_prop.unit_symbol = "m";
    pos_prop.dimension = base::units::Length;
    pos_prop.allowed_units = {"m", "mm", "cm", "km", "in", "ft"};
    pos_prop.value = ParameterValue(instance->transform().position);
    pos_prop.getter = [inst = instance]() -> ParameterValue { return ParameterValue(inst->transform().position); };
    pos_prop.setter = [inst = instance](const ParameterValue& val) -> base::Result<void> {
        auto xform = inst->transform();
        xform.position = val.as_vec3();
        inst->set_transform(xform);
        return base::Result<void>::ok();
    };
    sheet->add_property(std::move(pos_prop));

    ReflectedPropertyItem scale_prop;
    scale_prop.id = "transform.scale";
    scale_prop.name = "scale";
    scale_prop.display_name = "Scale";
    scale_prop.category = "Transform";
    scale_prop.visual_type = PropertyVisualType::Vector3;
    scale_prop.value = ParameterValue(instance->transform().scale);
    scale_prop.getter = [inst = instance]() -> ParameterValue { return ParameterValue(inst->transform().scale); };
    scale_prop.setter = [inst = instance](const ParameterValue& val) -> base::Result<void> {
        auto xform = inst->transform();
        xform.scale = val.as_vec3();
        inst->set_transform(xform);
        return base::Result<void>::ok();
    };
    sheet->add_property(std::move(scale_prop));

    // 3. Parameters Category
    const auto& def = instance->definition();
    if (def && !def->parameters().empty()) {
        sheet->add_category("Parameters", "Reflected Parameters");
        for (const auto& schema : def->parameters()) {
            ReflectedPropertyItem p;
            p.id = "parameter." + schema.name;
            p.name = schema.name;
            p.display_name = schema.display_name.empty() ? schema.name : schema.display_name;
            p.category = "Parameters";
            p.description = schema.description;
            p.min_value = schema.min_value;
            p.max_value = schema.max_value;
            p.enum_options = schema.enum_options;
            p.is_read_only = schema.is_read_only;

            switch (schema.type) {
                case ParameterType::Double:
                    p.visual_type = PropertyVisualType::Number;
                    break;
                case ParameterType::Int64:
                    p.visual_type = PropertyVisualType::Number;
                    break;
                case ParameterType::Bool:
                    p.visual_type = PropertyVisualType::Boolean;
                    break;
                case ParameterType::String:
                    p.visual_type = PropertyVisualType::Text;
                    break;
                case ParameterType::Quantity:
                    p.visual_type = PropertyVisualType::Quantity;
                    p.unit_symbol = schema.unit;
                    if (!schema.unit.empty()) {
                        p.allowed_units = base::units::UnitRegistry::instance().get_compatible_units(schema.unit);
                    }
                    break;
                case ParameterType::Vec3:
                    p.visual_type = PropertyVisualType::Vector3;
                    break;
                case ParameterType::Enum:
                    p.visual_type = PropertyVisualType::Enumeration;
                    break;
            }

            auto initial_val = instance->get_parameter(schema.name);
            auto effective_val = initial_val.is_ok() ? initial_val.value() : schema.default_value;
            if (schema.type == ParameterType::Quantity && effective_val.type() == ParameterType::Quantity) {
                p.value = ParameterValue(effective_val.as_quantity().value);
            } else {
                p.value = effective_val;
            }

            std::string p_name = schema.name;
            ParameterValue def_val = schema.default_value;
            ParameterType p_type = schema.type;
            std::string p_unit = schema.unit;

            p.getter = [inst = instance, p_name, def_val]() -> ParameterValue {
                auto val = inst->get_parameter(p_name);
                if (val.is_ok()) {
                    if (val.value().type() == ParameterType::Quantity) {
                        return ParameterValue(val.value().as_quantity().value);
                    }
                    return val.value();
                }
                if (def_val.type() == ParameterType::Quantity) {
                    return ParameterValue(def_val.as_quantity().value);
                }
                return def_val;
            };

            p.setter = [inst = instance, p_name, p_type, p_unit](const ParameterValue& val) -> base::Result<void> {
                if (p_type == ParameterType::Quantity && (val.type() == ParameterType::Double || val.type() == ParameterType::Int64)) {
                    double d = (val.type() == ParameterType::Double) ? val.as_double() : static_cast<double>(val.as_int64());
                    base::units::Dimension dim = base::units::Dimensionless;
                    if (!p_unit.empty()) {
                        const auto* udef = base::units::UnitRegistry::instance().find_unit(p_unit);
                        if (udef) dim = udef->dimension;
                    }
                    return inst->set_parameter(p_name, ParameterValue(base::units::Quantity(d, dim)));
                }
                return inst->set_parameter(p_name, val);
            };

            sheet->add_property(std::move(p));
        }
    }

    return sheet;
}

} // namespace shunya::core::domain
