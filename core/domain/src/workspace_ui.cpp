#include "shunya/core/domain/workspace_ui.hpp"
#include "shunya/core/base/fuzzy_match.hpp"

#include <algorithm>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_ui_error(std::string message, int32_t code = 3001) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::System,
        .severity = base::Severity::Error,
        .message = std::move(message),
        .details = ""
    };
}

} // namespace

// =============================================================================
// UI-01: Workspace Shell & Docking Layout
// =============================================================================

namespace {

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct SimpleJson {
    JsonType type{JsonType::Null};
    bool bool_val{false};
    double num_val{0.0};
    std::string str_val;
    std::vector<SimpleJson> arr_val;
    std::vector<std::pair<std::string, SimpleJson>> obj_val;

    [[nodiscard]] const SimpleJson* get(std::string_view key) const {
        for (const auto& [k, v] : obj_val) {
            if (k == key) return &v;
        }
        return nullptr;
    }

    [[nodiscard]] std::string get_string(std::string_view key, std::string default_val = "") const {
        const auto* p = get(key);
        return (p && p->type == JsonType::String) ? p->str_val : default_val;
    }

    [[nodiscard]] double get_number(std::string_view key, double default_val = 0.0) const {
        const auto* p = get(key);
        return (p && p->type == JsonType::Number) ? p->num_val : default_val;
    }

    [[nodiscard]] bool get_bool(std::string_view key, bool default_val = false) const {
        const auto* p = get(key);
        return (p && p->type == JsonType::Bool) ? p->bool_val : default_val;
    }
};

void skip_json_ws(std::string_view s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) {
        ++pos;
    }
}

SimpleJson parse_simple_json_val(std::string_view s, size_t& pos);

std::string parse_simple_json_str(std::string_view s, size_t& pos) {
    std::string result;
    if (pos >= s.size() || s[pos] != '"') return result;
    ++pos; // skip opening quote
    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') {
            return result;
        }
        if (c == '\\' && pos < s.size()) {
            char esc = s[pos++];
            if (esc == '"') result.push_back('"');
            else if (esc == '\\') result.push_back('\\');
            else if (esc == 'n') result.push_back('\n');
            else if (esc == 'r') result.push_back('\r');
            else if (esc == 't') result.push_back('\t');
            else result.push_back(esc);
        } else {
            result.push_back(c);
        }
    }
    return result;
}

SimpleJson parse_simple_json_val(std::string_view s, size_t& pos) {
    skip_json_ws(s, pos);
    SimpleJson val;
    if (pos >= s.size()) return val;

    if (s[pos] == '"') {
        val.type = JsonType::String;
        val.str_val = parse_simple_json_str(s, pos);
        return val;
    }

    if (s[pos] == '{') {
        val.type = JsonType::Object;
        ++pos;
        while (pos < s.size()) {
            skip_json_ws(s, pos);
            if (pos < s.size() && s[pos] == '}') {
                ++pos;
                break;
            }
            if (s[pos] != '"') break;
            std::string key = parse_simple_json_str(s, pos);
            skip_json_ws(s, pos);
            if (pos < s.size() && s[pos] == ':') ++pos;
            SimpleJson child = parse_simple_json_val(s, pos);
            val.obj_val.emplace_back(std::move(key), std::move(child));
            skip_json_ws(s, pos);
            if (pos < s.size() && s[pos] == ',') ++pos;
        }
        return val;
    }

    if (s[pos] == '[') {
        val.type = JsonType::Array;
        ++pos;
        while (pos < s.size()) {
            skip_json_ws(s, pos);
            if (pos < s.size() && s[pos] == ']') {
                ++pos;
                break;
            }
            SimpleJson child = parse_simple_json_val(s, pos);
            val.arr_val.push_back(std::move(child));
            skip_json_ws(s, pos);
            if (pos < s.size() && s[pos] == ',') ++pos;
        }
        return val;
    }

    if (s.substr(pos, 4) == "true") {
        val.type = JsonType::Bool;
        val.bool_val = true;
        pos += 4;
        return val;
    }
    if (s.substr(pos, 5) == "false") {
        val.type = JsonType::Bool;
        val.bool_val = false;
        pos += 5;
        return val;
    }
    if (s.substr(pos, 4) == "null") {
        val.type = JsonType::Null;
        pos += 4;
        return val;
    }

    // Number
    size_t start = pos;
    if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) ++pos;
    while (pos < s.size() && ((s[pos] >= '0' && s[pos] <= '9') || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E')) {
        ++pos;
    }
    std::string num_str(s.substr(start, pos - start));
    val.type = JsonType::Number;
    try {
        val.num_val = std::stod(num_str);
    } catch (...) {
        val.num_val = 0.0;
    }
    return val;
}

std::shared_ptr<DockNode> parse_dock_node_from_json(const SimpleJson& obj) {
    if (obj.type != JsonType::Object) return nullptr;

    std::string id = obj.get_string("node_id", "node");
    std::string type_str = obj.get_string("type", "Leaf");

    if (type_str == "Leaf") {
        auto node = std::make_shared<DockNode>();
        node->node_id = id;
        node->type = DockNodeType::Leaf;
        node->panel_id = obj.get_string("panel_id", id);
        node->title = obj.get_string("title", node->panel_id);
        node->is_visible = obj.get_bool("is_visible", true);
        node->is_closable = obj.get_bool("is_closable", true);
        node->is_floating = obj.get_bool("is_floating", false);
        node->min_width = obj.get_number("min_width", 160.0);
        node->min_height = obj.get_number("min_height", 120.0);
        return node;
    }

    if (type_str == "SplitHorizontal" || type_str == "SplitVertical") {
        auto node = std::make_shared<DockNode>();
        node->node_id = id;
        node->type = (type_str == "SplitHorizontal") ? DockNodeType::SplitHorizontal : DockNodeType::SplitVertical;
        node->split_ratio = obj.get_number("split_ratio", 0.5);

        const auto* first = obj.get("first_child");
        if (first) node->first_child = parse_dock_node_from_json(*first);

        const auto* second = obj.get("second_child");
        if (second) node->second_child = parse_dock_node_from_json(*second);

        return node;
    }

    if (type_str == "Tabbed") {
        auto node = std::make_shared<DockNode>();
        node->node_id = id;
        node->type = DockNodeType::Tabbed;
        node->active_tab_index = static_cast<size_t>(obj.get_number("active_tab_index", 0.0));

        const auto* tabs_arr = obj.get("tabs");
        if (tabs_arr && tabs_arr->type == JsonType::Array) {
            for (const auto& tab_elem : tabs_arr->arr_val) {
                auto child = parse_dock_node_from_json(tab_elem);
                if (child) node->tabs.push_back(std::move(child));
            }
        }
        return node;
    }

    return nullptr;
}

} // namespace

// =============================================================================
// DockNode Implementation
// =============================================================================

std::shared_ptr<DockNode> DockNode::make_leaf(std::string node_id, std::string panel_id, std::string title, bool is_visible) {
    auto node = std::make_shared<DockNode>();
    node->node_id = std::move(node_id);
    node->type = DockNodeType::Leaf;
    node->panel_id = std::move(panel_id);
    node->title = std::move(title);
    node->is_visible = is_visible;
    return node;
}

std::shared_ptr<DockNode> DockNode::make_split(std::string node_id, DockNodeType split_type, double ratio, std::shared_ptr<DockNode> first, std::shared_ptr<DockNode> second) {
    auto node = std::make_shared<DockNode>();
    node->node_id = std::move(node_id);
    node->type = split_type;
    node->split_ratio = std::clamp(ratio, 0.05, 0.95);
    node->first_child = std::move(first);
    node->second_child = std::move(second);
    return node;
}

std::shared_ptr<DockNode> DockNode::make_tabbed(std::string node_id, std::vector<std::shared_ptr<DockNode>> tabs, size_t active_idx) {
    auto node = std::make_shared<DockNode>();
    node->node_id = std::move(node_id);
    node->type = DockNodeType::Tabbed;
    node->tabs = std::move(tabs);
    node->active_tab_index = active_idx;
    return node;
}

std::shared_ptr<DockNode> DockNode::clone() const {
    auto c = std::make_shared<DockNode>();
    c->node_id = node_id;
    c->type = type;
    c->panel_id = panel_id;
    c->title = title;
    c->is_visible = is_visible;
    c->is_closable = is_closable;
    c->is_floating = is_floating;
    c->min_width = min_width;
    c->min_height = min_height;
    c->split_ratio = split_ratio;
    c->active_tab_index = active_tab_index;

    if (first_child) c->first_child = first_child->clone();
    if (second_child) c->second_child = second_child->clone();

    for (const auto& t : tabs) {
        if (t) c->tabs.push_back(t->clone());
    }
    return c;
}

std::shared_ptr<DockNode> DockNode::find_node(std::string_view id) const {
    if (node_id == id) return std::const_pointer_cast<DockNode>(shared_from_this());

    if (first_child) {
        auto found = first_child->find_node(id);
        if (found) return found;
    }
    if (second_child) {
        auto found = second_child->find_node(id);
        if (found) return found;
    }
    for (const auto& t : tabs) {
        if (t) {
            auto found = t->find_node(id);
            if (found) return found;
        }
    }
    return nullptr;
}

std::shared_ptr<DockNode> DockNode::find_panel(std::string_view pid) const {
    if (type == DockNodeType::Leaf && panel_id == pid) {
        return std::const_pointer_cast<DockNode>(shared_from_this());
    }

    if (first_child) {
        auto found = first_child->find_panel(pid);
        if (found) return found;
    }
    if (second_child) {
        auto found = second_child->find_panel(pid);
        if (found) return found;
    }
    for (const auto& t : tabs) {
        if (t) {
            auto found = t->find_panel(pid);
            if (found) return found;
        }
    }
    return nullptr;
}

void DockNode::collect_leaves(std::vector<std::shared_ptr<DockNode>>& out) const {
    if (type == DockNodeType::Leaf) {
        out.push_back(std::const_pointer_cast<DockNode>(shared_from_this()));
        return;
    }
    if (first_child) first_child->collect_leaves(out);
    if (second_child) second_child->collect_leaves(out);
    for (const auto& t : tabs) {
        if (t) t->collect_leaves(out);
    }
}

std::string DockNode::to_json(int indent) const {
    std::string ind(static_cast<size_t>(indent), ' ');
    std::string ind2(static_cast<size_t>(indent + 2), ' ');
    std::ostringstream ss;

    ss << "{\n";
    ss << ind2 << "\"node_id\": \"" << node_id << "\",\n";
    ss << ind2 << "\"type\": \"" << dock_node_type_to_string(type) << "\"";

    if (type == DockNodeType::Leaf) {
        ss << ",\n" << ind2 << "\"panel_id\": \"" << panel_id << "\",\n";
        ss << ind2 << "\"title\": \"" << title << "\",\n";
        ss << ind2 << "\"is_visible\": " << (is_visible ? "true" : "false") << ",\n";
        ss << ind2 << "\"is_closable\": " << (is_closable ? "true" : "false") << ",\n";
        ss << ind2 << "\"is_floating\": " << (is_floating ? "true" : "false") << ",\n";
        ss << ind2 << "\"min_width\": " << min_width << ",\n";
        ss << ind2 << "\"min_height\": " << min_height << "\n";
    } else if (type == DockNodeType::SplitHorizontal || type == DockNodeType::SplitVertical) {
        ss << ",\n" << ind2 << "\"split_ratio\": " << split_ratio << ",\n";
        ss << ind2 << "\"first_child\": " << (first_child ? first_child->to_json(indent + 2) : "null") << ",\n";
        ss << ind2 << "\"second_child\": " << (second_child ? second_child->to_json(indent + 2) : "null") << "\n";
    } else if (type == DockNodeType::Tabbed) {
        ss << ",\n" << ind2 << "\"active_tab_index\": " << active_tab_index << ",\n";
        ss << ind2 << "\"tabs\": [\n";
        for (size_t i = 0; i < tabs.size(); ++i) {
            ss << ind2 << "  " << (tabs[i] ? tabs[i]->to_json(indent + 4) : "null");
            if (i + 1 < tabs.size()) ss << ",";
            ss << "\n";
        }
        ss << ind2 << "]\n";
    }

    ss << ind << "}";
    return ss.str();
}

std::shared_ptr<DockNode> DockNode::from_json_str(std::string_view json_str) {
    size_t pos = 0;
    SimpleJson root_val = parse_simple_json_val(json_str, pos);
    return parse_dock_node_from_json(root_val);
}

// =============================================================================
// DockingManager Implementation
// =============================================================================

DockingManager::DockingManager() {
    apply_preset(WorkspacePreset::Modeling);
}

void DockingManager::set_root(std::shared_ptr<DockNode> root) {
    root_ = std::move(root);
}

std::shared_ptr<DockNode> DockNodePtrHelperFind(std::shared_ptr<DockNode> n, std::string_view id) {
    if (!n) return nullptr;
    if (n->node_id == id) return n;
    if (n->first_child) {
        auto r = DockNodePtrHelperFind(n->first_child, id);
        if (r) return r;
    }
    if (n->second_child) {
        auto r = DockNodePtrHelperFind(n->second_child, id);
        if (r) return r;
    }
    for (const auto& t : n->tabs) {
        auto r = DockNodePtrHelperFind(t, id);
        if (r) return r;
    }
    return nullptr;
}

std::shared_ptr<DockNode> DockNodePtrHelperFindPanel(std::shared_ptr<DockNode> n, std::string_view pid) {
    if (!n) return nullptr;
    if (n->type == DockNodeType::Leaf && n->panel_id == pid) return n;
    if (n->first_child) {
        auto r = DockNodePtrHelperFindPanel(n->first_child, pid);
        if (r) return r;
    }
    if (n->second_child) {
        auto r = DockNodePtrHelperFindPanel(n->second_child, pid);
        if (r) return r;
    }
    for (const auto& t : n->tabs) {
        auto r = DockNodePtrHelperFindPanel(t, pid);
        if (r) return r;
    }
    return nullptr;
}

std::shared_ptr<DockNode> DockingManager::find_node(std::string_view node_id) const {
    return DockNodePtrHelperFind(root_, node_id);
}

std::shared_ptr<DockNode> DockingManager::find_panel(std::string_view panel_id) const {
    return DockNodePtrHelperFindPanel(root_, panel_id);
}

bool DockingManager::replace_child_in_tree(std::shared_ptr<DockNode> current, std::shared_ptr<DockNode> target, std::shared_ptr<DockNode> replacement) {
    if (!current || !target || !replacement) return false;

    if (current == target) {
        root_ = replacement;
        return true;
    }

    if (current->first_child == target) {
        current->first_child = replacement;
        return true;
    }
    if (current->second_child == target) {
        current->second_child = replacement;
        return true;
    }

    for (size_t i = 0; i < current->tabs.size(); ++i) {
        if (current->tabs[i] == target) {
            current->tabs[i] = replacement;
            return true;
        }
    }

    if (current->first_child && replace_child_in_tree(current->first_child, target, replacement)) return true;
    if (current->second_child && replace_child_in_tree(current->second_child, target, replacement)) return true;

    for (const auto& t : current->tabs) {
        if (t && replace_child_in_tree(t, target, replacement)) return true;
    }

    return false;
}

bool DockingManager::dock_panel(std::string_view panel_id, std::string_view title, std::string_view target_node_id, DockSnapZone snap_zone) {
    if (snap_zone == DockSnapZone::None) return false;

    auto target = find_node(target_node_id);
    if (!target) return false;

    // Create new leaf
    auto new_leaf = DockNode::make_leaf("node_" + std::string(panel_id), std::string(panel_id), std::string(title));

    if (snap_zone == DockSnapZone::CenterTab) {
        if (target->type == DockNodeType::Tabbed) {
            target->tabs.push_back(new_leaf);
            target->active_tab_index = target->tabs.size() - 1;
            return true;
        }
        auto tabbed = DockNode::make_tabbed("tab_" + target->node_id, {target->clone(), new_leaf}, 1);
        return replace_child_in_tree(root_, target, tabbed);
    }

    DockNodeType split_type = (snap_zone == DockSnapZone::North || snap_zone == DockSnapZone::South)
                                  ? DockNodeType::SplitVertical
                                  : DockNodeType::SplitHorizontal;

    std::shared_ptr<DockNode> first = (snap_zone == DockSnapZone::North || snap_zone == DockSnapZone::West)
                                          ? new_leaf
                                          : target->clone();
    std::shared_ptr<DockNode> second = (snap_zone == DockSnapZone::North || snap_zone == DockSnapZone::West)
                                           ? target->clone()
                                           : new_leaf;

    auto split = DockNode::make_split("split_" + target->node_id, split_type, 0.5, first, second);
    return replace_child_in_tree(root_, target, split);
}

bool DockingManager::remove_child_from_tree(std::shared_ptr<DockNode> current, std::string_view node_id) {
    if (!current) return false;

    // Check direct children of split
    if (current->first_child && current->first_child->node_id == node_id) {
        // Replace current with second_child
        replace_child_in_tree(root_, current, current->second_child);
        return true;
    }
    if (current->second_child && current->second_child->node_id == node_id) {
        // Replace current with first_child
        replace_child_in_tree(root_, current, current->first_child);
        return true;
    }

    // Check tabs
    for (auto it = current->tabs.begin(); it != current->tabs.end(); ++it) {
        if ((*it)->node_id == node_id) {
            current->tabs.erase(it);
            if (current->tabs.size() == 1) {
                replace_child_in_tree(root_, current, current->tabs[0]);
            } else if (current->active_tab_index >= current->tabs.size()) {
                current->active_tab_index = current->tabs.empty() ? 0 : current->tabs.size() - 1;
            }
            return true;
        }
    }

    if (current->first_child && remove_child_from_tree(current->first_child, node_id)) return true;
    if (current->second_child && remove_child_from_tree(current->second_child, node_id)) return true;
    for (const auto& t : current->tabs) {
        if (t && remove_child_from_tree(t, node_id)) return true;
    }

    return false;
}

bool DockingManager::remove_node(std::string_view node_id) {
    if (!root_) return false;
    if (root_->node_id == node_id) {
        root_ = nullptr;
        return true;
    }
    return remove_child_from_tree(root_, node_id);
}

bool DockingManager::toggle_panel_visibility(std::string_view panel_id, bool visible) {
    auto panel = find_panel(panel_id);
    if (!panel) return false;
    panel->is_visible = visible;
    return true;
}

bool DockingManager::set_split_ratio(std::string_view split_node_id, double ratio) {
    auto node = find_node(split_node_id);
    if (!node || (node->type != DockNodeType::SplitHorizontal && node->type != DockNodeType::SplitVertical)) {
        return false;
    }
    node->split_ratio = std::clamp(ratio, 0.05, 0.95);
    return true;
}

bool DockingManager::set_active_tab(std::string_view tab_node_id, size_t index) {
    auto node = find_node(tab_node_id);
    if (!node || node->type != DockNodeType::Tabbed || node->tabs.empty()) {
        return false;
    }
    node->active_tab_index = std::min(index, node->tabs.size() - 1);
    return true;
}

bool DockingManager::set_floating(std::string_view panel_id, bool floating) {
    auto panel = find_panel(panel_id);
    if (!panel) return false;
    panel->is_floating = floating;
    return true;
}

std::vector<std::shared_ptr<DockNode>> DockingManager::get_all_leaves() const {
    std::vector<std::shared_ptr<DockNode>> leaves;
    if (root_) root_->collect_leaves(leaves);
    return leaves;
}

std::string DockingManager::serialize_layout_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schema_version\": 1,\n";
    ss << "  \"root\": " << (root_ ? root_->to_json(2) : "null") << "\n";
    ss << "}";
    return ss.str();
}

base::Result<void> DockingManager::restore_layout_json(std::string_view json_str) {
    size_t pos = 0;
    SimpleJson doc = parse_simple_json_val(json_str, pos);
    if (doc.type != JsonType::Object) {
        return base::Result<void>::err(make_ui_error("Invalid docking layout JSON"));
    }
    const auto* root_val = doc.get("root");
    if (!root_val) {
        return base::Result<void>::err(make_ui_error("Missing 'root' in docking layout JSON"));
    }
    auto new_root = parse_dock_node_from_json(*root_val);
    if (!new_root) {
        return base::Result<void>::err(make_ui_error("Failed to parse docking root node"));
    }
    root_ = std::move(new_root);
    return base::Result<void>::ok();
}

void DockingManager::apply_preset(WorkspacePreset preset) {
    switch (preset) {
        case WorkspacePreset::Modeling: {
            auto left = DockNode::make_leaf("node_project_browser", "project_browser", "Project Browser");
            auto viewport = DockNode::make_leaf("node_viewport_3d", "viewport_3d", "3D Viewport");
            auto sim_bar = DockNode::make_leaf("node_simulation_toolbar", "simulation_toolbar", "Simulation Toolbar");
            auto center = DockNode::make_split("split_center_sim", DockNodeType::SplitVertical, 0.75, viewport, sim_bar);
            auto right = DockNode::make_leaf("node_property_inspector", "property_inspector", "Property Inspector");
            auto center_right = DockNode::make_split("split_center_right", DockNodeType::SplitHorizontal, 0.75, center, right);
            root_ = DockNode::make_split("root_split", DockNodeType::SplitHorizontal, 0.20, left, center_right);
            break;
        }
        case WorkspacePreset::Schematic: {
            auto left = DockNode::make_leaf("node_project_browser", "project_browser", "Component Library");
            auto canvas = DockNode::make_leaf("node_schematic_editor", "schematic_editor", "Schematic Canvas");
            auto right = DockNode::make_leaf("node_property_inspector", "property_inspector", "Property Inspector");
            auto center_right = DockNode::make_split("split_canvas_props", DockNodeType::SplitHorizontal, 0.75, canvas, right);
            root_ = DockNode::make_split("root_split", DockNodeType::SplitHorizontal, 0.22, left, center_right);
            break;
        }
        case WorkspacePreset::Simulation: {
            auto left = DockNode::make_leaf("node_simulation_toolbar", "simulation_toolbar", "Simulation Controls");
            auto viewport = DockNode::make_leaf("node_viewport_3d", "viewport_3d", "3D Viewport");
            auto results = DockNode::make_leaf("node_results_workspace", "results_workspace", "Live Telemetry & Traces");
            auto center_right = DockNode::make_split("split_viewport_results", DockNodeType::SplitHorizontal, 0.55, viewport, results);
            root_ = DockNode::make_split("root_split", DockNodeType::SplitHorizontal, 0.20, left, center_right);
            break;
        }
        case WorkspacePreset::Results: {
            auto center = DockNode::make_leaf("node_results_workspace", "results_workspace", "Dashboards & Traces");
            auto right = DockNode::make_leaf("node_property_inspector", "property_inspector", "Plot Inspector");
            root_ = DockNode::make_split("root_split", DockNodeType::SplitHorizontal, 0.75, center, right);
            break;
        }
        case WorkspacePreset::Scripting: {
            auto left = DockNode::make_leaf("node_project_browser", "project_browser", "Script Files");
            auto editor = DockNode::make_leaf("node_code_editor", "code_editor", "Script Editor");
            root_ = DockNode::make_split("root_split", DockNodeType::SplitHorizontal, 0.25, left, editor);
            break;
        }
    }
}

// =============================================================================
// DockActionManager Implementation
// =============================================================================

DockActionManager::DockActionManager() {
    register_shortcut(DockActionShortcut{
        .action_id = "panel.project_browser",
        .key_sequence = "F9",
        .target_panel_id = "project_browser",
        .description = "Toggle Project Browser",
        .is_global = true
    });
    register_shortcut(DockActionShortcut{
        .action_id = "panel.simulation_toolbar",
        .key_sequence = "F10",
        .target_panel_id = "simulation_toolbar",
        .description = "Toggle Simulation Controls",
        .is_global = true
    });
    register_shortcut(DockActionShortcut{
        .action_id = "panel.property_inspector",
        .key_sequence = "F12",
        .target_panel_id = "property_inspector",
        .description = "Toggle Property Inspector",
        .is_global = true
    });
    register_shortcut(DockActionShortcut{
        .action_id = "view.toggle_sidebars",
        .key_sequence = "Ctrl+B",
        .target_panel_id = "",
        .description = "Toggle All Sidebars",
        .is_global = true
    });
    register_shortcut(DockActionShortcut{
        .action_id = "view.toggle_console",
        .key_sequence = "Ctrl+J",
        .target_panel_id = "simulation_toolbar",
        .description = "Toggle Diagnostics Console",
        .is_global = true
    });
}

void DockActionManager::register_shortcut(DockActionShortcut shortcut) {
    shortcuts_.push_back(std::move(shortcut));
}

std::optional<DockActionShortcut> DockActionManager::find_by_key(std::string_view key_seq) const {
    for (const auto& s : shortcuts_) {
        if (s.key_sequence == key_seq) return s;
    }
    return std::nullopt;
}

std::optional<DockActionShortcut> DockActionManager::find_by_action(std::string_view action_id) const {
    for (const auto& s : shortcuts_) {
        if (s.action_id == action_id) return s;
    }
    return std::nullopt;
}

bool DockActionManager::execute_shortcut(std::string_view key_seq, DockingManager& manager) {
    auto s = find_by_key(key_seq);
    if (!s) return false;

    if (!s->target_panel_id.empty()) {
        auto panel = manager.find_panel(s->target_panel_id);
        if (panel) {
            manager.toggle_panel_visibility(s->target_panel_id, !panel->is_visible);
            return true;
        }
    } else if (s->action_id == "view.toggle_sidebars") {
        auto left = manager.find_panel("project_browser");
        auto right = manager.find_panel("property_inspector");
        bool any_visible = (left && left->is_visible) || (right && right->is_visible);
        if (left) manager.toggle_panel_visibility("project_browser", !any_visible);
        if (right) manager.toggle_panel_visibility("property_inspector", !any_visible);
        return true;
    }
    return false;
}

std::optional<WorkspacePreset> workspace_preset_from_string(std::string_view s) noexcept {
    if (s == "Modeling") return WorkspacePreset::Modeling;
    if (s == "Schematic") return WorkspacePreset::Schematic;
    if (s == "Simulation") return WorkspacePreset::Simulation;
    if (s == "Results") return WorkspacePreset::Results;
    if (s == "Scripting") return WorkspacePreset::Scripting;
    return std::nullopt;
}

// =============================================================================
// QuickAccessToolbarModel Implementation
// =============================================================================

QuickAccessToolbarModel::QuickAccessToolbarModel() {
    pinned_ids_ = {"file.save", "edit.undo", "edit.redo", "sim.run", "sim.step"};
}

void QuickAccessToolbarModel::pin_action(std::string action_id) {
    if (!is_pinned(action_id)) {
        pinned_ids_.push_back(std::move(action_id));
    }
}

void QuickAccessToolbarModel::unpin_action(std::string_view action_id) {
    auto it = std::find(pinned_ids_.begin(), pinned_ids_.end(), action_id);
    if (it != pinned_ids_.end()) {
        pinned_ids_.erase(it);
    }
}

bool QuickAccessToolbarModel::is_pinned(std::string_view action_id) const {
    return std::find(pinned_ids_.begin(), pinned_ids_.end(), action_id) != pinned_ids_.end();
}

// =============================================================================
// RibbonBarModel Implementation
// =============================================================================

RibbonBarModel::RibbonBarModel() {
    setup_default_ribbon();
}

void RibbonBarModel::setup_default_ribbon() {
    tabs_.clear();

    // 1. Home / General Tab
    RibbonTab home_tab{
        .id = "tab_home",
        .title = "Home",
        .category = RibbonCategory::General,
        .is_contextual = false,
        .is_active = true,
        .groups = {
            RibbonGroup{
                .id = "grp_project",
                .title = "Project",
                .actions = {
                    RibbonActionItem::make("file.new", "New", "new_project", "Create new simulation project (Ctrl+N)", "Ctrl+N"),
                    RibbonActionItem::make("file.open", "Open", "open_project", "Open existing project (Ctrl+O)", "Ctrl+O"),
                    RibbonActionItem::make("file.save", "Save", "save_project", "Save project state (Ctrl+S)", "Ctrl+S")
                }
            },
            RibbonGroup{
                .id = "grp_clipboard",
                .title = "Clipboard",
                .actions = {
                    RibbonActionItem::make("edit.cut", "Cut", "cut", "Cut selection to clipboard (Ctrl+X)", "Ctrl+X", RibbonItemSize::Small),
                    RibbonActionItem::make("edit.copy", "Copy", "copy", "Copy selection to clipboard (Ctrl+C)", "Ctrl+C", RibbonItemSize::Small),
                    RibbonActionItem::make("edit.paste", "Paste", "paste", "Paste from clipboard (Ctrl+V)", "Ctrl+V", RibbonItemSize::Large)
                }
            },
            RibbonGroup{
                .id = "grp_history",
                .title = "History",
                .actions = {
                    RibbonActionItem::make("edit.undo", "Undo", "undo", "Undo last command (Ctrl+Z)", "Ctrl+Z"),
                    RibbonActionItem::make("edit.redo", "Redo", "redo", "Redo command (Ctrl+Y)", "Ctrl+Y")
                }
            }
        }
    };
    tabs_.push_back(std::move(home_tab));

    // 2. 2D Sketch Tab (Contextual)
    RibbonTab sketch_tab{
        .id = "tab_sketch",
        .title = "2D Sketch",
        .category = RibbonCategory::Sketch2D,
        .is_contextual = true,
        .is_active = false,
        .groups = {
            RibbonGroup{
                .id = "grp_draw",
                .title = "Draw",
                .actions = {
                    RibbonActionItem::make("draw.line", "Line", "tool_line", "Draw 2D parametric line"),
                    RibbonActionItem::make("draw.circle", "Circle", "tool_circle", "Draw parametric circle/arc"),
                    RibbonActionItem::make("draw.rect", "Rectangle", "tool_rect", "Draw rectangle")
                }
            },
            RibbonGroup{
                .id = "grp_constraints",
                .title = "Constraints",
                .actions = {
                    RibbonActionItem::make("geom.coincident", "Coincident", "c_coincident", "Snap vertices together"),
                    RibbonActionItem::make("geom.parallel", "Parallel", "c_parallel", "Parallel constraint"),
                    RibbonActionItem::make("geom.tangent", "Tangent", "c_tangent", "Tangent constraint")
                }
            },
            RibbonGroup{
                .id = "grp_booleans",
                .title = "Booleans",
                .actions = {
                    RibbonActionItem::make("bool.union", "Union", "b_union", "Polygon Boolean Union"),
                    RibbonActionItem::make("bool.diff", "Subtract", "b_diff", "Polygon Boolean Difference")
                }
            }
        }
    };
    tabs_.push_back(std::move(sketch_tab));

    // 3. 3D Mesh Tab
    RibbonTab mesh_tab{
        .id = "tab_mesh",
        .title = "3D Mesh",
        .category = RibbonCategory::Mesh3D,
        .is_contextual = false,
        .is_active = true,
        .groups = {
            RibbonGroup{
                .id = "grp_features",
                .title = "Solid Features",
                .actions = {
                    RibbonActionItem::make("feat.extrude", "Extrude", "extrude", "Extrude 2D sketch into 3D solid"),
                    RibbonActionItem::make("feat.revolve", "Revolve", "revolve", "Revolve sketch about axis"),
                    RibbonActionItem::make("feat.sweep", "Sweep", "sweep", "Sweep profile along 3D trajectory")
                }
            },
            RibbonGroup{
                .id = "grp_modify",
                .title = "Modify",
                .actions = {
                    RibbonActionItem::make("mesh.fillet", "Fillet", "fillet", "3D edge fillet"),
                    RibbonActionItem::make("mesh.chamfer", "Chamfer", "chamfer", "Planar edge chamfer"),
                    RibbonActionItem::make("mesh.shell", "Shell", "shell", "Hollow thin-wall shell")
                }
            }
        }
    };
    tabs_.push_back(std::move(mesh_tab));

    // 4. PCB & Circuit Tab
    RibbonTab pcb_tab{
        .id = "tab_pcb",
        .title = "PCB & Schematic",
        .category = RibbonCategory::PcbRouting,
        .is_contextual = true,
        .is_active = false,
        .groups = {
            RibbonGroup{
                .id = "grp_components",
                .title = "Components",
                .actions = {
                    RibbonActionItem::make("pcb.resistor", "Resistor", "resistor", "Add passives: resistor"),
                    RibbonActionItem::make("pcb.capacitor", "Capacitor", "capacitor", "Add capacitor"),
                    RibbonActionItem::make("pcb.ic", "IC Chip", "chip", "Place IC footprint")
                }
            },
            RibbonGroup{
                .id = "grp_routing",
                .title = "Routing",
                .actions = {
                    RibbonActionItem::make("pcb.wire", "Net Wire", "wire", "Connect schematic net wire"),
                    RibbonActionItem::make("pcb.route45", "45° Push", "route", "Push-and-shove interactive routing")
                }
            }
        }
    };
    tabs_.push_back(std::move(pcb_tab));

    // 5. Physics Setup Tab
    RibbonTab phys_tab{
        .id = "tab_physics",
        .title = "Physics Setup",
        .category = RibbonCategory::PhysicsSetup,
        .is_contextual = false,
        .is_active = false,
        .groups = {
            RibbonGroup{
                .id = "grp_domains",
                .title = "Domains",
                .actions = {
                    RibbonActionItem::make("phys.rigid", "Rigid Body", "rigid_body", "Add rigid dynamic body"),
                    RibbonActionItem::make("phys.thermal", "Thermal Heat", "thermal", "Attach thermal dissipation field"),
                    RibbonActionItem::make("phys.fluid", "SPH Fluid", "fluid", "Spawn particle fluid basin")
                }
            },
            RibbonGroup{
                .id = "grp_simulation",
                .title = "Simulation",
                .actions = {
                    RibbonActionItem::make("sim.run", "Run", "play", "Start simulation execution (F5)", "F5"),
                    RibbonActionItem::make("sim.pause", "Pause", "pause", "Pause simulation (F6)", "F6"),
                    RibbonActionItem::make("sim.step", "Step", "step", "Step single timestep (F10)", "F10")
                }
            }
        }
    };
    tabs_.push_back(std::move(phys_tab));

    // 6. Analysis & Results Tab
    RibbonTab analysis_tab{
        .id = "tab_analysis",
        .title = "Analysis & Results",
        .category = RibbonCategory::AnalysisResults,
        .is_contextual = false,
        .is_active = false,
        .groups = {
            RibbonGroup{
                .id = "grp_probes",
                .title = "Telemetry",
                .actions = {
                    RibbonActionItem::make("plot.2d_trace", "Time Plot", "plot", "Open 2D real-time probe trace"),
                    RibbonActionItem::make("plot.contour", "3D Field", "contour", "Field contour visualization")
                }
            }
        }
    };
    tabs_.push_back(std::move(analysis_tab));

    active_tab_id_ = "tab_home";
}

void RibbonBarModel::add_tab(RibbonTab tab) {
    tabs_.push_back(std::move(tab));
}

void RibbonBarModel::set_active_tab(std::string_view tab_id) {
    for (auto& t : tabs_) {
        t.is_active = (t.id == tab_id);
        if (t.is_active) {
            active_tab_id_ = t.id;
        }
    }
}

void RibbonBarModel::set_context_for_workspace(WorkspacePreset preset) {
    switch (preset) {
        case WorkspacePreset::Modeling:
            set_active_tab("tab_mesh");
            break;
        case WorkspacePreset::Schematic:
            set_active_tab("tab_pcb");
            break;
        case WorkspacePreset::Simulation:
            set_active_tab("tab_physics");
            break;
        case WorkspacePreset::Results:
            set_active_tab("tab_analysis");
            break;
        case WorkspacePreset::Scripting:
            set_active_tab("tab_home");
            break;
    }
}

void RibbonBarModel::set_context_for_selection(std::string_view entity_type) {
    if (entity_type == "2D_Profile" || entity_type == "Sketch") {
        set_active_tab("tab_sketch");
    } else if (entity_type == "3D_Solid" || entity_type == "Mesh") {
        set_active_tab("tab_mesh");
    } else if (entity_type == "PCB_Component" || entity_type == "NetWire") {
        set_active_tab("tab_pcb");
    } else if (entity_type == "Physics_Body" || entity_type == "Constraint") {
        set_active_tab("tab_physics");
    }
}

std::optional<RibbonActionItem> RibbonBarModel::find_action(std::string_view action_id) const {
    for (const auto& tab : tabs_) {
        for (const auto& grp : tab.groups) {
            for (const auto& act : grp.actions) {
                if (act.id == action_id) return act;
            }
        }
    }
    return std::nullopt;
}

// =============================================================================
// CommandPaletteRegistry Implementation
// =============================================================================

void CommandPaletteRegistry::register_action(CommandPaletteAction action) {
    actions_.push_back(std::move(action));
}

std::vector<CommandPaletteAction> CommandPaletteRegistry::search(std::string_view query) const {
    if (query.empty()) return actions_;
    std::vector<CommandPaletteAction> results;
    std::string q(query);
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    for (const auto& act : actions_) {
        std::string title = act.title;
        std::transform(title.begin(), title.end(), title.begin(), ::tolower);
        std::string cat = act.category;
        std::transform(cat.begin(), cat.end(), cat.begin(), ::tolower);

        if (title.find(q) != std::string::npos || cat.find(q) != std::string::npos) {
            results.push_back(act);
        }
    }
    return results;
}

std::vector<CommandSearchResult> CommandPaletteRegistry::fuzzy_search(std::string_view query) const {
    std::vector<CommandSearchResult> results;
    if (query.empty()) {
        results.reserve(actions_.size());
        for (const auto& act : actions_) {
            results.push_back({act, 0});
        }
        return results;
    }

    for (const auto& act : actions_) {
        int best_score = 0;
        auto res_title = base::FuzzyMatcher::match(query, act.title);
        if (res_title.matches && res_title.score > best_score) {
            best_score = res_title.score;
        }

        auto res_cat = base::FuzzyMatcher::match(query, act.category);
        if (res_cat.matches && (res_cat.score + 5) > best_score) {
            best_score = res_cat.score + 5;
        }

        auto res_short = base::FuzzyMatcher::match(query, act.shortcut);
        if (res_short.matches && (res_short.score + 10) > best_score) {
            best_score = res_short.score + 10;
        }

        for (const auto& kw : act.keywords) {
            auto res_kw = base::FuzzyMatcher::match(query, kw);
            if (res_kw.matches && res_kw.score > best_score) {
                best_score = res_kw.score;
            }
        }

        if (best_score > 0) {
            results.push_back({act, best_score});
        }
    }

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });

    return results;
}

bool CommandPaletteRegistry::execute_action(std::string_view action_id) {
    for (const auto& act : actions_) {
        if (act.action_id == action_id) {
            if (act.is_enabled && act.callback) {
                act.callback();
                return true;
            }
            return false;
        }
    }
    return false;
}

WorkspaceShell::WorkspaceShell(WorkspacePreset initial_preset) {
    apply_preset(initial_preset);

    // Register standard actions
    command_palette_.register_action(CommandPaletteAction::make("sim.run", "Simulation", "Run Simulation", "F5", {"start", "execute"}));
    command_palette_.register_action(CommandPaletteAction::make("sim.pause", "Simulation", "Pause Simulation", "F6", {"halt", "freeze"}));
    command_palette_.register_action(CommandPaletteAction::make("view.preset.schematic", "View", "Switch to Schematic View", "Ctrl+2", {"circuit", "diagram"}, "", true, [this]() { apply_preset(WorkspacePreset::Schematic); }));
}

void WorkspaceShell::apply_preset(WorkspacePreset preset) {
    current_preset_ = preset;
    panels_.clear();
    docking_manager_.apply_preset(preset);
    ribbon_.set_context_for_workspace(preset);

    switch (preset) {
        case WorkspacePreset::Modeling:
            panels_ = {
                DockPanelState{.panel_id = "project_browser", .title = "Project Browser", .is_visible = true, .area = DockArea::Left},
                DockPanelState{.panel_id = "viewport_3d", .title = "3D Viewport", .is_visible = true, .area = DockArea::Center},
                DockPanelState{.panel_id = "property_inspector", .title = "Property Inspector", .is_visible = true, .area = DockArea::Right},
                DockPanelState{.panel_id = "simulation_toolbar", .title = "Simulation Toolbar", .is_visible = true, .area = DockArea::Bottom}
            };
            break;
        case WorkspacePreset::Schematic:
            panels_ = {
                DockPanelState{.panel_id = "project_browser", .title = "Project Browser", .is_visible = true, .area = DockArea::Left},
                DockPanelState{.panel_id = "schematic_editor", .title = "Schematic Canvas", .is_visible = true, .area = DockArea::Center},
                DockPanelState{.panel_id = "property_inspector", .title = "Property Inspector", .is_visible = true, .area = DockArea::Right}
            };
            break;
        case WorkspacePreset::Simulation:
            panels_ = {
                DockPanelState{.panel_id = "simulation_toolbar", .title = "Simulation Controls", .is_visible = true, .area = DockArea::Left},
                DockPanelState{.panel_id = "viewport_3d", .title = "3D Viewport", .is_visible = true, .area = DockArea::Center},
                DockPanelState{.panel_id = "results_workspace", .title = "Live Results", .is_visible = true, .area = DockArea::Right}
            };
            break;
        case WorkspacePreset::Results:
            panels_ = {
                DockPanelState{.panel_id = "results_workspace", .title = "Dashboards & Traces", .is_visible = true, .area = DockArea::Center},
                DockPanelState{.panel_id = "property_inspector", .title = "Plot Inspector", .is_visible = true, .area = DockArea::Right}
            };
            break;
        case WorkspacePreset::Scripting:
            panels_ = {
                DockPanelState{.panel_id = "project_browser", .title = "Project Files", .is_visible = true, .area = DockArea::Left},
                DockPanelState{.panel_id = "code_editor", .title = "Script Editor", .is_visible = true, .area = DockArea::Center}
            };
            break;
    }
}

void WorkspaceShell::set_panel_visibility(std::string_view panel_id, bool visible) {
    for (auto& p : panels_) {
        if (p.panel_id == panel_id) {
            p.is_visible = visible;
            return;
        }
    }
}

std::optional<DockPanelState> WorkspaceShell::get_panel(std::string_view panel_id) const {
    for (const auto& p : panels_) {
        if (p.panel_id == panel_id) return p;
    }
    return std::nullopt;
}

std::string WorkspaceShell::serialize_layout_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"preset\": \"" << workspace_preset_to_string(current_preset_) << "\",\n";
    ss << "  \"panels\": [\n";
    for (size_t i = 0; i < panels_.size(); ++i) {
        const auto& p = panels_[i];
        ss << "    {\"id\": \"" << p.panel_id << "\", \"visible\": "
           << (p.is_visible ? "true" : "false") << "}"
           << (i + 1 < panels_.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<void> WorkspaceShell::restore_layout_json(std::string_view json_str) {
    std::string key = "\"preset\": \"";
    auto pos = json_str.find(key);
    if (pos == std::string_view::npos) {
        return base::Result<void>::err(make_ui_error("Missing preset in layout JSON"));
    }
    pos += key.size();
    auto end = json_str.find('"', pos);
    if (end == std::string_view::npos) {
        return base::Result<void>::err(make_ui_error("Corrupted preset string in layout JSON"));
    }
    std::string preset_str(json_str.substr(pos, end - pos));
    auto p = workspace_preset_from_string(preset_str);
    if (!p) {
        return base::Result<void>::err(make_ui_error("Unknown workspace preset: " + preset_str));
    }
    apply_preset(*p);
    return base::Result<void>::ok();
}

// =============================================================================
// UI-02: Project Browser Model
// =============================================================================

std::vector<const ProjectTreeNode*> ProjectBrowserModel::search(std::string_view query) const {
    std::vector<const ProjectTreeNode*> results;
    std::string q(query);
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    recursive_search(root_, q, results);
    return results;
}

void ProjectBrowserModel::recursive_search(const ProjectTreeNode& node, std::string_view query, std::vector<const ProjectTreeNode*>& out) const {
    std::string name = node.name;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name.find(query) != std::string_view::npos) {
        out.push_back(&node);
    }
    for (const auto& child : node.children) {
        recursive_search(child, query, out);
    }
}

bool ProjectBrowserModel::select_node(std::string_view id) {
    return recursive_select(root_, id);
}

bool ProjectBrowserModel::recursive_select(ProjectTreeNode& node, std::string_view id) {
    bool found = (node.id == id);
    node.is_selected = found;
    for (auto& child : node.children) {
        if (recursive_select(child, id)) {
            found = true;
        }
    }
    return found;
}

std::optional<std::string> ProjectBrowserModel::selected_node_id() const {
    std::function<std::optional<std::string>(const ProjectTreeNode&)> find_sel =
        [&](const ProjectTreeNode& n) -> std::optional<std::string> {
            if (n.is_selected) return n.id;
            for (const auto& c : n.children) {
                auto res = find_sel(c);
                if (res) return res;
            }
            return std::nullopt;
        };
    return find_sel(root_);
}

// =============================================================================
// UI-04: Schematic Editor Model
// =============================================================================

void SchematicEditorModel::add_symbol(SchematicSymbol symbol) {
    symbols_.push_back(std::move(symbol));
}

void SchematicEditorModel::add_wire(SchematicWire wire) {
    wires_.push_back(std::move(wire));
}

std::vector<ElectricalIssue> SchematicEditorModel::run_electrical_diagnostics() const {
    std::vector<ElectricalIssue> issues;
    if (symbols_.empty()) {
        issues.push_back({"Schematic contains zero symbols", false});
    }
    // Check for dangling wires
    for (const auto& w : wires_) {
        if (w.waypoints.size() < 2) {
            issues.push_back({"Dangling wire detected (less than 2 waypoints): " + w.wire_id, true});
        }
    }
    return issues;
}

// =============================================================================
// UI-05: Graph / Flow Editor Model
// =============================================================================

void FlowEditorModel::add_block(FlowBlock block) {
    blocks_.push_back(std::move(block));
}

base::Result<void> FlowEditorModel::connect(
    std::string_view src_id, std::string_view src_sock,
    std::string_view dst_id, std::string_view dst_sock)
{
    bool src_found = false, dst_found = false;
    for (const auto& b : blocks_) {
        if (b.block_id == src_id) {
            for (const auto& s : b.sockets) {
                if (!s.is_input && s.name == src_sock) src_found = true;
            }
        }
        if (b.block_id == dst_id) {
            for (const auto& s : b.sockets) {
                if (s.is_input && s.name == dst_sock) dst_found = true;
            }
        }
    }

    if (!src_found) return base::Result<void>::err(make_ui_error("Source output socket not found"));
    if (!dst_found) return base::Result<void>::err(make_ui_error("Target input socket not found"));

    connections_.push_back(FlowConnection{
        .source_block_id = std::string(src_id),
        .source_socket = std::string(src_sock),
        .target_block_id = std::string(dst_id),
        .target_socket = std::string(dst_sock)
    });

    return base::Result<void>::ok();
}

// =============================================================================
// UI-07: Property Inspector Model
// =============================================================================

void PropertyInspectorModel::add_property(PropertyItem item) {
    properties_.push_back(std::move(item));
}

base::Result<void> PropertyInspectorModel::set_value(std::string_view name, std::string_view new_value) {
    for (auto& p : properties_) {
        if (p.name == name) {
            if (p.is_read_only) {
                return base::Result<void>::err(make_ui_error("Property '" + std::string(name) + "' is read-only"));
            }
            p.value = std::string(new_value);
            p.validation_error.clear();
            return base::Result<void>::ok();
        }
    }
    return base::Result<void>::err(make_ui_error("Property not found: " + std::string(name)));
}

void PropertyInspectorModel::reset_to_default(std::string_view name) {
    for (auto& p : properties_) {
        if (p.name == name) {
            p.value = p.default_value;
            p.validation_error.clear();
            return;
        }
    }
}

// =============================================================================
// UI-08: Simulation Toolbar Controller
// =============================================================================

void SimulationToolbarController::update_telemetry(double sim_time, uint64_t step_count, double current_rtf) noexcept {
    sim_time_ = sim_time;
    step_count_ = step_count;
    current_rtf_ = current_rtf;
}

bool SimulationToolbarController::can_run() const noexcept {
    return state_ == SimulationState::Stopped || state_ == SimulationState::Paused;
}

bool SimulationToolbarController::can_pause() const noexcept {
    return state_ == SimulationState::Running || state_ == SimulationState::Stepping;
}

bool SimulationToolbarController::can_step() const noexcept {
    return state_ == SimulationState::Paused || state_ == SimulationState::Stopped;
}

bool SimulationToolbarController::can_stop() const noexcept {
    return state_ == SimulationState::Running || state_ == SimulationState::Paused || state_ == SimulationState::Stepping;
}

// =============================================================================
// UI-10: Code Editor Controller
// =============================================================================

void CodeEditorController::toggle_breakpoint(uint32_t line) {
    auto it = std::find(breakpoints_.begin(), breakpoints_.end(), line);
    if (it != breakpoints_.end()) {
        breakpoints_.erase(it);
    } else {
        breakpoints_.push_back(line);
    }
}

bool CodeEditorController::has_breakpoint(uint32_t line) const {
    return std::find(breakpoints_.begin(), breakpoints_.end(), line) != breakpoints_.end();
}

// =============================================================================
// UI-11: Library Browser Model
// =============================================================================

std::vector<const LibraryPackageCard*> LibraryBrowserModel::filter_by_category(std::string_view cat) const {
    std::vector<const LibraryPackageCard*> res;
    for (const auto& p : packages_) {
        if (p.category == cat) res.push_back(&p);
    }
    return res;
}

std::vector<const LibraryPackageCard*> LibraryBrowserModel::search(std::string_view query) const {
    std::vector<const LibraryPackageCard*> res;
    std::string q(query);
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    for (const auto& p : packages_) {
        std::string n = p.package_name;
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        if (n.find(q) != std::string::npos) {
            res.push_back(&p);
        }
    }
    return res;
}

// =============================================================================
// UI-12: Accessibility and Usability Manager
// =============================================================================

base::Result<void> AccessibilityManager::set_ui_scale_factor(double scale) {
    if (scale <= 0.0 || scale > 4.0) {
        return base::Result<void>::err(make_ui_error("UI scale factor must be between 0.1 and 4.0"));
    }
    settings_.ui_scale_factor = scale;
    return base::Result<void>::ok();
}

// =============================================================================
// UI-01-003: Desktop Menu Bar & File Management Architecture
// =============================================================================

std::optional<MenuCategory> menu_category_from_string(std::string_view str) noexcept {
    if (str == "File") return MenuCategory::File;
    if (str == "Edit") return MenuCategory::Edit;
    if (str == "View") return MenuCategory::View;
    if (str == "Insert") return MenuCategory::Insert;
    if (str == "Simulation") return MenuCategory::Simulation;
    if (str == "Tools") return MenuCategory::Tools;
    if (str == "Window") return MenuCategory::Window;
    if (str == "Help") return MenuCategory::Help;
    return std::nullopt;
}

RecentProjectsManager::RecentProjectsManager(size_t max_entries)
    : max_entries_(max_entries) {}

void RecentProjectsManager::add_recent_project(std::string path, std::string title, std::string timestamp_iso) {
    if (path.empty()) return;

    // Remove if already present (MRU promotes to front)
    auto it = std::remove_if(entries_.begin(), entries_.end(),
        [&path](const RecentProjectEntry& e) { return e.path == path; });
    entries_.erase(it, entries_.end());

    RecentProjectEntry new_entry{
        .path = std::move(path),
        .title = title.empty() ? "Untitled Project" : std::move(title),
        .last_opened_iso = timestamp_iso.empty() ? "2026-11-07T00:00:00Z" : std::move(timestamp_iso),
        .thumbnail_hint = "",
        .is_pinned = false
    };

    entries_.insert(entries_.begin(), std::move(new_entry));

    // Keep pinned items and trim unpinned to max_entries_
    if (entries_.size() > max_entries_) {
        // Find last unpinned and remove
        for (auto rit = entries_.rbegin(); rit != entries_.rend(); ++rit) {
            if (!rit->is_pinned) {
                entries_.erase(std::next(rit).base());
                break;
            }
        }
    }
}

void RecentProjectsManager::remove_recent_project(std::string_view path) {
    auto it = std::remove_if(entries_.begin(), entries_.end(),
        [path](const RecentProjectEntry& e) { return e.path == path; });
    entries_.erase(it, entries_.end());
}

void RecentProjectsManager::clear() noexcept {
    entries_.clear();
}

void RecentProjectsManager::pin_project(std::string_view path, bool pinned) {
    for (auto& entry : entries_) {
        if (entry.path == path) {
            entry.is_pinned = pinned;
            break;
        }
    }
}

std::string RecentProjectsManager::serialize_json() const {
    std::ostringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        ss << "  {\n"
           << "    \"path\": \"" << e.path << "\",\n"
           << "    \"title\": \"" << e.title << "\",\n"
           << "    \"last_opened\": \"" << e.last_opened_iso << "\",\n"
           << "    \"is_pinned\": " << (e.is_pinned ? "true" : "false") << "\n"
           << "  }" << (i + 1 < entries_.size() ? "," : "") << "\n";
    }
    ss << "]\n";
    return ss.str();
}

base::Result<void> RecentProjectsManager::restore_json(std::string_view json_str) {
    if (json_str.empty()) {
        return base::Result<void>::err(make_ui_error("Empty JSON for recent projects"));
    }

    entries_.clear();
    // Simplified parsing for JSON array of objects
    size_t pos = 0;
    while ((pos = json_str.find('{', pos)) != std::string_view::npos) {
        size_t end_obj = json_str.find('}', pos);
        if (end_obj == std::string_view::npos) break;

        std::string_view obj = json_str.substr(pos, end_obj - pos + 1);
        RecentProjectEntry entry;

        auto extract_val = [obj](std::string_view key) -> std::string {
            size_t kpos = obj.find(key);
            if (kpos == std::string_view::npos) return "";
            size_t vstart = obj.find('"', kpos + key.size());
            if (vstart == std::string_view::npos) return "";
            size_t vend = obj.find('"', vstart + 1);
            if (vend == std::string_view::npos) return "";
            return std::string(obj.substr(vstart + 1, vend - vstart - 1));
        };

        entry.path = extract_val("\"path\"");
        entry.title = extract_val("\"title\"");
        entry.last_opened_iso = extract_val("\"last_opened\"");
        entry.is_pinned = (obj.find("\"is_pinned\": true") != std::string_view::npos);

        if (!entry.path.empty()) {
            entries_.push_back(std::move(entry));
        }

        pos = end_obj + 1;
    }

    return base::Result<void>::ok();
}

ProjectTemplateRegistry::ProjectTemplateRegistry() {
    register_template(ProjectTemplateDescriptor{
        .template_id = "template.multiphysics",
        .name = "Coupled Multiphysics Rig",
        .category = "Multiphysics",
        .description = "Coupled Electro-Thermo-Mechanical rig with active telemetry probes and transient solver setup.",
        .default_preset = WorkspacePreset::Simulation,
        .tags = {"multiphysics", "thermal", "mechanics", "circuits"}
    });

    register_template(ProjectTemplateDescriptor{
        .template_id = "template.pcb",
        .name = "Electronic Circuit & PCB Layout",
        .category = "Electronics",
        .description = "Schematic capture, SPICE simulation bench, and multi-layer PCB routing template.",
        .default_preset = WorkspacePreset::Schematic,
        .tags = {"pcb", "schematic", "spice", "digital"}
    });

    register_template(ProjectTemplateDescriptor{
        .template_id = "template.cad3d",
        .name = "Mechanical 3D Assembly",
        .category = "Mechanics",
        .description = "Parametric 3D solid modeling workspace with B-Rep feature tree and rigid kinematics.",
        .default_preset = WorkspacePreset::Modeling,
        .tags = {"cad", "3d", "mechanics", "assembly"}
    });

    register_template(ProjectTemplateDescriptor{
        .template_id = "template.chemistry",
        .name = "Chemical Reaction Reactor",
        .category = "Chemistry",
        .description = "Batch/CSTR reactor model with Arrhenius reaction kinetics and thermodynamic equilibrium.",
        .default_preset = WorkspacePreset::Simulation,
        .tags = {"chemistry", "kinetics", "reactions", "thermodynamics"}
    });

    register_template(ProjectTemplateDescriptor{
        .template_id = "template.blank",
        .name = "Blank Project",
        .category = "General",
        .description = "Clean empty scene graph with default SI unit configuration.",
        .default_preset = WorkspacePreset::Modeling,
        .tags = {"blank", "empty", "custom"}
    });
}

void ProjectTemplateRegistry::register_template(ProjectTemplateDescriptor desc) {
    templates_.push_back(std::move(desc));
}

std::optional<ProjectTemplateDescriptor> ProjectTemplateRegistry::get_template(std::string_view id) const {
    for (const auto& t : templates_) {
        if (t.template_id == id) return t;
    }
    return std::nullopt;
}

std::vector<ProjectTemplateDescriptor> ProjectTemplateRegistry::get_templates_by_category(std::string_view category) const {
    std::vector<ProjectTemplateDescriptor> matches;
    for (const auto& t : templates_) {
        if (t.category == category) matches.push_back(t);
    }
    return matches;
}

ApplicationMenuBarModel::ApplicationMenuBarModel() {
    initialize_default_menu_bar();
}

void ApplicationMenuBarModel::initialize_default_menu_bar() {
    menu_map_.clear();

    // -------------------------------------------------------------------------
    // FILE MENU
    // -------------------------------------------------------------------------
    menu_map_[MenuCategory::File] = {
        MenuItemDescriptor::make_action("file.new", "New Project...", MenuCategory::File, "Ctrl+N", "N"),
        MenuItemDescriptor::make_action("file.open", "Open Project...", MenuCategory::File, "Ctrl+O", "O"),
        MenuItemDescriptor::make_submenu("file.recent", "Recent Projects", MenuCategory::File, {}),
        MenuItemDescriptor::make_separator("file.sep1"),
        MenuItemDescriptor::make_action("file.save", "Save", MenuCategory::File, "Ctrl+S", "S"),
        MenuItemDescriptor::make_action("file.save_as", "Save As...", MenuCategory::File, "Ctrl+Shift+S", "A"),
        MenuItemDescriptor::make_separator("file.sep2"),
        MenuItemDescriptor::make_submenu("file.import", "Import", MenuCategory::File, {
            MenuItemDescriptor::make_action("file.import.dxf", "AutoCAD DXF (*.dxf)", MenuCategory::File),
            MenuItemDescriptor::make_action("file.import.svg", "Scalable Vector Graphics (*.svg)", MenuCategory::File),
            MenuItemDescriptor::make_action("file.import.step", "STEP 3D CAD (*.step, *.stp)", MenuCategory::File),
            MenuItemDescriptor::make_action("file.import.netlist", "SPICE Netlist (*.cir, *.net)", MenuCategory::File)
        }),
        MenuItemDescriptor::make_submenu("file.export", "Export", MenuCategory::File, {
            MenuItemDescriptor::make_action("file.export.step", "STEP Solid Model (*.step)", MenuCategory::File),
            MenuItemDescriptor::make_action("file.export.gerber", "Gerber RS-274X Package (*.zip)", MenuCategory::File),
            MenuItemDescriptor::make_action("file.export.report", "Engineering Validation Report (*.pdf)", MenuCategory::File),
            MenuItemDescriptor::make_action("file.export.csv", "Simulation Telemetry Data (*.csv)", MenuCategory::File)
        }),
        MenuItemDescriptor::make_separator("file.sep3"),
        MenuItemDescriptor::make_action("file.close", "Close Project", MenuCategory::File, "Ctrl+W", "C"),
        MenuItemDescriptor::make_action("file.exit", "Exit", MenuCategory::File, "Ctrl+Q", "X")
    };

    // -------------------------------------------------------------------------
    // EDIT MENU
    // -------------------------------------------------------------------------
    menu_map_[MenuCategory::Edit] = {
        MenuItemDescriptor::make_action("edit.undo", "Undo", MenuCategory::Edit, "Ctrl+Z"),
        MenuItemDescriptor::make_action("edit.redo", "Redo", MenuCategory::Edit, "Ctrl+Y"),
        MenuItemDescriptor::make_separator("edit.sep1"),
        MenuItemDescriptor::make_action("edit.cut", "Cut", MenuCategory::Edit, "Ctrl+X"),
        MenuItemDescriptor::make_action("edit.copy", "Copy", MenuCategory::Edit, "Ctrl+C"),
        MenuItemDescriptor::make_action("edit.paste", "Paste", MenuCategory::Edit, "Ctrl+V"),
        MenuItemDescriptor::make_separator("edit.sep2"),
        MenuItemDescriptor::make_action("edit.preferences", "Preferences...", MenuCategory::Edit, "Ctrl+,")
    };

    // -------------------------------------------------------------------------
    // VIEW MENU
    // -------------------------------------------------------------------------
    menu_map_[MenuCategory::View] = {
        MenuItemDescriptor::make_action("view.reset_layout", "Reset to Default Layout", MenuCategory::View),
        MenuItemDescriptor::make_action("view.toggle_fullscreen", "Toggle Full Screen", MenuCategory::View, "F11")
    };

    // -------------------------------------------------------------------------
    // SIMULATION MENU
    // -------------------------------------------------------------------------
    menu_map_[MenuCategory::Simulation] = {
        MenuItemDescriptor::make_action("sim.run", "Start Simulation", MenuCategory::Simulation, "F5"),
        MenuItemDescriptor::make_action("sim.step", "Step Forward", MenuCategory::Simulation, "F10"),
        MenuItemDescriptor::make_action("sim.pause", "Pause", MenuCategory::Simulation, "F6"),
        MenuItemDescriptor::make_action("sim.stop", "Stop", MenuCategory::Simulation, "Shift+F5"),
        MenuItemDescriptor::make_action("sim.reset", "Reset Simulation Clock", MenuCategory::Simulation)
    };

    // -------------------------------------------------------------------------
    // HELP MENU
    // -------------------------------------------------------------------------
    menu_map_[MenuCategory::Help] = {
        MenuItemDescriptor::make_action("help.docs", "Shunya Documentation", MenuCategory::Help, "F1"),
        MenuItemDescriptor::make_action("help.about", "About Shunya Studio...", MenuCategory::Help)
    };
}

void ApplicationMenuBarModel::add_item(MenuItemDescriptor item) {
    menu_map_[item.category].push_back(std::move(item));
}

const std::vector<MenuItemDescriptor>& ApplicationMenuBarModel::items_for_category(MenuCategory cat) const {
    static const std::vector<MenuItemDescriptor> empty;
    auto it = menu_map_.find(cat);
    return (it != menu_map_.end()) ? it->second : empty;
}

MenuItemDescriptor* ApplicationMenuBarModel::find_item(std::string_view id) {
    for (auto& [cat, list] : menu_map_) {
        if (auto* found = recursive_find(list, id)) {
            return found;
        }
    }
    return nullptr;
}

const MenuItemDescriptor* ApplicationMenuBarModel::find_item(std::string_view id) const {
    for (const auto& [cat, list] : menu_map_) {
        if (const auto* found = recursive_find(list, id)) {
            return found;
        }
    }
    return nullptr;
}

MenuItemDescriptor* ApplicationMenuBarModel::recursive_find(std::vector<MenuItemDescriptor>& items, std::string_view id) {
    for (auto& item : items) {
        if (item.id == id) return &item;
        if (!item.sub_items.empty()) {
            if (auto* sub = recursive_find(item.sub_items, id)) {
                return sub;
            }
        }
    }
    return nullptr;
}

const MenuItemDescriptor* ApplicationMenuBarModel::recursive_find(const std::vector<MenuItemDescriptor>& items, std::string_view id) const {
    for (const auto& item : items) {
        if (item.id == id) return &item;
        if (!item.sub_items.empty()) {
            if (const auto* sub = recursive_find(item.sub_items, id)) {
                return sub;
            }
        }
    }
    return nullptr;
}

bool ApplicationMenuBarModel::execute_item(std::string_view id) {
    auto* item = find_item(id);
    if (!item || !item->is_enabled) return false;
    if (item->callback) {
        item->callback();
        return true;
    }
    return false;
}

} // namespace shunya::core::domain
