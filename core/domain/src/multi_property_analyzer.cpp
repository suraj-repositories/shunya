#include "shunya/core/domain/multi_property_analyzer.hpp"
#include <sstream>

namespace shunya::core::domain {

std::vector<IntersectedProperty> PropertyIntersectionAnalyzer::analyze(
    const std::vector<std::shared_ptr<PropertySheet>>& sheets
) {
    if (sheets.empty()) return {};

    std::vector<IntersectedProperty> result;

    const auto& first_sheet = sheets.front();
    if (!first_sheet) return {};

    for (const auto& group : first_sheet->categories()) {
        for (const auto& candidate : group.items) {
            bool present_in_all = true;
            bool values_identical = true;
            bool any_read_only = candidate.is_read_only;
            std::vector<std::string> entity_ids;

            // Check candidate against all sheets
            for (const auto& sheet : sheets) {
                if (!sheet) {
                    present_in_all = false;
                    break;
                }

                const auto* prop = sheet->find_property(candidate.id);
                if (!prop || prop->visual_type != candidate.visual_type) {
                    present_in_all = false;
                    break;
                }

                if (prop->is_read_only) any_read_only = true;

                const auto* id_prop = sheet->find_property("general.id");
                if (id_prop) {
                    entity_ids.push_back(id_prop->value.as_string());
                }

                if (prop->value != candidate.value) {
                    values_identical = false;
                }
            }

            if (present_in_all) {
                IntersectedProperty intersected;
                intersected.property_id = candidate.id;
                intersected.name = candidate.name;
                intersected.display_name = candidate.display_name;
                intersected.category = candidate.category;
                intersected.visual_type = candidate.visual_type;
                intersected.unit_symbol = candidate.unit_symbol;
                intersected.allowed_units = candidate.allowed_units;
                intersected.is_mixed = !values_identical;
                intersected.unified_value = values_identical ? candidate.value : ParameterValue("<mixed>");
                intersected.target_entity_ids = std::move(entity_ids);
                intersected.is_read_only = any_read_only;

                result.push_back(std::move(intersected));
            }
        }
    }

    return result;
}

SetPropertyCommand::SetPropertyCommand(
    std::shared_ptr<PropertySheet> sheet,
    std::string property_id,
    ParameterValue old_value,
    ParameterValue new_value,
    std::string description
)
    : sheet_(std::move(sheet)),
      property_id_(std::move(property_id)),
      old_value_(std::move(old_value)),
      new_value_(std::move(new_value)),
      name_(description.empty() ? ("Set " + property_id_) : std::move(description))
{
}

base::Result<void> SetPropertyCommand::execute() {
    if (!sheet_) {
        return base::Result<void>::err({
            .code = 3070,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Null property sheet in SetPropertyCommand",
            .details = ""
        });
    }
    return sheet_->set_property_value(property_id_, new_value_);
}

base::Result<void> SetPropertyCommand::undo() {
    if (!sheet_) {
        return base::Result<void>::err({
            .code = 3070,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Null property sheet in SetPropertyCommand",
            .details = ""
        });
    }
    return sheet_->set_property_value(property_id_, old_value_);
}

BatchSetPropertyCommand::BatchSetPropertyCommand(std::string description)
    : name_(std::move(description))
{
}

void BatchSetPropertyCommand::add_modification(
    std::shared_ptr<PropertySheet> sheet,
    std::string property_id,
    ParameterValue old_value,
    ParameterValue new_value
) {
    commands_.emplace_back(std::move(sheet), std::move(property_id), std::move(old_value), std::move(new_value));
}

base::Result<void> BatchSetPropertyCommand::execute() {
    for (size_t i = 0; i < commands_.size(); ++i) {
        auto res = commands_[i].execute();
        if (res.is_err()) {
            // Atomic rollback of already executed modifications
            for (size_t j = i; j > 0; --j) {
                (void)commands_[j - 1].undo();
            }
            return res;
        }
    }
    return base::Result<void>::ok();
}

base::Result<void> BatchSetPropertyCommand::undo() {
    for (size_t i = commands_.size(); i > 0; --i) {
        auto res = commands_[i - 1].undo();
        if (res.is_err()) {
            return res;
        }
    }
    return base::Result<void>::ok();
}

} // namespace shunya::core::domain
