#include "shunya/core/domain/workspace_ui.hpp"

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

std::optional<WorkspacePreset> workspace_preset_from_string(std::string_view s) noexcept {
    if (s == "Modeling") return WorkspacePreset::Modeling;
    if (s == "Schematic") return WorkspacePreset::Schematic;
    if (s == "Simulation") return WorkspacePreset::Simulation;
    if (s == "Results") return WorkspacePreset::Results;
    if (s == "Scripting") return WorkspacePreset::Scripting;
    return std::nullopt;
}

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
    command_palette_.register_action(CommandPaletteAction{
        .action_id = "sim.run",
        .category = "Simulation",
        .title = "Run Simulation",
        .shortcut = "F5",
        .is_enabled = true,
        .callback = nullptr
    });

    command_palette_.register_action(CommandPaletteAction{
        .action_id = "sim.pause",
        .category = "Simulation",
        .title = "Pause Simulation",
        .shortcut = "F6",
        .is_enabled = true,
        .callback = nullptr
    });

    command_palette_.register_action(CommandPaletteAction{
        .action_id = "view.preset.schematic",
        .category = "View",
        .title = "Switch to Schematic View",
        .shortcut = "Ctrl+2",
        .is_enabled = true,
        .callback = [this]() { apply_preset(WorkspacePreset::Schematic); }
    });
}

void WorkspaceShell::apply_preset(WorkspacePreset preset) {
    current_preset_ = preset;
    panels_.clear();

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

} // namespace shunya::core::domain
