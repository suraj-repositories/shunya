#include "ribbon_controller.hpp"

namespace shunya::gui {

ShunyaRibbonController::ShunyaRibbonController(QObject* parent)
    : QObject(parent) {
    initializeDefaultCommands();
}

void ShunyaRibbonController::initializeDefaultCommands() {
    using namespace core::domain;

    command_registry_.register_action(CommandPaletteAction::make("file.new", "File", "New Project", "Ctrl+N", {"new", "create", "project", "wizard"}));
    command_registry_.register_action(CommandPaletteAction::make("file.open", "File", "Open Project...", "Ctrl+O", {"open", "load", "file"}));
    command_registry_.register_action(CommandPaletteAction::make("file.save", "File", "Save Project", "Ctrl+S", {"save", "commit", "disk"}));
    command_registry_.register_action(CommandPaletteAction::make("file.save_as", "File", "Save Project As...", "Ctrl+Shift+S", {"save", "copy"}));
    command_registry_.register_action(CommandPaletteAction::make("edit.undo", "Edit", "Undo", "Ctrl+Z", {"undo", "revert"}));
    command_registry_.register_action(CommandPaletteAction::make("edit.redo", "Edit", "Redo", "Ctrl+Y", {"redo", "repeat"}));
    command_registry_.register_action(CommandPaletteAction::make("edit.cut", "Edit", "Cut Selection", "Ctrl+X", {"cut", "clipboard"}));
    command_registry_.register_action(CommandPaletteAction::make("edit.copy", "Edit", "Copy Selection", "Ctrl+C", {"copy", "clipboard"}));
    command_registry_.register_action(CommandPaletteAction::make("edit.paste", "Edit", "Paste", "Ctrl+V", {"paste", "insert"}));

    // Modeling & Drawing
    command_registry_.register_action(CommandPaletteAction::make("draw.line", "2D Sketch", "Draw 2D Line", "L", {"line", "sketch", "vector"}));
    command_registry_.register_action(CommandPaletteAction::make("draw.circle", "2D Sketch", "Draw Parametric Circle", "C", {"circle", "arc", "round"}));
    command_registry_.register_action(CommandPaletteAction::make("draw.rect", "2D Sketch", "Draw Rectangle", "R", {"rect", "box", "square"}));
    command_registry_.register_action(CommandPaletteAction::make("feat.extrude", "3D Mesh", "Extrude 3D Solid", "E", {"extrude", "solid", "boss"}));
    command_registry_.register_action(CommandPaletteAction::make("feat.revolve", "3D Mesh", "Revolve 3D Solid", "V", {"revolve", "lathe", "axis"}));
    command_registry_.register_action(CommandPaletteAction::make("mesh.fillet", "3D Mesh", "Edge Fillet", "F", {"fillet", "round", "bevel"}));

    // PCB & Circuits
    command_registry_.register_action(CommandPaletteAction::make("pcb.resistor", "PCB Design", "Place Resistor", "", {"resistor", "passive", "ohm"}));
    command_registry_.register_action(CommandPaletteAction::make("pcb.capacitor", "PCB Design", "Place Capacitor", "", {"capacitor", "passive", "farad"}));
    command_registry_.register_action(CommandPaletteAction::make("pcb.route45", "PCB Design", "Interactive 45° Router", "X", {"route", "trace", "wire"}));

    // Multiphysics & Simulation
    command_registry_.register_action(CommandPaletteAction::make("sim.run", "Simulation", "Start Simulation", "F5", {"run", "solve", "execute", "play"}));
    command_registry_.register_action(CommandPaletteAction::make("sim.pause", "Simulation", "Pause Simulation", "F6", {"pause", "freeze", "halt"}));
    command_registry_.register_action(CommandPaletteAction::make("sim.step", "Simulation", "Step Timestep Forward", "F10", {"step", "advance", "next"}));
    command_registry_.register_action(CommandPaletteAction::make("sim.stop", "Simulation", "Stop Simulation", "Shift+F5", {"stop", "abort", "halt"}));
    command_registry_.register_action(CommandPaletteAction::make("sim.reset", "Simulation", "Reset Simulation Clock", "", {"reset", "zero", "restart"}));

    // Views & Materials
    command_registry_.register_action(CommandPaletteAction::make("mat.periodic", "Materials", "Periodic Table Database", "Ctrl+P", {"elements", "atoms", "chemistry", "isotopes"}));
    command_registry_.register_action(CommandPaletteAction::make("view.toggle_console", "View", "Toggle Diagnostics Console", "F12", {"console", "terminal", "logs", "output"}));
    command_registry_.register_action(CommandPaletteAction::make("view.toggle_sidebars", "View", "Toggle Sidebars", "Ctrl+B", {"sidebar", "browser", "inspector"}));
}

QString ShunyaRibbonController::activeTabId() const {
    return QString::fromStdString(ribbon_model_.active_tab_id());
}

bool ShunyaRibbonController::isCollapsed() const {
    return ribbon_model_.is_collapsed();
}

QStringList ShunyaRibbonController::pinnedActions() const {
    QStringList list;
    for (const auto& id : ribbon_model_.quick_access().pinned_action_ids()) {
        list.append(QString::fromStdString(id));
    }
    return list;
}

QVariantList ShunyaRibbonController::historyEntries() const {
    QVariantList list;
    if (!command_stack_) return list;

    auto entries = command_stack_->history_entries();
    for (const auto& e : entries) {
        QVariantMap map;
        map["index"] = static_cast<int>(e.index);
        map["name"] = QString::fromStdString(e.name);
        map["isUndone"] = e.is_undone;
        map["isCurrent"] = e.is_current;
        list.append(map);
    }
    return list;
}

bool ShunyaRibbonController::canUndo() const {
    return command_stack_ ? command_stack_->can_undo() : false;
}

bool ShunyaRibbonController::canRedo() const {
    return command_stack_ ? command_stack_->can_redo() : false;
}

QString ShunyaRibbonController::undoName() const {
    return command_stack_ ? QString::fromStdString(std::string(command_stack_->undo_name())) : QString();
}

QString ShunyaRibbonController::redoName() const {
    return command_stack_ ? QString::fromStdString(std::string(command_stack_->redo_name())) : QString();
}

void ShunyaRibbonController::setActiveTab(const QString& tabId) {
    ribbon_model_.set_active_tab(tabId.toStdString());
    emit activeTabChanged(tabId);
}

void ShunyaRibbonController::setContextForPreset(const QString& presetName) {
    auto preset = core::domain::workspace_preset_from_string(presetName.toStdString());
    if (preset) {
        ribbon_model_.set_context_for_workspace(*preset);
        emit activeTabChanged(QString::fromStdString(ribbon_model_.active_tab_id()));
    }
}

void ShunyaRibbonController::setContextForSelection(const QString& entityType) {
    ribbon_model_.set_context_for_selection(entityType.toStdString());
    emit activeTabChanged(QString::fromStdString(ribbon_model_.active_tab_id()));
}

void ShunyaRibbonController::toggleCollapsed() {
    ribbon_model_.toggle_collapsed();
    emit collapseStateChanged(ribbon_model_.is_collapsed());
}

void ShunyaRibbonController::pinAction(const QString& actionId) {
    ribbon_model_.quick_access().pin_action(actionId.toStdString());
    emit pinnedActionsChanged();
}

void ShunyaRibbonController::unpinAction(const QString& actionId) {
    ribbon_model_.quick_access().unpin_action(actionId.toStdString());
    emit pinnedActionsChanged();
}

bool ShunyaRibbonController::isPinned(const QString& actionId) const {
    return ribbon_model_.quick_access().is_pinned(actionId.toStdString());
}

QVariantList ShunyaRibbonController::fuzzySearchCommands(const QString& query) const {
    QVariantList list;
    auto results = command_registry_.fuzzy_search(query.toStdString());
    for (const auto& r : results) {
        QVariantMap map;
        map["actionId"] = QString::fromStdString(r.action.action_id);
        map["category"] = QString::fromStdString(r.action.category);
        map["title"] = QString::fromStdString(r.action.title);
        map["shortcut"] = QString::fromStdString(r.action.shortcut);
        map["score"] = r.score;
        list.append(map);
    }
    return list;
}

bool ShunyaRibbonController::executeCommand(const QString& actionId) {
    emit actionTriggered(actionId);
    return command_registry_.execute_action(actionId.toStdString());
}

bool ShunyaRibbonController::rollbackToIndex(int targetIndex) {
    if (!command_stack_) return false;
    auto res = command_stack_->rollback_to_index(static_cast<size_t>(targetIndex));
    if (res.is_ok()) {
        emit historyChanged();
        return true;
    }
    return false;
}

bool ShunyaRibbonController::undo() {
    if (!command_stack_) return false;
    auto res = command_stack_->undo();
    if (res.is_ok()) {
        emit historyChanged();
        return true;
    }
    return false;
}

bool ShunyaRibbonController::redo() {
    if (!command_stack_) return false;
    auto res = command_stack_->redo();
    if (res.is_ok()) {
        emit historyChanged();
        return true;
    }
    return false;
}

QVariantList ShunyaRibbonController::getRibbonTabs() const {
    QVariantList tabsList;
    for (const auto& tab : ribbon_model_.tabs()) {
        QVariantMap tabMap;
        tabMap["id"] = QString::fromStdString(tab.id);
        tabMap["title"] = QString::fromStdString(tab.title);
        tabMap["category"] = QString::fromStdString(std::string(core::domain::ribbon_category_to_string(tab.category)));
        tabMap["isContextual"] = tab.is_contextual;
        tabMap["isActive"] = (tab.id == ribbon_model_.active_tab_id());

        QVariantList groupsList;
        for (const auto& grp : tab.groups) {
            QVariantMap grpMap;
            grpMap["id"] = QString::fromStdString(grp.id);
            grpMap["title"] = QString::fromStdString(grp.title);

            QVariantList actsList;
            for (const auto& act : grp.actions) {
                QVariantMap actMap;
                actMap["id"] = QString::fromStdString(act.id);
                actMap["label"] = QString::fromStdString(act.label);
                actMap["icon"] = QString::fromStdString(act.icon_name);
                actMap["tooltip"] = QString::fromStdString(act.tooltip);
                actMap["shortcut"] = QString::fromStdString(act.shortcut);
                actMap["isLarge"] = (act.size == core::domain::RibbonItemSize::Large);
                actMap["isEnabled"] = act.is_enabled;
                actsList.append(actMap);
            }
            grpMap["actions"] = actsList;
            groupsList.append(grpMap);
        }
        tabMap["groups"] = groupsList;
        tabsList.append(tabMap);
    }
    return tabsList;
}

} // namespace shunya::gui
