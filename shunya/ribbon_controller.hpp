#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "shunya/core/domain/workspace_ui.hpp"
#include "shunya/core/domain/command_stack.hpp"

namespace shunya::gui {

class ShunyaRibbonController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activeTabId READ activeTabId WRITE setActiveTab NOTIFY activeTabChanged)
    Q_PROPERTY(bool isCollapsed READ isCollapsed NOTIFY collapseStateChanged)
    Q_PROPERTY(QStringList pinnedActions READ pinnedActions NOTIFY pinnedActionsChanged)
    Q_PROPERTY(QVariantList historyEntries READ historyEntries NOTIFY historyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString undoName READ undoName NOTIFY historyChanged)
    Q_PROPERTY(QString redoName READ redoName NOTIFY historyChanged)

public:
    explicit ShunyaRibbonController(QObject* parent = nullptr);
    ~ShunyaRibbonController() override = default;

    void setCommandStack(core::domain::CommandStack* stack) noexcept { command_stack_ = stack; emit historyChanged(); }
    [[nodiscard]] core::domain::RibbonBarModel& ribbonModel() noexcept { return ribbon_model_; }
    [[nodiscard]] core::domain::CommandPaletteRegistry& commandRegistry() noexcept { return command_registry_; }

    [[nodiscard]] QString activeTabId() const;
    [[nodiscard]] bool isCollapsed() const;
    [[nodiscard]] QStringList pinnedActions() const;
    [[nodiscard]] QVariantList historyEntries() const;
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QString undoName() const;
    [[nodiscard]] QString redoName() const;

    Q_INVOKABLE void setActiveTab(const QString& tabId);
    Q_INVOKABLE void setContextForPreset(const QString& presetName);
    Q_INVOKABLE void setContextForSelection(const QString& entityType);
    Q_INVOKABLE void toggleCollapsed();

    Q_INVOKABLE void pinAction(const QString& actionId);
    Q_INVOKABLE void unpinAction(const QString& actionId);
    Q_INVOKABLE bool isPinned(const QString& actionId) const;

    Q_INVOKABLE QVariantList fuzzySearchCommands(const QString& query) const;
    Q_INVOKABLE bool executeCommand(const QString& actionId);

    Q_INVOKABLE bool rollbackToIndex(int targetIndex);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();

    Q_INVOKABLE QVariantList getRibbonTabs() const;

signals:
    void activeTabChanged(const QString& tabId);
    void collapseStateChanged(bool collapsed);
    void pinnedActionsChanged();
    void historyChanged();
    void actionTriggered(const QString& actionId);

private:
    core::domain::RibbonBarModel ribbon_model_;
    core::domain::CommandPaletteRegistry command_registry_;
    core::domain::CommandStack* command_stack_{nullptr};

    void initializeDefaultCommands();
};

} // namespace shunya::gui
