#include "shunya/cad2d/layer_manager.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace shunya::cad2d {

namespace {

std::string escape_json_str(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string extract_str(std::string_view json, std::string_view key, std::string_view def = "") {
    std::string pattern = "\"" + std::string(key) + "\"";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return std::string(def);
    pos = json.find(':', pos);
    if (pos == std::string_view::npos) return std::string(def);
    auto q1 = json.find('"', pos);
    if (q1 == std::string_view::npos) return std::string(def);
    auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) return std::string(def);
    return std::string(json.substr(q1 + 1, q2 - q1 - 1));
}

double extract_num(std::string_view json, std::string_view key, double def = 0.0) {
    std::string pattern = "\"" + std::string(key) + "\"";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string_view::npos) return def;
    size_t start = pos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t' || json[start] == '\r' || json[start] == '\n')) {
        ++start;
    }
    size_t end = start;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '.' || json[end] == '-' || json[end] == '+')) {
        ++end;
    }
    if (start == end) return def;
    try {
        return std::stod(std::string(json.substr(start, end - start)));
    } catch (...) {
        return def;
    }
}

bool extract_bool(std::string_view json, std::string_view key, bool def = false) {
    std::string pattern = "\"" + std::string(key) + "\"";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string_view::npos) return def;
    auto t_pos = json.find("true", pos);
    auto f_pos = json.find("false", pos);
    auto next_comma = json.find(',', pos);
    auto next_brace = json.find('}', pos);
    size_t lim = std::min(next_comma, next_brace);
    if (t_pos != std::string_view::npos && t_pos < lim) return true;
    if (f_pos != std::string_view::npos && f_pos < lim) return false;
    return def;
}

} // namespace

LayerManager::LayerManager() {
    layers_.push_back(Layer2D{"0", "", true, false, LayerStyle{"#61afef", 0.25, LinePattern::Solid, 1.0}, std::nullopt, std::nullopt, 1.0});
}

Layer2D* LayerManager::find_layer_mut(std::string_view name) noexcept {
    for (auto& l : layers_) {
        if (l.name == name) return &l;
    }
    return nullptr;
}

const Layer2D* LayerManager::find_layer(std::string_view name) const noexcept {
    for (const auto& l : layers_) {
        if (l.name == name) return &l;
    }
    return nullptr;
}

bool LayerManager::add_layer(std::string name, std::string parent_layer, LayerStyle style) {
    if (name.empty() || find_layer(name) != nullptr) return false;
    if (!parent_layer.empty() && find_layer(parent_layer) == nullptr) {
        return false;
    }
    const double op = std::clamp(style.opacity, 0.0, 1.0);
    layers_.push_back(Layer2D{std::move(name), std::move(parent_layer), true, false, style, std::nullopt, std::nullopt, op});
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
    if (active_layer_ == name) {
        active_layer_ = "0";
    }
    return true;
}

bool LayerManager::set_visible(std::string_view name, bool visible) {
    if (auto* l = find_layer_mut(name)) {
        l->visible = visible;
        return true;
    }
    return false;
}

bool LayerManager::set_locked(std::string_view name, bool locked) {
    if (auto* l = find_layer_mut(name)) {
        l->locked = locked;
        return true;
    }
    return false;
}

bool LayerManager::set_style(std::string_view name, const LayerStyle& style) {
    if (auto* l = find_layer_mut(name)) {
        l->style = style;
        l->opacity = std::clamp(style.opacity, 0.0, 1.0);
        return true;
    }
    return false;
}

bool LayerManager::set_color_override(std::string_view name, std::optional<std::string> color_hex) {
    if (auto* l = find_layer_mut(name)) {
        l->color_override = std::move(color_hex);
        return true;
    }
    return false;
}

bool LayerManager::set_line_weight_override(std::string_view name, std::optional<double> weight_mm) {
    if (auto* l = find_layer_mut(name)) {
        if (weight_mm.has_value() && *weight_mm < 0.0) return false;
        l->line_weight_override = weight_mm;
        return true;
    }
    return false;
}

bool LayerManager::set_opacity(std::string_view name, double opacity) {
    if (auto* l = find_layer_mut(name)) {
        l->opacity = std::clamp(opacity, 0.0, 1.0);
        l->style.opacity = l->opacity;
        return true;
    }
    return false;
}

bool LayerManager::set_active_layer(std::string_view name) {
    if (find_layer(name) == nullptr) return false;
    active_layer_ = std::string(name);
    return true;
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

    if (auto* l = find_layer_mut(name)) {
        l->parent_layer = std::move(parent_layer);
        return true;
    }
    return false;
}

std::vector<const Layer2D*> LayerManager::ordered_layers() const {
    std::vector<const Layer2D*> out;
    out.reserve(layers_.size());
    for (const auto& l : layers_) out.push_back(&l);
    return out;
}

std::vector<std::string> LayerManager::get_children(std::string_view name) const {
    std::vector<std::string> res;
    for (const auto& l : layers_) {
        if (l.parent_layer == name) {
            res.push_back(l.name);
        }
    }
    return res;
}

std::vector<std::string> LayerManager::get_ancestors(std::string_view name) const {
    std::vector<std::string> res;
    std::string_view curr = name;
    for (int depth = 0; depth < 64; ++depth) {
        const auto* l = find_layer(curr);
        if (!l || l->parent_layer.empty()) break;
        res.push_back(l->parent_layer);
        curr = l->parent_layer;
    }
    return res;
}

bool LayerManager::move_layer_up(std::string_view name) {
    for (size_t i = 1; i < layers_.size(); ++i) {
        if (layers_[i].name == name) {
            std::swap(layers_[i], layers_[i - 1]);
            return true;
        }
    }
    return false;
}

bool LayerManager::move_layer_down(std::string_view name) {
    for (size_t i = 0; i + 1 < layers_.size(); ++i) {
        if (layers_[i].name == name) {
            std::swap(layers_[i], layers_[i + 1]);
            return true;
        }
    }
    return false;
}

bool LayerManager::reorder_layer(std::string_view name, size_t target_index) {
    if (target_index >= layers_.size()) return false;
    auto it = std::find_if(layers_.begin(), layers_.end(), [&](const Layer2D& l) {
        return l.name == name;
    });
    if (it == layers_.end()) return false;
    size_t curr_idx = static_cast<size_t>(std::distance(layers_.begin(), it));
    if (curr_idx == target_index) return true;

    Layer2D item = std::move(*it);
    layers_.erase(it);
    layers_.insert(layers_.begin() + static_cast<ptrdiff_t>(target_index), std::move(item));
    return true;
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

double LayerManager::effective_opacity(std::string_view name) const noexcept {
    double comp = 1.0;
    std::string_view curr = name;
    for (int depth = 0; depth < 32 && !curr.empty(); ++depth) {
        const auto* l = find_layer(curr);
        if (!l) break;
        comp *= l->opacity;
        curr = l->parent_layer;
    }
    return std::clamp(comp, 0.0, 1.0);
}

std::string LayerManager::effective_color(std::string_view name) const {
    std::string_view curr = name;
    for (int depth = 0; depth < 32 && !curr.empty(); ++depth) {
        const auto* l = find_layer(curr);
        if (!l) break;
        if (l->color_override.has_value()) {
            return *l->color_override;
        }
        if (!l->style.color_hex.empty()) {
            return l->style.color_hex;
        }
        curr = l->parent_layer;
    }
    return "#61afef";
}

double LayerManager::effective_line_weight(std::string_view name) const noexcept {
    std::string_view curr = name;
    for (int depth = 0; depth < 32 && !curr.empty(); ++depth) {
        const auto* l = find_layer(curr);
        if (!l) break;
        if (l->line_weight_override.has_value()) {
            return *l->line_weight_override;
        }
        if (l->style.line_weight_mm > 0.0) {
            return l->style.line_weight_mm;
        }
        curr = l->parent_layer;
    }
    return 0.25;
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

std::string LayerManager::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"active_layer\": \"" << escape_json_str(active_layer_) << "\",\n";
    ss << "  \"layers\": [\n";
    for (size_t i = 0; i < layers_.size(); ++i) {
        const auto& l = layers_[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << escape_json_str(l.name) << "\",\n";
        ss << "      \"parent_layer\": \"" << escape_json_str(l.parent_layer) << "\",\n";
        ss << "      \"visible\": " << (l.visible ? "true" : "false") << ",\n";
        ss << "      \"locked\": " << (l.locked ? "true" : "false") << ",\n";
        ss << "      \"opacity\": " << l.opacity << ",\n";
        if (l.color_override.has_value()) {
            ss << "      \"color_override\": \"" << escape_json_str(*l.color_override) << "\",\n";
        }
        if (l.line_weight_override.has_value()) {
            ss << "      \"line_weight_override\": " << *l.line_weight_override << ",\n";
        }
        ss << "      \"style\": {\n";
        ss << "        \"color_hex\": \"" << escape_json_str(l.style.color_hex) << "\",\n";
        ss << "        \"line_weight_mm\": " << l.style.line_weight_mm << ",\n";
        ss << "        \"pattern\": \"" << escape_json_str(line_pattern_to_string(l.style.pattern)) << "\",\n";
        ss << "        \"opacity\": " << l.style.opacity << "\n";
        ss << "      }\n";
        ss << "    }" << (i + 1 < layers_.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

bool LayerManager::from_json(std::string_view json_str) {
    if (json_str.empty()) return false;
    auto layers_pos = json_str.find("\"layers\"");
    if (layers_pos == std::string_view::npos) return false;
    auto open_bracket = json_str.find('[', layers_pos);
    if (open_bracket == std::string_view::npos) return false;
    auto close_bracket = json_str.rfind(']');
    if (close_bracket == std::string_view::npos || close_bracket <= open_bracket) return false;

    std::vector<Layer2D> loaded;
    size_t cursor = open_bracket + 1;
    while (cursor < close_bracket) {
        auto obj_start = json_str.find('{', cursor);
        if (obj_start == std::string_view::npos || obj_start >= close_bracket) break;
        // find matching closing brace
        int depth = 0;
        size_t obj_end = obj_start;
        for (; obj_end < close_bracket; ++obj_end) {
            if (json_str[obj_end] == '{') ++depth;
            else if (json_str[obj_end] == '}') {
                --depth;
                if (depth == 0) break;
            }
        }
        if (depth != 0) break;
        std::string_view block = json_str.substr(obj_start, obj_end - obj_start + 1);

        Layer2D l;
        l.name = extract_str(block, "name");
        l.parent_layer = extract_str(block, "parent_layer");
        l.visible = extract_bool(block, "visible", true);
        l.locked = extract_bool(block, "locked", false);
        l.opacity = std::clamp(extract_num(block, "opacity", 1.0), 0.0, 1.0);
        
        std::string cov = extract_str(block, "color_override");
        if (!cov.empty()) {
            l.color_override = cov;
        }
        if (block.find("\"line_weight_override\"") != std::string_view::npos) {
            l.line_weight_override = extract_num(block, "line_weight_override", 0.25);
        }

        auto style_pos = block.find("\"style\"");
        if (style_pos != std::string_view::npos) {
            std::string_view style_block = block.substr(style_pos);
            l.style.color_hex = extract_str(style_block, "color_hex", "#61afef");
            l.style.line_weight_mm = extract_num(style_block, "line_weight_mm", 0.25);
            l.style.pattern = string_to_line_pattern(extract_str(style_block, "pattern", "Solid"));
            l.style.opacity = std::clamp(extract_num(style_block, "opacity", 1.0), 0.0, 1.0);
        }

        if (!l.name.empty()) {
            loaded.push_back(std::move(l));
        }
        cursor = obj_end + 1;
    }

    if (loaded.empty()) return false;
    layers_ = std::move(loaded);
    active_layer_ = extract_str(json_str, "active_layer", "0");
    if (find_layer(active_layer_) == nullptr && !layers_.empty()) {
        active_layer_ = layers_.front().name;
    }
    return true;
}

} // namespace shunya::cad2d
