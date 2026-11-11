#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "shunya/core/domain/workspace_ui.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/eng/engineering_pipeline.hpp"

namespace shunya::gui { class ShunyaSketchCanvas2D; }

class ShunyaMenuBarController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentProjectsChanged)
    Q_PROPERTY(QVariantList projectTemplates READ projectTemplates CONSTANT)
    Q_PROPERTY(bool hasRecentProjects READ hasRecentProjects NOTIFY recentProjectsChanged)
    Q_PROPERTY(bool isProjectDirty READ isProjectDirty NOTIFY projectDirtyChanged)

public:
    explicit ShunyaMenuBarController(QObject* parent = nullptr);
    ~ShunyaMenuBarController() override = default;

    [[nodiscard]] QVariantList recentProjects() const;
    [[nodiscard]] QVariantList projectTemplates() const;
    [[nodiscard]] bool hasRecentProjects() const;
    [[nodiscard]] bool isProjectDirty() const;

    void setProject(std::shared_ptr<shunya::core::domain::Project> project);
    void setWorkspace(std::shared_ptr<shunya::eng::EngineeringWorkspaceState> ws);
    void setSketchCanvas2D(shunya::gui::ShunyaSketchCanvas2D* canvas) { sketch_canvas_ = canvas; }

public slots:
    void newProject(const QString& name, const QString& author, const QString& templateId);
    void openProject(const QString& filePath);
    void saveProject();
    void saveProjectAs(const QString& filePath);
    void closeProject(bool force = false);
    void requestExit();
    void forceProceedPendingAction();
    void saveThenProceedPendingAction();

    void importFile(const QString& format, const QString& filePath);
    void exportFile(const QString& format, const QString& filePath);

    void clearRecentProjects();
    void pinRecentProject(const QString& filePath, bool pinned);
    void executeAction(const QString& actionId);

signals:
    void recentProjectsChanged();
    void projectDirtyChanged();
    void confirmUnsavedChanges(const QString& pendingAction);
    void projectLoaded(const QString& name, const QString& path);
    void projectClosed();
    void logMessage(const QString& msg);
    void statusNotice(const QString& title, const QString& message);

private:
    shunya::core::domain::ApplicationMenuBarModel menu_model_;
    std::shared_ptr<shunya::core::domain::Project> active_project_;
    std::shared_ptr<shunya::eng::EngineeringWorkspaceState> workspace_;
    shunya::gui::ShunyaSketchCanvas2D* sketch_canvas_{nullptr};
    QString pending_action_;
};
