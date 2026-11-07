#include "docking_controller.hpp"

namespace shunya::gui {

ShunyaDockingController::ShunyaDockingController(QObject* parent)
    : QObject(parent) {
    docking_manager_.apply_preset(core::domain::WorkspacePreset::Modeling);
}

QString ShunyaDockingController::activePreset() const {
    return active_preset_;
}

QString ShunyaDockingController::layoutJson() const {
    return QString::fromStdString(docking_manager_.serialize_layout_json());
}

bool ShunyaDockingController::hasFloatingPanels() const {
    for (const auto& leaf : docking_manager_.get_all_leaves()) {
        if (leaf && leaf->is_floating) return true;
    }
    return false;
}

void ShunyaDockingController::applyPreset(const QString& presetName) {
    auto p = core::domain::workspace_preset_from_string(presetName.toStdString());
    if (p) {
        docking_manager_.apply_preset(*p);
        active_preset_ = presetName;
        emit presetChanged(active_preset_);
        emit layoutChanged();
        emit floatingStateChanged();
    }
}

bool ShunyaDockingController::togglePanel(const QString& panelId) {
    auto panel = docking_manager_.find_panel(panelId.toStdString());
    if (!panel) return false;
    bool new_visible = !panel->is_visible;
    bool ok = docking_manager_.toggle_panel_visibility(panelId.toStdString(), new_visible);
    if (ok) {
        emit panelVisibilityChanged(panelId, new_visible);
        emit layoutChanged();
    }
    return ok;
}

bool ShunyaDockingController::isPanelVisible(const QString& panelId) const {
    auto panel = docking_manager_.find_panel(panelId.toStdString());
    return panel ? panel->is_visible : false;
}

bool ShunyaDockingController::isPanelFloating(const QString& panelId) const {
    auto panel = docking_manager_.find_panel(panelId.toStdString());
    return panel ? panel->is_floating : false;
}

bool ShunyaDockingController::setSplitRatio(const QString& splitNodeId, qreal ratio) {
    bool ok = docking_manager_.set_split_ratio(splitNodeId.toStdString(), static_cast<double>(ratio));
    if (ok) {
        emit layoutChanged();
    }
    return ok;
}

bool ShunyaDockingController::dockPanel(const QString& panelId, const QString& title, const QString& targetNodeId, int snapZone) {
    auto zone = static_cast<core::domain::DockSnapZone>(snapZone);
    bool ok = docking_manager_.dock_panel(panelId.toStdString(), title.toStdString(), targetNodeId.toStdString(), zone);
    if (ok) {
        emit layoutChanged();
    }
    return ok;
}

bool ShunyaDockingController::floatPanel(const QString& panelId, bool floating) {
    bool ok = docking_manager_.set_floating(panelId.toStdString(), floating);
    if (ok) {
        emit floatingStateChanged();
        emit layoutChanged();
    }
    return ok;
}

bool ShunyaDockingController::removePanel(const QString& nodeId) {
    bool ok = docking_manager_.remove_node(nodeId.toStdString());
    if (ok) {
        emit layoutChanged();
        emit floatingStateChanged();
    }
    return ok;
}

bool ShunyaDockingController::triggerShortcut(const QString& keySequence) {
    bool ok = action_manager_.execute_shortcut(keySequence.toStdString(), docking_manager_);
    if (ok) {
        emit layoutChanged();
    }
    return ok;
}

QString ShunyaDockingController::saveLayoutToJson() {
    return QString::fromStdString(docking_manager_.serialize_layout_json());
}

bool ShunyaDockingController::loadLayoutFromJson(const QString& json) {
    auto res = docking_manager_.restore_layout_json(json.toStdString());
    if (res.is_ok()) {
        emit layoutChanged();
        emit floatingStateChanged();
        return true;
    }
    return false;
}

QVariantMap ShunyaDockingController::nodeToVariant(const std::shared_ptr<core::domain::DockNode>& node) const {
    QVariantMap map;
    if (!node) return map;

    map["node_id"] = QString::fromStdString(node->node_id);
    map["type"] = QString::fromStdString(std::string(core::domain::dock_node_type_to_string(node->type)));

    if (node->type == core::domain::DockNodeType::Leaf) {
        map["panel_id"] = QString::fromStdString(node->panel_id);
        map["title"] = QString::fromStdString(node->title);
        map["is_visible"] = node->is_visible;
        map["is_closable"] = node->is_closable;
        map["is_floating"] = node->is_floating;
        map["min_width"] = node->min_width;
        map["min_height"] = node->min_height;
    } else if (node->type == core::domain::DockNodeType::SplitHorizontal || node->type == core::domain::DockNodeType::SplitVertical) {
        map["split_ratio"] = node->split_ratio;
        map["first_child"] = nodeToVariant(node->first_child);
        map["second_child"] = nodeToVariant(node->second_child);
    } else if (node->type == core::domain::DockNodeType::Tabbed) {
        map["active_tab_index"] = static_cast<int>(node->active_tab_index);
        QVariantList tabsList;
        for (const auto& t : node->tabs) {
            tabsList.append(nodeToVariant(t));
        }
        map["tabs"] = tabsList;
    }

    return map;
}

QVariantMap ShunyaDockingController::getPanelHierarchy() const {
    return nodeToVariant(docking_manager_.root());
}

} // namespace shunya::gui
