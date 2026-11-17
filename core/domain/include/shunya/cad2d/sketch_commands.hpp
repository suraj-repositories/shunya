#pragma once

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/group_manager.hpp"
#include "shunya/core/domain/command_stack.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::cad2d {

/**
 * @brief Command to add a new 2D sketch entity to the document.
 */
class AddEntity2DCommand final : public core::domain::ICommand {
public:
    AddEntity2DCommand(
        SketchDocument2D& doc,
        std::shared_ptr<Curve2D> curve,
        bool is_construction = false,
        std::string layer = "0"
    );

    [[nodiscard]] core::base::Result<void> execute() override;
    [[nodiscard]] core::base::Result<void> undo() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Add 2D Entity"; }
    [[nodiscard]] const std::string& created_id() const noexcept { return entity_id_; }

private:
    SketchDocument2D& doc_;
    std::shared_ptr<Curve2D> curve_;
    bool is_construction_{false};
    std::string layer_{"0"};
    std::string entity_id_;
};

/**
 * @brief Command to delete one or more entities from the document.
 */
class DeleteEntities2DCommand final : public core::domain::ICommand {
public:
    DeleteEntities2DCommand(SketchDocument2D& doc, std::vector<std::string> entity_ids);

    [[nodiscard]] core::base::Result<void> execute() override;
    [[nodiscard]] core::base::Result<void> undo() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Delete 2D Entities"; }

private:
    struct StoredEntity {
        std::string id;
        std::shared_ptr<Curve2D> curve;
        bool is_construction{false};
        std::string layer{"0"};
        bool visible{true};
    };

    SketchDocument2D& doc_;
    std::vector<std::string> entity_ids_;
    std::vector<StoredEntity> saved_entities_;
};

/**
 * @brief Command to apply an affine Transform2D to a set of entities.
 */
class TransformEntities2DCommand final : public core::domain::ICommand {
public:
    TransformEntities2DCommand(
        SketchDocument2D& doc,
        std::vector<std::string> entity_ids,
        Transform2D transform
    );

    [[nodiscard]] core::base::Result<void> execute() override;
    [[nodiscard]] core::base::Result<void> undo() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Transform 2D Entities"; }

private:
    SketchDocument2D& doc_;
    std::vector<std::string> entity_ids_;
    Transform2D xf_;
    std::vector<std::pair<std::string, std::shared_ptr<Curve2D>>> before_curves_;
    std::vector<std::pair<std::string, std::shared_ptr<Curve2D>>> after_curves_;
};

/**
 * @brief Command to modify entity properties (layer, construction, visibility).
 */
class ModifyEntityProps2DCommand final : public core::domain::ICommand {
public:
    ModifyEntityProps2DCommand(
        SketchDocument2D& doc,
        std::vector<std::string> entity_ids,
        std::optional<std::string> new_layer,
        std::optional<bool> new_construction
    );

    [[nodiscard]] core::base::Result<void> execute() override;
    [[nodiscard]] core::base::Result<void> undo() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Modify Entity Properties"; }

private:
    struct PropState {
        std::string id;
        std::string old_layer;
        bool old_construction{false};
    };

    SketchDocument2D& doc_;
    std::vector<std::string> entity_ids_;
    std::optional<std::string> new_layer_;
    std::optional<bool> new_construction_;
    std::vector<PropState> prev_states_;
};

/**
 * @brief Command to group entities using GroupManager.
 */
class GroupEntities2DCommand final : public core::domain::ICommand {
public:
    GroupEntities2DCommand(
        SketchDocument2D& doc,
        GroupManager& gmgr,
        std::vector<std::string> entity_ids,
        std::string group_name = "",
        std::string group_layer = "0"
    );

    [[nodiscard]] core::base::Result<void> execute() override;
    [[nodiscard]] core::base::Result<void> undo() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "Group 2D Entities"; }
    [[nodiscard]] const std::string& group_id() const noexcept { return group_id_; }

private:
    SketchDocument2D& doc_;
    GroupManager& gmgr_;
    std::vector<std::string> entity_ids_;
    std::string group_name_;
    std::string group_layer_;
    std::string group_id_;
};

/**
 * @brief Transactional 2D Sketch Command History coordinator.
 */
class SketchCommandHistory2D {
public:
    explicit SketchCommandHistory2D(size_t max_history = 100) : stack_(max_history) {}

    std::string execute_add_entity(
        SketchDocument2D& doc,
        std::shared_ptr<Curve2D> curve,
        bool is_construction = false,
        std::string layer = "0"
    );

    bool execute_delete_entities(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids
    );

    bool execute_transform(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        const Transform2D& xf
    );

    bool execute_reassign_layer(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        std::string new_layer
    );

    bool execute_set_construction(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        bool is_construction
    );

    std::string execute_group(
        SketchDocument2D& doc,
        GroupManager& gmgr,
        const std::vector<std::string>& entity_ids,
        std::string group_name = "",
        std::string group_layer = "0"
    );

    // Transaction management
    [[nodiscard]] core::base::Result<void> begin_transaction(std::string name) {
        return stack_.begin_transaction(std::move(name));
    }
    [[nodiscard]] core::base::Result<void> commit_transaction() {
        return stack_.commit_transaction();
    }
    [[nodiscard]] core::base::Result<void> rollback_transaction() {
        return stack_.rollback_transaction();
    }
    [[nodiscard]] bool is_in_transaction() const noexcept {
        return stack_.is_in_transaction();
    }

    // Undo / Redo
    [[nodiscard]] core::base::Result<void> undo() { return stack_.undo(); }
    [[nodiscard]] core::base::Result<void> redo() { return stack_.redo(); }
    [[nodiscard]] bool can_undo() const noexcept { return stack_.can_undo(); }
    [[nodiscard]] bool can_redo() const noexcept { return stack_.can_redo(); }
    [[nodiscard]] std::string_view undo_name() const noexcept { return stack_.undo_name(); }
    [[nodiscard]] std::string_view redo_name() const noexcept { return stack_.redo_name(); }

    [[nodiscard]] size_t history_count() const noexcept { return stack_.count(); }
    void clear() noexcept { stack_.clear(); }

    [[nodiscard]] const core::domain::CommandStack& underlying_stack() const noexcept { return stack_; }

private:
    core::domain::CommandStack stack_;
};

} // namespace shunya::cad2d
