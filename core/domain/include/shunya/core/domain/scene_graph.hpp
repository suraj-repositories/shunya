#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <functional>

namespace shunya::core::domain {

class Entity {
public:
    explicit Entity(std::string name, base::Uuid id = base::Uuid::generate_random())
        : id_(id), name_(std::move(name)) {}

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const std::optional<base::Uuid>& parent_id() const noexcept { return parent_id_; }
    [[nodiscard]] const std::vector<base::Uuid>& children_ids() const noexcept { return children_ids_; }

    [[nodiscard]] const base::math::Xform& local_transform() const noexcept { return transform_; }
    base::math::Xform& local_transform() noexcept { return transform_; }
    void set_local_transform(const base::math::Xform& xform) noexcept { transform_ = xform; }

    [[nodiscard]] bool is_visible() const noexcept { return visible_; }
    void set_visible(bool v) noexcept { visible_ = v; }

    [[nodiscard]] bool is_active() const noexcept { return active_; }
    void set_active(bool a) noexcept { active_ = a; }

    // Tag management
    [[nodiscard]] bool has_tag(std::string_view tag) const noexcept {
        return tags_.find(std::string(tag)) != tags_.end();
    }
    void add_tag(std::string tag) { tags_.insert(std::move(tag)); }
    void remove_tag(std::string_view tag) { tags_.erase(std::string(tag)); }
    [[nodiscard]] const std::unordered_set<std::string>& tags() const noexcept { return tags_; }

    // Domain attachments (multiphysics domain bindings e.g., "cad_mesh", "spice_subcircuit", "fem_boundary")
    void set_attachment(std::string domain_key, std::string uri_or_payload) {
        domain_attachments_[std::move(domain_key)] = std::move(uri_or_payload);
    }
    [[nodiscard]] std::optional<std::string_view> get_attachment(std::string_view domain_key) const noexcept {
        auto it = domain_attachments_.find(std::string(domain_key));
        if (it != domain_attachments_.end()) return it->second;
        return std::nullopt;
    }
    [[nodiscard]] bool has_attachment(std::string_view domain_key) const noexcept {
        return domain_attachments_.find(std::string(domain_key)) != domain_attachments_.end();
    }
    void remove_attachment(std::string_view domain_key) {
        domain_attachments_.erase(std::string(domain_key));
    }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& domain_attachments() const noexcept {
        return domain_attachments_;
    }

private:
    friend class SceneGraph;

    base::Uuid id_;
    std::string name_;
    std::optional<base::Uuid> parent_id_;
    std::vector<base::Uuid> children_ids_;
    base::math::Xform transform_{};
    bool visible_ = true;
    bool active_ = true;
    std::unordered_set<std::string> tags_;
    std::unordered_map<std::string, std::string> domain_attachments_;
};

class SceneGraph {
public:
    SceneGraph() = default;

    // Entity lifecycle
    [[nodiscard]] base::Result<std::shared_ptr<Entity>> create_entity(
        std::string name,
        std::optional<base::Uuid> parent_id = std::nullopt
    );

    [[nodiscard]] base::Result<void> remove_entity(const base::Uuid& id, bool recursive = true);

    [[nodiscard]] std::shared_ptr<Entity> get_entity(const base::Uuid& id) const noexcept;
    [[nodiscard]] bool has_entity(const base::Uuid& id) const noexcept;
    [[nodiscard]] std::size_t entity_count() const noexcept { return entities_.size(); }
    void clear() noexcept;

    // Hierarchy management
    [[nodiscard]] base::Result<void> set_parent(
        const base::Uuid& child_id,
        std::optional<base::Uuid> new_parent_id
    );

    [[nodiscard]] std::vector<std::shared_ptr<Entity>> root_entities() const;

    // Spatial and state hierarchy evaluations
    [[nodiscard]] base::math::Mat4 compute_world_matrix(const base::Uuid& id) const;
    [[nodiscard]] bool is_globally_visible(const base::Uuid& id) const;
    [[nodiscard]] bool is_globally_active(const base::Uuid& id) const;

    // Query and search
    [[nodiscard]] std::vector<std::shared_ptr<Entity>> find_by_name(std::string_view name) const;
    [[nodiscard]] std::vector<std::shared_ptr<Entity>> find_by_tag(std::string_view tag) const;
    [[nodiscard]] std::vector<std::shared_ptr<Entity>> find_by_domain_attachment(std::string_view domain_key) const;

    // Traversal
    void traverse_depth_first(const std::function<void(const std::shared_ptr<Entity>&)>& visitor) const;

    // Serialization
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<std::unique_ptr<SceneGraph>> from_json(std::string_view json_str);

private:
    [[nodiscard]] bool is_descendant_of(const base::Uuid& potential_descendant, const base::Uuid& ancestor) const;
    void traverse_entity_subtree(
        const std::shared_ptr<Entity>& entity,
        const std::function<void(const std::shared_ptr<Entity>&)>& visitor
    ) const;

    std::unordered_map<base::Uuid, std::shared_ptr<Entity>> entities_;
    std::vector<base::Uuid> root_ids_;
};

} // namespace shunya::core::domain
