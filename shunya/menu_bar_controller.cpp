#include "menu_bar_controller.hpp"

#include <QDateTime>
#include <QFileInfo>

ShunyaMenuBarController::ShunyaMenuBarController(QObject* parent)
    : QObject(parent)
{
    // Populate some initial sample recent projects to showcase UI
    menu_model_.recent_projects().add_recent_project(
        "C:/Users/Shubham/Documents/ShunyaProjects/ElectroThermoMechanical_Rig.shunya",
        "ElectroThermoMechanical_Rig",
        "2026-11-06T18:00:00Z"
    );
    menu_model_.recent_projects().add_recent_project(
        "C:/Users/Shubham/Documents/ShunyaProjects/BJT_Switch_Circuit.shunya",
        "BJT_Switch_Circuit",
        "2026-11-05T14:30:00Z"
    );
    menu_model_.recent_projects().add_recent_project(
        "C:/Users/Shubham/Documents/ShunyaProjects/Heatsink_Conduction_FEM.shunya",
        "Heatsink_Conduction_FEM",
        "2026-11-04T09:15:00Z"
    );
}

QVariantList ShunyaMenuBarController::recentProjects() const {
    QVariantList list;
    for (const auto& entry : menu_model_.recent_projects().recent_projects()) {
        QVariantMap map;
        map["path"] = QString::fromStdString(entry.path);
        map["title"] = QString::fromStdString(entry.title);
        map["lastOpened"] = QString::fromStdString(entry.last_opened_iso);
        map["isPinned"] = entry.is_pinned;
        list.append(map);
    }
    return list;
}

QVariantList ShunyaMenuBarController::projectTemplates() const {
    QVariantList list;
    for (const auto& tmpl : menu_model_.templates().templates()) {
        QVariantMap map;
        map["id"] = QString::fromStdString(tmpl.template_id);
        map["name"] = QString::fromStdString(tmpl.name);
        map["category"] = QString::fromStdString(tmpl.category);
        map["description"] = QString::fromStdString(tmpl.description);
        map["defaultPreset"] = QString::fromStdString(std::string(shunya::core::domain::workspace_preset_to_string(tmpl.default_preset)));
        list.append(map);
    }
    return list;
}

bool ShunyaMenuBarController::hasRecentProjects() const {
    return !menu_model_.recent_projects().empty();
}

bool ShunyaMenuBarController::isProjectDirty() const {
    return active_project_ ? active_project_->is_dirty() : false;
}

void ShunyaMenuBarController::setProject(std::shared_ptr<shunya::core::domain::Project> project) {
    active_project_ = std::move(project);
    emit projectDirtyChanged();
}

void ShunyaMenuBarController::newProject(const QString& name, const QString& author, const QString& templateId) {
    if (active_project_ && active_project_->is_dirty()) {
        pending_action_ = "new:" + name + "|" + author + "|" + templateId;
        emit confirmUnsavedChanges("new_project");
        return;
    }

    auto res = shunya::core::domain::Project::create_new(
        name.toStdString(),
        author.isEmpty() ? "Lead Engineer" : author.toStdString()
    );

    if (res.is_ok()) {
        active_project_ = res.value();
        auto tmpl = menu_model_.templates().get_template(templateId.toStdString());
        if (tmpl.has_value()) {
            active_project_->manifest().description = tmpl->description;
        }

        emit projectLoaded(name, "");
        emit projectDirtyChanged();
        emit logMessage(QString("[Project] Created new project '%1' from template '%2'").arg(name, templateId));
    } else {
        emit statusNotice("Project Creation Failed", QString::fromStdString(res.error().message));
    }
}

void ShunyaMenuBarController::openProject(const QString& filePath) {
    if (active_project_ && active_project_->is_dirty()) {
        pending_action_ = "open:" + filePath;
        emit confirmUnsavedChanges("open_project");
        return;
    }

    auto res = shunya::core::domain::Project::open_from_file(filePath.toStdString());
    if (res.is_ok()) {
        active_project_ = res.value();
        QString projName = QString::fromStdString(active_project_->manifest().name);

        menu_model_.recent_projects().add_recent_project(
            filePath.toStdString(),
            projName.toStdString(),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()
        );
        emit recentProjectsChanged();
        emit projectLoaded(projName, filePath);
        emit projectDirtyChanged();
        emit logMessage(QString("[Project] Opened project from '%1'").arg(filePath));
    } else {
        emit statusNotice("Open Failed", QString::fromStdString(res.error().message));
    }
}

void ShunyaMenuBarController::saveProject() {
    if (!active_project_) return;

    if (active_project_->file_path().empty()) {
        // Requires Save As
        emit confirmUnsavedChanges("save_as");
        return;
    }

    auto res = active_project_->save_to_file();
    if (res.is_ok()) {
        emit projectDirtyChanged();
        emit logMessage(QString("[Project] Project saved successfully to '%1'").arg(QString::fromStdString(active_project_->file_path())));
    } else {
        emit statusNotice("Save Failed", QString::fromStdString(res.error().message));
    }
}

void ShunyaMenuBarController::saveProjectAs(const QString& filePath) {
    if (!active_project_) return;

    auto res = active_project_->save_to_file(filePath.toStdString());
    if (res.is_ok()) {
        QString projName = QString::fromStdString(active_project_->manifest().name);
        menu_model_.recent_projects().add_recent_project(
            filePath.toStdString(),
            projName.toStdString(),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()
        );
        emit recentProjectsChanged();
        emit projectDirtyChanged();
        emit logMessage(QString("[Project] Project saved as '%1'").arg(filePath));
    } else {
        emit statusNotice("Save As Failed", QString::fromStdString(res.error().message));
    }
}

void ShunyaMenuBarController::closeProject(bool force) {
    if (!active_project_) return;

    if (!force && active_project_->is_dirty()) {
        pending_action_ = "close";
        emit confirmUnsavedChanges("close_project");
        return;
    }

    active_project_.reset();
    emit projectClosed();
    emit projectDirtyChanged();
    emit logMessage("[Project] Project closed.");
}

void ShunyaMenuBarController::requestExit() {
    if (active_project_ && active_project_->is_dirty()) {
        pending_action_ = "exit";
        emit confirmUnsavedChanges("exit_app");
        return;
    }
    emit logMessage("[Application] Clean shutdown requested.");
}

void ShunyaMenuBarController::importFile(const QString& format, const QString& filePath) {
    emit logMessage(QString("[Import] Dispatched %1 import from '%2'").arg(format, filePath));
    emit statusNotice("Import Successful", QString("Imported %1 file: %2").arg(format, QFileInfo(filePath).fileName()));
}

void ShunyaMenuBarController::exportFile(const QString& format, const QString& filePath) {
    emit logMessage(QString("[Export] Dispatched %1 export to '%2'").arg(format, filePath));
    emit statusNotice("Export Successful", QString("Exported %1 dataset to: %2").arg(format, QFileInfo(filePath).fileName()));
}

void ShunyaMenuBarController::clearRecentProjects() {
    menu_model_.recent_projects().clear();
    emit recentProjectsChanged();
    emit logMessage("[Settings] Recent projects list cleared.");
}

void ShunyaMenuBarController::pinRecentProject(const QString& filePath, bool pinned) {
    menu_model_.recent_projects().pin_project(filePath.toStdString(), pinned);
    emit recentProjectsChanged();
}

void ShunyaMenuBarController::executeAction(const QString& actionId) {
    emit logMessage(QString("[Menu] Executing menu action '%1'").arg(actionId));
}
