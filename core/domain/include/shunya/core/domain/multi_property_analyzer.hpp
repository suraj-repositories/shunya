#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/domain/property_node.hpp"
#include "shunya/core/domain/command_stack.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace shunya::core::domain {

struct IntersectedProperty {
    std::string property_id;
    std::string name;
    std::string display_name;
    std::string category;
    PropertyVisualType visual_type = PropertyVisualType::Number;
    std::string unit_symbol;
    std::vector<std::string> allowed_units;
    bool is_mixed = false;
    ParameterValue unified_value;
    std::vector<std::string> target_entity_ids;
    bool is_read_only = false;
};

class PropertyIntersectionAnalyzer {
public:
    [[nodiscard]] static std::vector<IntersectedProperty> analyze(
        const std::vector<std::shared_ptr<PropertySheet>>& sheets
    );
};

class SetPropertyCommand : public ICommand {
public:
    SetPropertyCommand(
        std::shared_ptr<PropertySheet> sheet,
        std::string property_id,
        ParameterValue old_value,
        ParameterValue new_value,
        std::string description = ""
    );

    [[nodiscard]] base::Result<void> execute() override;
    [[nodiscard]] base::Result<void> undo() override;
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    std::shared_ptr<PropertySheet> sheet_;
    std::string property_id_;
    ParameterValue old_value_;
    ParameterValue new_value_;
    std::string name_;
};

class BatchSetPropertyCommand : public ICommand {
public:
    explicit BatchSetPropertyCommand(
        std::string description = "Batch Property Update"
    );

    void add_modification(
        std::shared_ptr<PropertySheet> sheet,
        std::string property_id,
        ParameterValue old_value,
        ParameterValue new_value
    );

    [[nodiscard]] std::size_t size() const noexcept { return commands_.size(); }
    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }

    [[nodiscard]] base::Result<void> execute() override;
    [[nodiscard]] base::Result<void> undo() override;
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    std::string name_;
    std::vector<SetPropertyCommand> commands_;
};

} // namespace shunya::core::domain
