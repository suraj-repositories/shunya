#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

#include "shunya/core/domain/workspace_ui.hpp"

namespace shunya::gui {

class ShunyaDockingController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activePreset READ activePreset WRITE applyPreset NOTIFY presetChanged)
    Q_PROPERTY(QString layoutJson READ layoutJson NOTIFY layoutChanged)
    Q_PROPERTY(bool hasFloatingPanels READ hasFloatingPanels NOTIFY floatingStateChanged)

public:
    explicit ShunyaDockingController(QObject* parent = nullptr);
    ~ShunyaDockingController() override = default;

    [[nodiscard]] QString activePreset() const;
    [[nodiscard]] QString layoutJson() const;
    [[nodiscard]] bool hasFloatingPanels() const;

    Q_INVOKABLE void applyPreset(const QString& presetName);
    Q_INVOKABLE bool togglePanel(const QString& panelId);
    Q_INVOKABLE bool isPanelVisible(const QString& panelId) const;
    Q_INVOKABLE bool isPanelFloating(const QString& panelId) const;
    Q_INVOKABLE bool setSplitRatio(const QString& splitNodeId, qreal ratio);
    Q_INVOKABLE bool dockPanel(const QString& panelId, const QString& title, const QString& targetNodeId, int snapZone);
    Q_INVOKABLE bool floatPanel(const QString& panelId, bool floating);
    Q_INVOKABLE bool removePanel(const QString& nodeId);
    Q_INVOKABLE bool triggerShortcut(const QString& keySequence);
    Q_INVOKABLE QString saveLayoutToJson();
    Q_INVOKABLE bool loadLayoutFromJson(const QString& json);
    Q_INVOKABLE QVariantMap getPanelHierarchy() const;

signals:
    void layoutChanged();
    void presetChanged(const QString& preset);
    void panelVisibilityChanged(const QString& panelId, bool visible);
    void floatingStateChanged();

private:
    core::domain::DockingManager docking_manager_;
    core::domain::DockActionManager action_manager_;
    QString active_preset_{"Modeling"};

    QVariantMap nodeToVariant(const std::shared_ptr<core::domain::DockNode>& node) const;
};

} // namespace shunya::gui
