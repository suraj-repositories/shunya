#include "shunya/cad2d/layer_manager.hpp"

#include <algorithm>
#include <unordered_set>

namespace shunya::cad2d {

LayerManager::LayerManager() {
    layers_.push_back(Layer2D{"0", "", true, false, LayerStyle{"#61afef", 0.25, LinePattern::Solid}});
}

bool LayerManager::add_layer(std::string name, std::string parent_layer, LayerStyle style) {
    if (name.empty() || find_layer(name) != nullptr) return false;
    if (!parent_layer.empty() && find_layer(parent_layer) == nullptr) {
        return false;
    }
    layers_.push_back(Layer2D{std::move(name), std::move(parent_layer), true, false, std::move(style)});
    return true;
}

bool LayerManager::remove_layer(std::string_view name) {
    if (name == "0") return false; // Default layer "0" cannot be removed
    auto it = std::find_if(layers_.begin(), layers_.end(), [&](const Layer2D& l) {
        return l.name == name;
    });
    if (it == layers_.end()) return false;

    std::string fallback_parent = it->parent_layer;
    layers_.erase(it);
    for (auto& l : layers_) {
        if (l.parent_layer == name) {
            l.parent_layer = fallback_parent;
        }
    }
    return true;
}

bool LayerManager::set_visible(std::string_view name, bool visible) {
    for (auto& l : layers_) {
        if (l.name == name) {
            l.visible = visible;
            return true;
        }
    }
    return false;
}

bool LayerManager::set_locked(std::string_view name, bool locked) {
    for (auto& l : layers_) {
        if (l.name == name) {
            l.locked = locked;
            return true;
        }
    }
    return false;
}

bool LayerManager::set_style(std::string_view name, const LayerStyle& style) {
    for (auto& l : layers_) {
        if (l.name == name) {
            l.style = style;
            return true;
        }
    }
    return false;
}

bool LayerManager::would_create_cycle(std::string_view layer, std::string_view proposed_parent) const noexcept {
    std::string_view curr = proposed_parent;
    std::unordered_set<std::string_view> visited;
    while (!curr.empty()) {
        if (curr == layer) return true;
        if (!visited.insert(curr).second) return true;
        const auto* p = find_layer(curr);
        if (!p) break;
        curr = p->parent_layer;
    }
    return false;
}

bool LayerManager::set_parent(std::string_view name, std::string parent_layer) {
    if (!parent_layer.empty() && find_layer(parent_layer) == nullptr) return false;
    if (would_create_cycle(name, parent_layer)) return false;

    for (auto& l : layers_) {
        if (l.name == name) {
            l.parent_layer = std::move(parent_layer);
            return true;
        }
    }
    return false;
}

const Layer2D* LayerManager::find_layer(std::string_view name) const noexcept {
    for (const auto& l : layers_) {
        if (l.name == name) return &l;
    }
    return nullptr;
}

std::vector<const Layer2D*> LayerManager::ordered_layers() const {
    std::vector<const Layer2D*> out;
    out.reserve(layers_.size());
    for (const auto& l : layers_) out.push_back(&l);
    return out;
}

bool LayerManager::is_effectively_visible(std::string_view name) const noexcept {
    std::string_view curr = name;
    for (int depth = 0; depth < 32 && !curr.empty(); ++depth) {
        const auto* l = find_layer(curr);
        if (!l) return true;
        if (!l->visible) return false;
        curr = l->parent_layer;
    }
    return true;
}

bool LayerManager::is_effectively_locked(std::string_view name) const noexcept {
    std::string_view curr = name;
    for (int depth = 0; depth < 32 && !curr.empty(); ++depth) {
        const auto* l = find_layer(curr);
        if (!l) return false;
        if (l->locked) return true;
        curr = l->parent_layer;
    }
    return false;
}

bool LayerManager::can_modify_entity(const SketchEntity& entity) const noexcept {
    return is_effectively_visible(entity.layer) && !is_effectively_locked(entity.layer);
}

size_t LayerManager::reassign_entities(
    SketchDocument2D& doc,
    std::string_view from_layer,
    std::string_view to_layer
) {
    if (find_layer(to_layer) == nullptr) return 0;
    size_t moved = 0;
    for (auto& ent : doc.mutable_entities()) {
        if (ent.layer == from_layer) {
            ent.layer = std::string(to_layer);
            ++moved;
        }
    }
    return moved;
}

} // namespace shunya::cad2d
