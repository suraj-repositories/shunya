#include "shunya/core/domain/scene_graph.hpp"

#include <algorithm>
#include <sstream>
#include <regex>

namespace shunya::core::domain {

namespace {

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

} // namespace

base::Result<std::shared_ptr<Entity>> SceneGraph::create_entity(
    std::string name,
    std::optional<base::Uuid> parent_id
) {
    if (name.empty()) {
        name = "Entity";
    }

    if (parent_id.has_value()) {
        auto it = entities_.find(parent_id.value());
        if (it == entities_.end()) {
            return base::Result<std::shared_ptr<Entity>>::err({
                .code = 2101,
                .domain = base::ErrorDomain::System,
                .severity = base::Severity::Error,
                .message = "Parent entity does not exist: " + parent_id.value().to_string(),
                .details = ""
            });
        }
    }

    auto entity = std::make_shared<Entity>(std::move(name));
    base::Uuid eid = entity->id();
    entity->parent_id_ = parent_id;

    if (parent_id.has_value()) {
        entities_[parent_id.value()]->children_ids_.push_back(eid);
    } else {
        root_ids_.push_back(eid);
    }

    entities_.emplace(eid, entity);
    return entity;
}

base::Result<void> SceneGraph::remove_entity(const base::Uuid& id, bool recursive) {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        return base::Result<void>::err({
            .code = 2104,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Entity to remove does not exist: " + id.to_string(),
            .details = ""
        });
    }

    auto entity = it->second;

    if (recursive) {
        // Collect all descendant ids recursively
        std::vector<base::Uuid> to_delete;
        std::function<void(const base::Uuid&)> collect_subtree = [&](const base::Uuid& curr_id) {
            auto cur_it = entities_.find(curr_id);
            if (cur_it != entities_.end()) {
                for (const auto& child_id : cur_it->second->children_ids_) {
                    collect_subtree(child_id);
                }
                to_delete.push_back(curr_id);
            }
        };

        for (const auto& child_id : entity->children_ids_) {
            collect_subtree(child_id);
        }

        // Delete collected descendants from entities map
        for (const auto& desc_id : to_delete) {
            entities_.erase(desc_id);
        }
    } else {
        // Reparent direct children to entity's parent
        auto parent_id = entity->parent_id_;
        for (const auto& child_id : entity->children_ids_) {
            auto child_it = entities_.find(child_id);
            if (child_it != entities_.end()) {
                child_it->second->parent_id_ = parent_id;
                if (parent_id.has_value()) {
                    entities_[parent_id.value()]->children_ids_.push_back(child_id);
                } else {
                    root_ids_.push_back(child_id);
                }
            }
        }
    }

    // Unlink entity from parent or roots
    if (entity->parent_id_.has_value()) {
        auto p_it = entities_.find(entity->parent_id_.value());
        if (p_it != entities_.end()) {
            auto& p_children = p_it->second->children_ids_;
            p_children.erase(
                std::remove(p_children.begin(), p_children.end(), id),
                p_children.end()
            );
        }
    } else {
        root_ids_.erase(
            std::remove(root_ids_.begin(), root_ids_.end(), id),
            root_ids_.end()
        );
    }

    entities_.erase(id);
    return base::Result<void>::ok();
}

std::shared_ptr<Entity> SceneGraph::get_entity(const base::Uuid& id) const noexcept {
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        return it->second;
    }
    return nullptr;
}

bool SceneGraph::has_entity(const base::Uuid& id) const noexcept {
    return entities_.find(id) != entities_.end();
}

void SceneGraph::clear() noexcept {
    entities_.clear();
    root_ids_.clear();
}

base::Result<void> SceneGraph::set_parent(
    const base::Uuid& child_id,
    std::optional<base::Uuid> new_parent_id
) {
    auto child_it = entities_.find(child_id);
    if (child_it == entities_.end()) {
        return base::Result<void>::err({
            .code = 2105,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Child entity not found: " + child_id.to_string(),
            .details = ""
        });
    }

    if (new_parent_id.has_value()) {
        if (new_parent_id.value() == child_id) {
            return base::Result<void>::err({
                .code = 2102,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Cannot parent an entity to itself: " + child_id.to_string(),
                .details = ""
            });
        }

        auto p_it = entities_.find(new_parent_id.value());
        if (p_it == entities_.end()) {
            return base::Result<void>::err({
                .code = 2101,
                .domain = base::ErrorDomain::System,
                .severity = base::Severity::Error,
                .message = "Target parent entity does not exist: " + new_parent_id.value().to_string(),
                .details = ""
            });
        }

        // Cycle check: verify new parent is not a descendant of child
        if (is_descendant_of(new_parent_id.value(), child_id)) {
            return base::Result<void>::err({
                .code = 2103,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Circular hierarchy detected: cannot parent to a descendant",
                .details = ""
            });
        }
    }

    auto child = child_it->second;

    // Unlink from current parent or root
    if (child->parent_id_.has_value()) {
        auto old_p = entities_.find(child->parent_id_.value());
        if (old_p != entities_.end()) {
            auto& c_list = old_p->second->children_ids_;
            c_list.erase(std::remove(c_list.begin(), c_list.end(), child_id), c_list.end());
        }
    } else {
        root_ids_.erase(std::remove(root_ids_.begin(), root_ids_.end(), child_id), root_ids_.end());
    }

    // Link to new parent or root
    child->parent_id_ = new_parent_id;
    if (new_parent_id.has_value()) {
        entities_[new_parent_id.value()]->children_ids_.push_back(child_id);
    } else {
        root_ids_.push_back(child_id);
    }

    return base::Result<void>::ok();
}

bool SceneGraph::is_descendant_of(const base::Uuid& potential_descendant, const base::Uuid& ancestor) const {
    auto curr_it = entities_.find(potential_descendant);
    while (curr_it != entities_.end() && curr_it->second->parent_id_.has_value()) {
        base::Uuid p_id = curr_it->second->parent_id_.value();
        if (p_id == ancestor) return true;
        curr_it = entities_.find(p_id);
    }
    return false;
}

std::vector<std::shared_ptr<Entity>> SceneGraph::root_entities() const {
    std::vector<std::shared_ptr<Entity>> res;
    res.reserve(root_ids_.size());
    for (const auto& rid : root_ids_) {
        auto it = entities_.find(rid);
        if (it != entities_.end()) {
            res.push_back(it->second);
        }
    }
    return res;
}

base::math::Mat4 SceneGraph::compute_world_matrix(const base::Uuid& id) const {
    std::vector<base::math::Mat4> matrices;
    auto curr_it = entities_.find(id);

    while (curr_it != entities_.end()) {
        matrices.push_back(curr_it->second->local_transform().to_matrix());
        if (curr_it->second->parent_id_.has_value()) {
            curr_it = entities_.find(curr_it->second->parent_id_.value());
        } else {
            break;
        }
    }

    // Compose root down to leaf: root * ... * parent * local
    base::math::Mat4 world = base::math::Mat4::identity();
    for (auto it = matrices.rbegin(); it != matrices.rend(); ++it) {
        world = world * (*it);
    }
    return world;
}

bool SceneGraph::is_globally_visible(const base::Uuid& id) const {
    auto curr_it = entities_.find(id);
    while (curr_it != entities_.end()) {
        if (!curr_it->second->is_visible()) return false;
        if (curr_it->second->parent_id_.has_value()) {
            curr_it = entities_.find(curr_it->second->parent_id_.value());
        } else {
            break;
        }
    }
    return true;
}

bool SceneGraph::is_globally_active(const base::Uuid& id) const {
    auto curr_it = entities_.find(id);
    while (curr_it != entities_.end()) {
        if (!curr_it->second->is_active()) return false;
        if (curr_it->second->parent_id_.has_value()) {
            curr_it = entities_.find(curr_it->second->parent_id_.value());
        } else {
            break;
        }
    }
    return true;
}

std::vector<std::shared_ptr<Entity>> SceneGraph::find_by_name(std::string_view name) const {
    std::vector<std::shared_ptr<Entity>> matches;
    for (const auto& [_, entity] : entities_) {
        if (entity->name() == name) {
            matches.push_back(entity);
        }
    }
    return matches;
}

std::vector<std::shared_ptr<Entity>> SceneGraph::find_by_tag(std::string_view tag) const {
    std::vector<std::shared_ptr<Entity>> matches;
    for (const auto& [_, entity] : entities_) {
        if (entity->has_tag(tag)) {
            matches.push_back(entity);
        }
    }
    return matches;
}

std::vector<std::shared_ptr<Entity>> SceneGraph::find_by_domain_attachment(std::string_view domain_key) const {
    std::vector<std::shared_ptr<Entity>> matches;
    for (const auto& [_, entity] : entities_) {
        if (entity->has_attachment(domain_key)) {
            matches.push_back(entity);
        }
    }
    return matches;
}

void SceneGraph::traverse_depth_first(const std::function<void(const std::shared_ptr<Entity>&)>& visitor) const {
    for (const auto& root_entity : root_entities()) {
        traverse_entity_subtree(root_entity, visitor);
    }
}

void SceneGraph::traverse_entity_subtree(
    const std::shared_ptr<Entity>& entity,
    const std::function<void(const std::shared_ptr<Entity>&)>& visitor
) const {
    visitor(entity);
    for (const auto& child_id : entity->children_ids()) {
        auto child = get_entity(child_id);
        if (child) {
            traverse_entity_subtree(child, visitor);
        }
    }
}

std::string SceneGraph::to_json() const {
    std::ostringstream ss;
    ss << "{\n  \"entities\": [\n";

    std::size_t count = 0;
    for (const auto& [_, ent] : entities_) {
        ss << "    {\n";
        ss << "      \"id\": \"" << ent->id().to_string() << "\",\n";
        ss << "      \"name\": \"" << escape_json(ent->name()) << "\",\n";
        ss << "      \"parent_id\": \"" << (ent->parent_id().has_value() ? ent->parent_id().value().to_string() : "") << "\",\n";
        ss << "      \"visible\": " << (ent->is_visible() ? "true" : "false") << ",\n";
        ss << "      \"active\": " << (ent->is_active() ? "true" : "false") << ",\n";

        // Transform
        const auto& t = ent->local_transform();
        ss << "      \"transform\": {\n";
        ss << "        \"pos\": [" << t.position.x << ", " << t.position.y << ", " << t.position.z << "],\n";
        ss << "        \"rot\": [" << t.rotation.w << ", " << t.rotation.x << ", " << t.rotation.y << ", " << t.rotation.z << "],\n";
        ss << "        \"scale\": [" << t.scale.x << ", " << t.scale.y << ", " << t.scale.z << "]\n";
        ss << "      },\n";

        // Tags
        ss << "      \"tags\": [";
        std::size_t tag_idx = 0;
        for (const auto& tag : ent->tags()) {
            ss << "\"" << escape_json(tag) << "\"";
            if (++tag_idx < ent->tags().size()) ss << ", ";
        }
        ss << "],\n";

        // Domain attachments
        ss << "      \"domain_attachments\": {\n";
        std::size_t att_idx = 0;
        for (const auto& [k, v] : ent->domain_attachments()) {
            ss << "        \"" << escape_json(k) << "\": \"" << escape_json(v) << "\"";
            if (++att_idx < ent->domain_attachments().size()) ss << ",";
            ss << "\n";
        }
        ss << "      }\n";

        ss << "    }";
        if (++count < entities_.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n}\n";
    return ss.str();
}

base::Result<std::unique_ptr<SceneGraph>> SceneGraph::from_json(std::string_view json_str) {
    if (json_str.empty()) {
        return base::Result<std::unique_ptr<SceneGraph>>::err({
            .code = 2110,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Empty scene JSON input",
            .details = ""
        });
    }

    auto sg = std::make_unique<SceneGraph>();

    // Parse entity blocks
    static const std::regex id_regex(R"re("id"\s*:\s*"([0-9a-fA-F-]+)")re");
    static const std::regex name_regex(R"re("name"\s*:\s*"([^"]*)")re");
    static const std::regex parent_regex(R"re("parent_id"\s*:\s*"([0-9a-fA-F-]*)")re");
    static const std::regex vis_regex(R"re("visible"\s*:\s*(true|false))re");
    static const std::regex act_regex(R"re("active"\s*:\s*(true|false))re");

    // Tokenize entities
    std::string json_s(json_str);
    std::size_t pos = 0;

    struct ParsedEntityRaw {
        base::Uuid id;
        std::string name;
        std::optional<base::Uuid> parent_id;
        bool visible = true;
        bool active = true;
    };

    std::vector<ParsedEntityRaw> parsed_entities;

    while ((pos = json_s.find('{', pos)) != std::string::npos) {
        auto next_obj = json_s.find('}', pos);
        if (next_obj == std::string::npos) break;

        // Skip inner objects like transform or domain_attachments if needed
        // Find matching outer entity
        std::string block = json_s.substr(pos, 512); // block header

        std::smatch id_m;
        if (std::regex_search(block, id_m, id_regex)) {
            auto uid_opt = base::Uuid::from_string(id_m[1].str());
            if (uid_opt.has_value()) {
                ParsedEntityRaw raw;
                raw.id = uid_opt.value();

                std::smatch name_m;
                if (std::regex_search(block, name_m, name_regex)) {
                    raw.name = name_m[1].str();
                }

                std::smatch p_m;
                if (std::regex_search(block, p_m, parent_regex) && !p_m[1].str().empty()) {
                    raw.parent_id = base::Uuid::from_string(p_m[1].str());
                }

                std::smatch v_m;
                if (std::regex_search(block, v_m, vis_regex)) {
                    raw.visible = (v_m[1].str() == "true");
                }

                std::smatch a_m;
                if (std::regex_search(block, a_m, act_regex)) {
                    raw.active = (a_m[1].str() == "true");
                }

                parsed_entities.push_back(raw);
            }
        }
        pos = next_obj + 1;
    }

    // Populate SceneGraph
    for (const auto& raw : parsed_entities) {
        auto ent = std::make_shared<Entity>(raw.name, raw.id);
        ent->set_visible(raw.visible);
        ent->set_active(raw.active);
        ent->parent_id_ = raw.parent_id;
        sg->entities_[raw.id] = ent;
    }

    // Wire up parent-child relationships
    for (const auto& [id, ent] : sg->entities_) {
        if (ent->parent_id_.has_value()) {
            auto p_it = sg->entities_.find(ent->parent_id_.value());
            if (p_it != sg->entities_.end()) {
                p_it->second->children_ids_.push_back(id);
            } else {
                // If parent does not exist, make it a root
                ent->parent_id_ = std::nullopt;
                sg->root_ids_.push_back(id);
            }
        } else {
            sg->root_ids_.push_back(id);
        }
    }

    return sg;
}

} // namespace shunya::core::domain
