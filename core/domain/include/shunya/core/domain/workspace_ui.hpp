#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/domain/simulation_runtime.hpp"
#include "shunya/core/domain/simulation_execution.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// UI-01: Workspace Shell & Docking Layout
// =============================================================================

enum class WorkspacePreset {
    Modeling,
    Schematic,
    Simulation,
    Results,
    Scripting
};

[[nodiscard]] constexpr std::string_view workspace_preset_to_string(WorkspacePreset p) noexcept {
    switch (p) {
        case WorkspacePreset::Modeling: return "Modeling";
        case WorkspacePreset::Schematic: return "Schematic";
        case WorkspacePreset::Simulation: return "Simulation";
        case WorkspacePreset::Results: return "Results";
        case WorkspacePreset::Scripting: return "Scripting";
    }
    return "Modeling";
}

[[nodiscard]] std::optional<WorkspacePreset> workspace_preset_from_string(std::string_view s) noexcept;

enum class DockArea {
    Left,
    Right,
    Center,
    Bottom
};

struct DockPanelState {
    std::string panel_id;
    std::string title;
    bool is_visible{true};
    DockArea area{DockArea::Center};
    bool is_floating{false};
    double width_fraction{0.25};
    double height_fraction{0.25};
};

// =============================================================================
// UI-01-001: Multi-pane Docking Framework & Layout Hierarchy
// =============================================================================

enum class DockNodeType {
    Leaf,
    SplitHorizontal,
    SplitVertical,
    Tabbed
};

enum class DockSnapZone {
    None,
    North,
    South,
    East,
    West,
    CenterTab
};

[[nodiscard]] constexpr std::string_view dock_node_type_to_string(DockNodeType t) noexcept {
    switch (t) {
        case DockNodeType::Leaf: return "Leaf";
        case DockNodeType::SplitHorizontal: return "SplitHorizontal";
        case DockNodeType::SplitVertical: return "SplitVertical";
        case DockNodeType::Tabbed: return "Tabbed";
    }
    return "Leaf";
}

[[nodiscard]] constexpr std::string_view dock_snap_zone_to_string(DockSnapZone z) noexcept {
    switch (z) {
        case DockSnapZone::None: return "None";
        case DockSnapZone::North: return "North";
        case DockSnapZone::South: return "South";
        case DockSnapZone::East: return "East";
        case DockSnapZone::West: return "West";
        case DockSnapZone::CenterTab: return "CenterTab";
    }
    return "None";
}

struct DockNode : public std::enable_shared_from_this<DockNode> {
    std::string node_id;
    DockNodeType type{DockNodeType::Leaf};

    // Leaf node attributes
    std::string panel_id;
    std::string title;
    bool is_visible{true};
    bool is_closable{true};
    bool is_floating{false};
    double min_width{160.0};
    double min_height{120.0};

    // Split attributes (0.0 to 1.0 normalized proportion)
    double split_ratio{0.5};
    std::shared_ptr<DockNode> first_child;
    std::shared_ptr<DockNode> second_child;

    // Tabbed attributes
    std::vector<std::shared_ptr<DockNode>> tabs;
    size_t active_tab_index{0};

    static std::shared_ptr<DockNode> make_leaf(std::string node_id, std::string panel_id, std::string title, bool is_visible = true);
    static std::shared_ptr<DockNode> make_split(std::string node_id, DockNodeType split_type, double ratio, std::shared_ptr<DockNode> first, std::shared_ptr<DockNode> second);
    static std::shared_ptr<DockNode> make_tabbed(std::string node_id, std::vector<std::shared_ptr<DockNode>> tabs, size_t active_idx = 0);

    [[nodiscard]] std::shared_ptr<DockNode> clone() const;
    [[nodiscard]] std::shared_ptr<DockNode> find_node(std::string_view id) const;
    [[nodiscard]] std::shared_ptr<DockNode> find_panel(std::string_view pid) const;
    void collect_leaves(std::vector<std::shared_ptr<DockNode>>& out) const;
    [[nodiscard]] std::string to_json(int indent = 2) const;
    static std::shared_ptr<DockNode> from_json_str(std::string_view json_str);
};

class DockingManager {
public:
    DockingManager();

    void set_root(std::shared_ptr<DockNode> root);
    [[nodiscard]] std::shared_ptr<DockNode> root() const noexcept { return root_; }

    [[nodiscard]] std::shared_ptr<DockNode> find_node(std::string_view node_id) const;
    [[nodiscard]] std::shared_ptr<DockNode> find_panel(std::string_view panel_id) const;

    bool dock_panel(std::string_view panel_id, std::string_view title, std::string_view target_node_id, DockSnapZone snap_zone);
    bool remove_node(std::string_view node_id);
    bool toggle_panel_visibility(std::string_view panel_id, bool visible);
    bool set_split_ratio(std::string_view split_node_id, double ratio);
    bool set_active_tab(std::string_view tab_node_id, size_t index);
    bool set_floating(std::string_view panel_id, bool floating);

    [[nodiscard]] std::vector<std::shared_ptr<DockNode>> get_all_leaves() const;

    [[nodiscard]] std::string serialize_layout_json() const;
    base::Result<void> restore_layout_json(std::string_view json_str);

    void apply_preset(WorkspacePreset preset);

private:
    std::shared_ptr<DockNode> root_;
    bool replace_child_in_tree(std::shared_ptr<DockNode> current, std::shared_ptr<DockNode> target, std::shared_ptr<DockNode> replacement);
    bool remove_child_from_tree(std::shared_ptr<DockNode> current, std::string_view node_id);
};

// UI-01-001-03: Hotkey & Action manager for panel toggles & window management
struct DockActionShortcut {
    std::string action_id;
    std::string key_sequence;
    std::string target_panel_id;
    std::string description;
    bool is_global{true};
};

class DockActionManager {
public:
    DockActionManager();
    void register_shortcut(DockActionShortcut shortcut);
    [[nodiscard]] const std::vector<DockActionShortcut>& shortcuts() const noexcept { return shortcuts_; }
    [[nodiscard]] std::optional<DockActionShortcut> find_by_key(std::string_view key_seq) const;
    [[nodiscard]] std::optional<DockActionShortcut> find_by_action(std::string_view action_id) const;
    bool execute_shortcut(std::string_view key_seq, DockingManager& manager);

private:
    std::vector<DockActionShortcut> shortcuts_;
};

// =============================================================================
// UI-01-002: Contextual Ribbon, Quick Access Toolbar & Fuzzy Command Palette
// =============================================================================

enum class RibbonCategory {
    General,
    Sketch2D,
    Mesh3D,
    PcbRouting,
    PhysicsSetup,
    AnalysisResults
};

[[nodiscard]] constexpr std::string_view ribbon_category_to_string(RibbonCategory c) noexcept {
    switch (c) {
        case RibbonCategory::General: return "General";
        case RibbonCategory::Sketch2D: return "2D Sketch";
        case RibbonCategory::Mesh3D: return "3D Mesh";
        case RibbonCategory::PcbRouting: return "PCB Routing";
        case RibbonCategory::PhysicsSetup: return "Physics Setup";
        case RibbonCategory::AnalysisResults: return "Analysis";
    }
    return "General";
}

enum class RibbonItemSize {
    Large,
    Small
};

struct RibbonActionItem {
    std::string id;
    std::string label;
    std::string icon_name;
    std::string tooltip;
    std::string shortcut;
    RibbonItemSize size{RibbonItemSize::Large};
    bool is_enabled{true};
    bool is_checked{false};

    static RibbonActionItem make(std::string id, std::string label, std::string icon_name, std::string tooltip, std::string shortcut = "", RibbonItemSize size = RibbonItemSize::Large, bool is_enabled = true, bool is_checked = false) {
        RibbonActionItem item;
        item.id = std::move(id);
        item.label = std::move(label);
        item.icon_name = std::move(icon_name);
        item.tooltip = std::move(tooltip);
        item.shortcut = std::move(shortcut);
        item.size = size;
        item.is_enabled = is_enabled;
        item.is_checked = is_checked;
        return item;
    }
};

struct RibbonGroup {
    std::string id;
    std::string title;
    std::vector<RibbonActionItem> actions;
};

struct RibbonTab {
    std::string id;
    std::string title;
    RibbonCategory category{RibbonCategory::General};
    bool is_contextual{false};
    bool is_active{false};
    std::vector<RibbonGroup> groups;
};

class QuickAccessToolbarModel {
public:
    QuickAccessToolbarModel();
    void pin_action(std::string action_id);
    void unpin_action(std::string_view action_id);
    [[nodiscard]] bool is_pinned(std::string_view action_id) const;
    [[nodiscard]] const std::vector<std::string>& pinned_action_ids() const noexcept { return pinned_ids_; }

private:
    std::vector<std::string> pinned_ids_;
};

class RibbonBarModel {
public:
    RibbonBarModel();

    void add_tab(RibbonTab tab);
    [[nodiscard]] const std::vector<RibbonTab>& tabs() const noexcept { return tabs_; }
    [[nodiscard]] std::string active_tab_id() const noexcept { return active_tab_id_; }
    void set_active_tab(std::string_view tab_id);

    [[nodiscard]] bool is_collapsed() const noexcept { return is_collapsed_; }
    void set_collapsed(bool collapsed) noexcept { is_collapsed_ = collapsed; }
    void toggle_collapsed() noexcept { is_collapsed_ = !is_collapsed_; }

    void set_context_for_workspace(WorkspacePreset preset);
    void set_context_for_selection(std::string_view entity_type);

    [[nodiscard]] std::optional<RibbonActionItem> find_action(std::string_view action_id) const;
    QuickAccessToolbarModel& quick_access() noexcept { return quick_access_; }
    [[nodiscard]] const QuickAccessToolbarModel& quick_access() const noexcept { return quick_access_; }

private:
    std::vector<RibbonTab> tabs_;
    std::string active_tab_id_;
    bool is_collapsed_{false};
    QuickAccessToolbarModel quick_access_;
    void setup_default_ribbon();
};

struct CommandPaletteAction {
    std::string action_id;
    std::string category;
    std::string title;
    std::string shortcut;
    std::vector<std::string> keywords;
    std::string icon_name;
    bool is_enabled{true};
    std::function<void()> callback;

    static CommandPaletteAction make(std::string action_id, std::string category, std::string title, std::string shortcut = "", std::vector<std::string> keywords = {}, std::string icon_name = "", bool is_enabled = true, std::function<void()> callback = nullptr) {
        CommandPaletteAction act;
        act.action_id = std::move(action_id);
        act.category = std::move(category);
        act.title = std::move(title);
        act.shortcut = std::move(shortcut);
        act.keywords = std::move(keywords);
        act.icon_name = std::move(icon_name);
        act.is_enabled = is_enabled;
        act.callback = std::move(callback);
        return act;
    }
};

struct CommandSearchResult {
    CommandPaletteAction action;
    int score{0};
};

class CommandPaletteRegistry {
public:
    void register_action(CommandPaletteAction action);
    [[nodiscard]] std::vector<CommandPaletteAction> search(std::string_view query) const;
    [[nodiscard]] std::vector<CommandSearchResult> fuzzy_search(std::string_view query) const;
    [[nodiscard]] bool execute_action(std::string_view action_id);
    [[nodiscard]] const std::vector<CommandPaletteAction>& actions() const noexcept { return actions_; }

private:
    std::vector<CommandPaletteAction> actions_;
};

class WorkspaceShell {
public:
    explicit WorkspaceShell(WorkspacePreset initial_preset = WorkspacePreset::Modeling);

    void apply_preset(WorkspacePreset preset);
    [[nodiscard]] WorkspacePreset current_preset() const noexcept { return current_preset_; }

    void set_panel_visibility(std::string_view panel_id, bool visible);
    [[nodiscard]] std::optional<DockPanelState> get_panel(std::string_view panel_id) const;
    [[nodiscard]] const std::vector<DockPanelState>& panels() const noexcept { return panels_; }

    [[nodiscard]] std::string serialize_layout_json() const;
    base::Result<void> restore_layout_json(std::string_view json_str);

    CommandPaletteRegistry& command_palette() noexcept { return command_palette_; }
    [[nodiscard]] const CommandPaletteRegistry& command_palette() const noexcept { return command_palette_; }

    DockingManager& docking_manager() noexcept { return docking_manager_; }
    [[nodiscard]] const DockingManager& docking_manager() const noexcept { return docking_manager_; }

    DockActionManager& action_manager() noexcept { return action_manager_; }
    [[nodiscard]] const DockActionManager& action_manager() const noexcept { return action_manager_; }

    RibbonBarModel& ribbon() noexcept { return ribbon_; }
    [[nodiscard]] const RibbonBarModel& ribbon() const noexcept { return ribbon_; }

private:
    WorkspacePreset current_preset_{WorkspacePreset::Modeling};
    std::vector<DockPanelState> panels_;
    CommandPaletteRegistry command_palette_;
    DockingManager docking_manager_;
    DockActionManager action_manager_;
    RibbonBarModel ribbon_;
};

// =============================================================================
// UI-02: Project Browser Model
// =============================================================================

enum class ProjectTreeItemType {
    ProjectRoot,
    SceneNode,
    Component,
    Experiment,
    RunResult,
    Asset,
    Script
};

struct ProjectTreeNode {
    std::string id;
    std::string name;
    ProjectTreeItemType type{ProjectTreeItemType::Asset};
    std::vector<std::string> tags;
    bool is_expanded{false};
    bool is_selected{false};
    std::vector<ProjectTreeNode> children;
};

class ProjectBrowserModel {
public:
    void set_root(ProjectTreeNode root) { root_ = std::move(root); }
    [[nodiscard]] const ProjectTreeNode& root() const noexcept { return root_; }

    [[nodiscard]] std::vector<const ProjectTreeNode*> search(std::string_view query) const;
    bool select_node(std::string_view id);
    [[nodiscard]] std::optional<std::string> selected_node_id() const;

private:
    ProjectTreeNode root_;
    void recursive_search(const ProjectTreeNode& node, std::string_view query, std::vector<const ProjectTreeNode*>& out) const;
    bool recursive_select(ProjectTreeNode& node, std::string_view id);
};

// =============================================================================
// UI-03: Viewport Controller (2D/3D)
// =============================================================================

enum class WorkspaceCameraMode {
    Perspective,
    Orthographic
};

enum class WorkspaceGizmoMode {
    None,
    Translate,
    Rotate,
    Scale
};

struct WorkspaceViewportCamera {
    WorkspaceCameraMode mode{WorkspaceCameraMode::Perspective};
    base::math::Vector3<double> position{0.0, 5.0, 10.0};
    base::math::Vector3<double> target{0.0, 0.0, 0.0};
    double fov_degrees{60.0};
    double near_clip{0.1};
    double far_clip{1000.0};
    double ortho_width{10.0};
};

struct RenderDiagnostics {
    double frames_per_second{60.0};
    uint32_t draw_calls{12};
    uint64_t triangle_count{5400};
    bool overlay_visible{true};
};

class WorkspaceViewportController {
public:
    void set_camera_mode(WorkspaceCameraMode mode) { camera_.mode = mode; }
    [[nodiscard]] const WorkspaceViewportCamera& camera() const noexcept { return camera_; }
    void set_gizmo(WorkspaceGizmoMode gizmo) noexcept { gizmo_ = gizmo; }
    [[nodiscard]] WorkspaceGizmoMode gizmo() const noexcept { return gizmo_; }

    void set_grid_visible(bool visible) noexcept { grid_visible_ = visible; }
    [[nodiscard]] bool grid_visible() const noexcept { return grid_visible_; }
    void set_grid_spacing(double spacing) noexcept { grid_spacing_ = spacing; }
    [[nodiscard]] double grid_spacing() const noexcept { return grid_spacing_; }

    [[nodiscard]] RenderDiagnostics& diagnostics() noexcept { return diagnostics_; }
    [[nodiscard]] const RenderDiagnostics& diagnostics() const noexcept { return diagnostics_; }

private:
    WorkspaceViewportCamera camera_{};
    WorkspaceGizmoMode gizmo_{WorkspaceGizmoMode::Translate};
    bool grid_visible_{true};
    double grid_spacing_{1.0};
    RenderDiagnostics diagnostics_;
};

// =============================================================================
// UI-04: Schematic Editor Model
// =============================================================================

struct SchematicPin {
    std::string name;
    double offset_x{0.0};
    double offset_y{0.0};
};

struct SchematicSymbol {
    std::string symbol_id;
    std::string name;
    double pos_x{0.0};
    double pos_y{0.0};
    double width{80.0};
    double height{60.0};
    std::vector<SchematicPin> pins;
};

struct SchematicWire {
    std::string wire_id;
    std::string net_name;
    std::vector<std::pair<double, double>> waypoints;
};

struct ElectricalIssue {
    std::string message;
    bool is_critical{false};
};

class SchematicEditorModel {
public:
    void add_symbol(SchematicSymbol symbol);
    void add_wire(SchematicWire wire);

    [[nodiscard]] const std::vector<SchematicSymbol>& symbols() const noexcept { return symbols_; }
    [[nodiscard]] const std::vector<SchematicWire>& wires() const noexcept { return wires_; }

    [[nodiscard]] std::vector<ElectricalIssue> run_electrical_diagnostics() const;

private:
    std::vector<SchematicSymbol> symbols_;
    std::vector<SchematicWire> wires_;
};

// =============================================================================
// UI-05: Graph / Flow Editor Model
// =============================================================================

struct FlowSocket {
    std::string name;
    std::string data_type; // e.g. "float", "signal", "event"
    bool is_input{true};
};

struct FlowBlock {
    std::string block_id;
    std::string title;
    double pos_x{0.0};
    double pos_y{0.0};
    std::vector<FlowSocket> sockets;
};

struct FlowConnection {
    std::string source_block_id;
    std::string source_socket;
    std::string target_block_id;
    std::string target_socket;
};

class FlowEditorModel {
public:
    void add_block(FlowBlock block);
    base::Result<void> connect(std::string_view src_id, std::string_view src_sock,
                               std::string_view dst_id, std::string_view dst_sock);

    [[nodiscard]] const std::vector<FlowBlock>& blocks() const noexcept { return blocks_; }
    [[nodiscard]] const std::vector<FlowConnection>& connections() const noexcept { return connections_; }

    void set_highlighted_block(std::string_view block_id) { active_block_ = std::string(block_id); }
    [[nodiscard]] const std::string& active_block() const noexcept { return active_block_; }

private:
    std::vector<FlowBlock> blocks_;
    std::vector<FlowConnection> connections_;
    std::string active_block_;
};

// =============================================================================
// UI-06: Component Editor Model
// =============================================================================

struct ComponentEditorPort {
    std::string name;
    std::string domain;
    std::string direction; // "Input", "Output", "Bidirectional"
    std::string unit_symbol;
};

struct ComponentEditorParameter {
    std::string name;
    double default_value{0.0};
    std::string unit_symbol;
    double min_value{0.0};
    double max_value{100.0};
};

class ComponentEditorModel {
public:
    void set_component_name(std::string name) { name_ = std::move(name); }
    [[nodiscard]] const std::string& component_name() const noexcept { return name_; }

    void add_port(ComponentEditorPort port) { ports_.push_back(std::move(port)); }
    [[nodiscard]] const std::vector<ComponentEditorPort>& ports() const noexcept { return ports_; }

    void add_parameter(ComponentEditorParameter param) { params_.push_back(std::move(param)); }
    [[nodiscard]] const std::vector<ComponentEditorParameter>& parameters() const noexcept { return params_; }

    void set_documentation_markdown(std::string md) { documentation_md_ = std::move(md); }
    [[nodiscard]] const std::string& documentation_markdown() const noexcept { return documentation_md_; }

private:
    std::string name_{"NewComponent"};
    std::vector<ComponentEditorPort> ports_;
    std::vector<ComponentEditorParameter> params_;
    std::string documentation_md_;
};

// =============================================================================
// UI-07: Property Inspector Model
// =============================================================================

struct PropertyItem {
    std::string name;
    std::string category{"General"};
    std::string value;
    std::string default_value;
    std::string unit_symbol;
    bool is_read_only{false};
    std::string validation_error;
};

class PropertyInspectorModel {
public:
    void add_property(PropertyItem item);
    void clear() { properties_.clear(); }

    [[nodiscard]] const std::vector<PropertyItem>& properties() const noexcept { return properties_; }
    base::Result<void> set_value(std::string_view name, std::string_view new_value);
    void reset_to_default(std::string_view name);

private:
    std::vector<PropertyItem> properties_;
};

// =============================================================================
// UI-08: Simulation Toolbar Controller
// =============================================================================

class SimulationToolbarController {
public:
    void set_state(SimulationState state) noexcept { state_ = state; }
    [[nodiscard]] SimulationState state() const noexcept { return state_; }

    void set_mode(ExecutionMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] ExecutionMode mode() const noexcept { return mode_; }

    void update_telemetry(double sim_time, uint64_t step_count, double current_rtf) noexcept;

    [[nodiscard]] double sim_time() const noexcept { return sim_time_; }
    [[nodiscard]] uint64_t step_count() const noexcept { return step_count_; }
    [[nodiscard]] double current_rtf() const noexcept { return current_rtf_; }

    [[nodiscard]] bool can_run() const noexcept;
    [[nodiscard]] bool can_pause() const noexcept;
    [[nodiscard]] bool can_step() const noexcept;
    [[nodiscard]] bool can_stop() const noexcept;

private:
    SimulationState state_{SimulationState::Stopped};
    ExecutionMode mode_{ExecutionMode::ScientificAuthoritative};
    double sim_time_{0.0};
    uint64_t step_count_{0};
    double current_rtf_{1.0};
};

// =============================================================================
// UI-09: Results Workspace Controller
// =============================================================================

struct ActivePlotTrace {
    std::string signal_name;
    std::string color_hex{"#00ffff"};
    bool is_visible{true};
    bool log_scale{false};
};

class ResultsWorkspaceController {
public:
    void add_trace(ActivePlotTrace trace) { traces_.push_back(std::move(trace)); }
    [[nodiscard]] const std::vector<ActivePlotTrace>& traces() const noexcept { return traces_; }

    void set_split_comparison_enabled(bool enabled) noexcept { split_comparison_ = enabled; }
    [[nodiscard]] bool split_comparison_enabled() const noexcept { return split_comparison_; }

private:
    std::vector<ActivePlotTrace> traces_;
    bool split_comparison_{false};
};

// =============================================================================
// UI-10: Code Editor Controller
// =============================================================================

struct DiagnosticMarker {
    uint32_t line{1};
    uint32_t column{1};
    std::string message;
    bool is_error{true};
};

class CodeEditorController {
public:
    void set_text(std::string text) { buffer_ = std::move(text); }
    [[nodiscard]] const std::string& text() const noexcept { return buffer_; }

    void set_language(std::string lang) { language_ = std::move(lang); }
    [[nodiscard]] const std::string& language() const noexcept { return language_; }

    void add_diagnostic(DiagnosticMarker diag) { diagnostics_.push_back(std::move(diag)); }
    void clear_diagnostics() { diagnostics_.clear(); }
    [[nodiscard]] const std::vector<DiagnosticMarker>& diagnostics() const noexcept { return diagnostics_; }

    void toggle_breakpoint(uint32_t line);
    [[nodiscard]] bool has_breakpoint(uint32_t line) const;

private:
    std::string buffer_;
    std::string language_{"lua"};
    std::vector<DiagnosticMarker> diagnostics_;
    std::vector<uint32_t> breakpoints_;
};

// =============================================================================
// UI-11: Library Browser Model
// =============================================================================

struct LibraryPackageCard {
    std::string package_name;
    base::Version version;
    std::string author;
    std::string license;
    std::string category{"Private"}; // "Private", "PublicCommunity", "Templates"
    std::vector<std::string> tags;
    bool is_installed{false};
};

class LibraryBrowserModel {
public:
    void add_package(LibraryPackageCard card) { packages_.push_back(std::move(card)); }
    [[nodiscard]] const std::vector<LibraryPackageCard>& packages() const noexcept { return packages_; }

    [[nodiscard]] std::vector<const LibraryPackageCard*> filter_by_category(std::string_view cat) const;
    [[nodiscard]] std::vector<const LibraryPackageCard*> search(std::string_view query) const;

private:
    std::vector<LibraryPackageCard> packages_;
};

// =============================================================================
// UI-12: Accessibility and Usability Manager
// =============================================================================

struct AccessibilitySettings {
    bool high_contrast_mode{false};
    double ui_scale_factor{1.0}; // 1.0, 1.25, 1.5, 2.0
    bool keyboard_navigation_enabled{true};
    std::string locale{"en_US"};
};

class AccessibilityManager {
public:
    explicit AccessibilityManager(AccessibilitySettings settings = {})
        : settings_(std::move(settings)) {}

    [[nodiscard]] const AccessibilitySettings& settings() const noexcept { return settings_; }

    base::Result<void> set_ui_scale_factor(double scale);
    void set_high_contrast(bool enable) noexcept { settings_.high_contrast_mode = enable; }
    void set_locale(std::string loc) { settings_.locale = std::move(loc); }

private:
    AccessibilitySettings settings_;
};

// =============================================================================
// UI-01-003: Desktop Menu Bar & File Management Architecture
// =============================================================================

enum class MenuCategory {
    File,
    Edit,
    View,
    Insert,
    Simulation,
    Tools,
    Window,
    Help
};

[[nodiscard]] constexpr std::string_view menu_category_to_string(MenuCategory cat) noexcept {
    switch (cat) {
        case MenuCategory::File: return "File";
        case MenuCategory::Edit: return "Edit";
        case MenuCategory::View: return "View";
        case MenuCategory::Insert: return "Insert";
        case MenuCategory::Simulation: return "Simulation";
        case MenuCategory::Tools: return "Tools";
        case MenuCategory::Window: return "Window";
        case MenuCategory::Help: return "Help";
    }
    return "File";
}

[[nodiscard]] std::optional<MenuCategory> menu_category_from_string(std::string_view str) noexcept;

struct MenuItemDescriptor {
    std::string id{};
    std::string title{};
    MenuCategory category{MenuCategory::File};
    std::string shortcut{};
    std::string mnemonic{};
    bool is_enabled{true};
    bool is_checkable{false};
    bool is_checked{false};
    bool is_separator{false};
    std::vector<MenuItemDescriptor> sub_items{};
    std::function<void()> callback{nullptr};

    static MenuItemDescriptor make_action(std::string p_id, std::string p_title, MenuCategory p_cat = MenuCategory::File, std::string p_shortcut = "", std::string p_mnemonic = "") {
        MenuItemDescriptor item;
        item.id = std::move(p_id);
        item.title = std::move(p_title);
        item.category = p_cat;
        item.shortcut = std::move(p_shortcut);
        item.mnemonic = std::move(p_mnemonic);
        return item;
    }

    static MenuItemDescriptor make_separator(std::string p_id) {
        MenuItemDescriptor item;
        item.id = std::move(p_id);
        item.is_separator = true;
        return item;
    }

    static MenuItemDescriptor make_submenu(std::string p_id, std::string p_title, MenuCategory p_cat, std::vector<MenuItemDescriptor> p_subs) {
        MenuItemDescriptor item;
        item.id = std::move(p_id);
        item.title = std::move(p_title);
        item.category = p_cat;
        item.sub_items = std::move(p_subs);
        return item;
    }
};

struct RecentProjectEntry {
    std::string path;
    std::string title;
    std::string last_opened_iso;
    std::string thumbnail_hint;
    bool is_pinned{false};

    bool operator==(const RecentProjectEntry& o) const noexcept {
        return path == o.path;
    }
};

class RecentProjectsManager {
public:
    explicit RecentProjectsManager(size_t max_entries = 10);

    void add_recent_project(std::string path, std::string title, std::string timestamp_iso = "");
    void remove_recent_project(std::string_view path);
    void clear() noexcept;
    void pin_project(std::string_view path, bool pinned);

    [[nodiscard]] const std::vector<RecentProjectEntry>& recent_projects() const noexcept { return entries_; }
    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    [[nodiscard]] std::string serialize_json() const;
    base::Result<void> restore_json(std::string_view json_str);

private:
    size_t max_entries_{10};
    std::vector<RecentProjectEntry> entries_;
};

struct ProjectTemplateDescriptor {
    std::string template_id;
    std::string name;
    std::string category;
    std::string description;
    WorkspacePreset default_preset{WorkspacePreset::Modeling};
    std::vector<std::string> tags;
};

class ProjectTemplateRegistry {
public:
    ProjectTemplateRegistry();

    void register_template(ProjectTemplateDescriptor desc);
    [[nodiscard]] const std::vector<ProjectTemplateDescriptor>& templates() const noexcept { return templates_; }
    [[nodiscard]] std::optional<ProjectTemplateDescriptor> get_template(std::string_view id) const;
    [[nodiscard]] std::vector<ProjectTemplateDescriptor> get_templates_by_category(std::string_view category) const;

private:
    std::vector<ProjectTemplateDescriptor> templates_;
};

class ApplicationMenuBarModel {
public:
    ApplicationMenuBarModel();

    void initialize_default_menu_bar();

    void add_item(MenuItemDescriptor item);
    [[nodiscard]] const std::vector<MenuItemDescriptor>& items_for_category(MenuCategory cat) const;
    [[nodiscard]] MenuItemDescriptor* find_item(std::string_view id);
    [[nodiscard]] const MenuItemDescriptor* find_item(std::string_view id) const;
    bool execute_item(std::string_view id);

    RecentProjectsManager& recent_projects() noexcept { return recent_projects_; }
    [[nodiscard]] const RecentProjectsManager& recent_projects() const noexcept { return recent_projects_; }

    ProjectTemplateRegistry& templates() noexcept { return templates_; }
    [[nodiscard]] const ProjectTemplateRegistry& templates() const noexcept { return templates_; }

private:
    std::unordered_map<MenuCategory, std::vector<MenuItemDescriptor>> menu_map_;
    RecentProjectsManager recent_projects_;
    ProjectTemplateRegistry templates_;

    MenuItemDescriptor* recursive_find(std::vector<MenuItemDescriptor>& items, std::string_view id);
    const MenuItemDescriptor* recursive_find(const std::vector<MenuItemDescriptor>& items, std::string_view id) const;
};

} // namespace shunya::core::domain
