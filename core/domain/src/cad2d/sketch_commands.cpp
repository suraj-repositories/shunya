#include "shunya/cad2d/sketch_commands.hpp"

#include <algorithm>

namespace shunya::cad2d {

// 1. AddEntity2DCommand
AddEntity2DCommand::AddEntity2DCommand(
    SketchDocument2D& doc,
    std::shared_ptr<Curve2D> curve,
    bool is_construction,
    std::string layer
) : doc_(doc), curve_(std::move(curve)), is_construction_(is_construction), layer_(std::move(layer)) {}

core::base::Result<void> AddEntity2DCommand::execute() {
    if (entity_id_.empty()) {
        entity_id_ = doc_.add_entity(curve_, is_construction_, layer_);
    } else {
        SketchEntity ent;
        ent.id = entity_id_;
        ent.curve = curve_;
        ent.is_construction = is_construction_;
        ent.layer = layer_;
        ent.visible = true;
        doc_.mutable_entities().push_back(std::move(ent));
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> AddEntity2DCommand::undo() {
    if (!entity_id_.empty()) {
        doc_.remove_entity(entity_id_);
    }
    return core::base::Result<void>::ok();
}

// 2. DeleteEntities2DCommand
DeleteEntities2DCommand::DeleteEntities2DCommand(SketchDocument2D& doc, std::vector<std::string> entity_ids)
    : doc_(doc), entity_ids_(std::move(entity_ids)) {}

core::base::Result<void> DeleteEntities2DCommand::execute() {
    saved_entities_.clear();
    for (const auto& id : entity_ids_) {
        if (const auto* ent = doc_.find_entity(id)) {
            StoredEntity s;
            s.id = ent->id;
            s.curve = ent->curve ? std::shared_ptr<Curve2D>(ent->curve->clone()) : nullptr;
            s.is_construction = ent->is_construction;
            s.layer = ent->layer;
            s.visible = ent->visible;
            saved_entities_.push_back(std::move(s));
        }
    }
    for (const auto& id : entity_ids_) {
        doc_.remove_entity(id);
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> DeleteEntities2DCommand::undo() {
    for (const auto& s : saved_entities_) {
        SketchEntity ent;
        ent.id = s.id;
        ent.curve = s.curve ? std::shared_ptr<Curve2D>(s.curve->clone()) : nullptr;
        ent.is_construction = s.is_construction;
        ent.layer = s.layer;
        ent.visible = s.visible;
        doc_.mutable_entities().push_back(std::move(ent));
    }
    return core::base::Result<void>::ok();
}

// 3. TransformEntities2DCommand
TransformEntities2DCommand::TransformEntities2DCommand(
    SketchDocument2D& doc,
    std::vector<std::string> entity_ids,
    Transform2D transform
) : doc_(doc), entity_ids_(std::move(entity_ids)), xf_(transform) {}

core::base::Result<void> TransformEntities2DCommand::execute() {
    if (before_curves_.empty()) {
        for (const auto& id : entity_ids_) {
            for (auto& ent : doc_.mutable_entities()) {
                if (ent.id == id && ent.curve) {
                    before_curves_.push_back({id, std::shared_ptr<Curve2D>(ent.curve->clone())});
                    ent.curve = transform_curve(*ent.curve, xf_);
                    after_curves_.push_back({id, std::shared_ptr<Curve2D>(ent.curve->clone())});
                    break;
                }
            }
        }
    } else {
        for (const auto& [id, curve] : after_curves_) {
            for (auto& ent : doc_.mutable_entities()) {
                if (ent.id == id) {
                    ent.curve = std::shared_ptr<Curve2D>(curve->clone());
                    break;
                }
            }
        }
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> TransformEntities2DCommand::undo() {
    for (const auto& [id, curve] : before_curves_) {
        for (auto& ent : doc_.mutable_entities()) {
            if (ent.id == id) {
                ent.curve = std::shared_ptr<Curve2D>(curve->clone());
                break;
            }
        }
    }
    return core::base::Result<void>::ok();
}

// 4. ModifyEntityProps2DCommand
ModifyEntityProps2DCommand::ModifyEntityProps2DCommand(
    SketchDocument2D& doc,
    std::vector<std::string> entity_ids,
    std::optional<std::string> new_layer,
    std::optional<bool> new_construction
) : doc_(doc), entity_ids_(std::move(entity_ids)), new_layer_(std::move(new_layer)), new_construction_(new_construction) {}

core::base::Result<void> ModifyEntityProps2DCommand::execute() {
    if (prev_states_.empty()) {
        for (const auto& id : entity_ids_) {
            if (const auto* ent = doc_.find_entity(id)) {
                prev_states_.push_back({id, ent->layer, ent->is_construction});
            }
        }
    }

    for (const auto& id : entity_ids_) {
        for (auto& ent : doc_.mutable_entities()) {
            if (ent.id == id) {
                if (new_layer_.has_value()) ent.layer = *new_layer_;
                if (new_construction_.has_value()) ent.is_construction = *new_construction_;
                break;
            }
        }
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> ModifyEntityProps2DCommand::undo() {
    for (const auto& s : prev_states_) {
        for (auto& ent : doc_.mutable_entities()) {
            if (ent.id == s.id) {
                ent.layer = s.old_layer;
                ent.is_construction = s.old_construction;
                break;
            }
        }
    }
    return core::base::Result<void>::ok();
}

// 5. GroupEntities2DCommand
GroupEntities2DCommand::GroupEntities2DCommand(
    SketchDocument2D& doc,
    GroupManager& gmgr,
    std::vector<std::string> entity_ids,
    std::string group_name,
    std::string group_layer
) : doc_(doc), gmgr_(gmgr), entity_ids_(std::move(entity_ids)),
    group_name_(std::move(group_name)), group_layer_(std::move(group_layer)) {}

core::base::Result<void> GroupEntities2DCommand::execute() {
    group_id_ = gmgr_.create_group(doc_, entity_ids_, {}, group_name_, group_layer_);
    if (group_id_.empty()) {
        return core::base::Result<void>::err(core::base::Error{-1, core::base::ErrorDomain::System, core::base::Severity::Error, "Failed to create group", ""});
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> GroupEntities2DCommand::undo() {
    if (!group_id_.empty()) {
        gmgr_.ungroup(doc_, group_id_);
    }
    return core::base::Result<void>::ok();
}

// 6. SketchCommandHistory2D
std::string SketchCommandHistory2D::execute_add_entity(
    SketchDocument2D& doc,
    std::shared_ptr<Curve2D> curve,
    bool is_construction,
    std::string layer
) {
    auto cmd = std::make_unique<AddEntity2DCommand>(doc, std::move(curve), is_construction, std::move(layer));
    auto* raw_cmd = cmd.get();
    auto res = stack_.push_and_execute(std::move(cmd));
    return res.is_ok() ? raw_cmd->created_id() : std::string{};
}

bool SketchCommandHistory2D::execute_delete_entities(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids
) {
    auto cmd = std::make_unique<DeleteEntities2DCommand>(doc, entity_ids);
    return stack_.push_and_execute(std::move(cmd)).is_ok();
}

bool SketchCommandHistory2D::execute_transform(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    const Transform2D& xf
) {
    auto cmd = std::make_unique<TransformEntities2DCommand>(doc, entity_ids, xf);
    return stack_.push_and_execute(std::move(cmd)).is_ok();
}

bool SketchCommandHistory2D::execute_reassign_layer(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    std::string new_layer
) {
    auto cmd = std::make_unique<ModifyEntityProps2DCommand>(doc, entity_ids, std::move(new_layer), std::nullopt);
    return stack_.push_and_execute(std::move(cmd)).is_ok();
}

bool SketchCommandHistory2D::execute_set_construction(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    bool is_construction
) {
    auto cmd = std::make_unique<ModifyEntityProps2DCommand>(doc, entity_ids, std::nullopt, is_construction);
    return stack_.push_and_execute(std::move(cmd)).is_ok();
}

std::string SketchCommandHistory2D::execute_group(
    SketchDocument2D& doc,
    GroupManager& gmgr,
    const std::vector<std::string>& entity_ids,
    std::string group_name,
    std::string group_layer
) {
    auto cmd = std::make_unique<GroupEntities2DCommand>(doc, gmgr, entity_ids, std::move(group_name), std::move(group_layer));
    auto* raw_cmd = cmd.get();
    auto res = stack_.push_and_execute(std::move(cmd));
    return res.is_ok() ? raw_cmd->group_id() : std::string{};
}

} // namespace shunya::cad2d
