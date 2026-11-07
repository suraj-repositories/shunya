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
