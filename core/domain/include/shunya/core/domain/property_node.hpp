#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/units.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/domain/element_definition.hpp"
#include "shunya/core/domain/element_instance.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <memory>

namespace shunya::core::domain {

enum class PropertyVisualType : uint8_t {
    Number,
    Quantity,
    Vector3,
    Boolean,
    Text,
    Enumeration,
    Color
};

[[nodiscard]] constexpr std::string_view property_visual_type_to_string(PropertyVisualType type) noexcept {
    switch (type) {
        case PropertyVisualType::Number:      return "number";
        case PropertyVisualType::Quantity:    return "quantity";
        case PropertyVisualType::Vector3:     return "vec3";
        case PropertyVisualType::Boolean:     return "bool";
        case PropertyVisualType::Text:        return "text";
        case PropertyVisualType::Enumeration: return "enum";
        case PropertyVisualType::Color:       return "color";
    }
    return "number";
}

struct ReflectedPropertyItem {
    std::string id;
    std::string name;
    std::string display_name;
    std::string category;
    std::string description;
    PropertyVisualType visual_type = PropertyVisualType::Number;
    ParameterValue value;
    std::string unit_symbol;
    base::units::Dimension dimension = base::units::Dimensionless;
    std::vector<std::string> allowed_units;
    std::string formula;
    std::optional<double> min_value;
    std::optional<double> max_value;
    std::vector<std::string> enum_options;
    bool is_read_only = false;

    std::function<ParameterValue()> getter;
    std::function<base::Result<void>(const ParameterValue&)> setter;
};

struct PropertyCategoryGroup {
    std::string name;
    std::string display_name;
    bool is_expanded = true;
    std::vector<ReflectedPropertyItem> items;
};

class PropertySheet {
public:
    using ChangeListener = std::function<void(const std::string& property_id, const ParameterValue& new_value)>;

    PropertySheet() = default;

    void add_category(std::string name, std::string display_name = "");
    void add_property(ReflectedPropertyItem item);

    [[nodiscard]] const std::vector<PropertyCategoryGroup>& categories() const noexcept { return categories_; }
    [[nodiscard]] std::vector<PropertyCategoryGroup>& categories() noexcept { return categories_; }

    [[nodiscard]] const ReflectedPropertyItem* find_property(std::string_view property_id) const noexcept;
    [[nodiscard]] ReflectedPropertyItem* find_property_mut(std::string_view property_id) noexcept;

    [[nodiscard]] base::Result<void> set_property_value(std::string_view property_id, const ParameterValue& value);
    [[nodiscard]] base::Result<void> set_property_formula(std::string_view property_id, std::string_view formula);
    [[nodiscard]] base::Result<void> set_property_unit(std::string_view property_id, std::string_view new_unit);

    void refresh_values();

    void add_listener(ChangeListener listener) {
        listeners_.push_back(std::move(listener));
    }

    void clear() noexcept {
        categories_.clear();
        id_to_indices_.clear();
    }

    [[nodiscard]] static std::shared_ptr<PropertySheet> create_for_element_instance(
        const std::shared_ptr<ElementInstance>& instance
    );

private:
    std::vector<PropertyCategoryGroup> categories_;
    std::unordered_map<std::string, std::pair<size_t, size_t>> id_to_indices_;
    std::vector<ChangeListener> listeners_;
};

} // namespace shunya::core::domain
