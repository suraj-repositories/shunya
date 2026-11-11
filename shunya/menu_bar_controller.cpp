#include "menu_bar_controller.hpp"
#include "sketch_canvas_2d.hpp"

#include "shunya/cad2d/dxf_io.hpp"
#include "shunya/cad2d/svg_io.hpp"
#include "shunya/core/domain/material.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QUrl>
#include <fstream>
#include <sstream>

namespace {
QString toLocalPath(const QString& pathOrUrl) {
    if (pathOrUrl.startsWith("file:")) {
        return QUrl(pathOrUrl).toLocalFile();
    }
    return pathOrUrl;
}
} // namespace

ShunyaMenuBarController::ShunyaMenuBarController(QObject* parent)
    : QObject(parent)
{
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
    QVariantMap bat;
    bat["id"] = "lithium_battery";
    bat["name"] = "Lithium-Ion Battery Workflow (Elements -> Circuit)";
    bat["category"] = "Chemistry & Power";
    bat["description"] = "Complete CSV Elements -> LiFePO4 Materials -> Objects -> Battery -> Closed Circuit Demo.";
    bat["defaultPreset"] = "Simulation";
    list.append(bat);

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

void ShunyaMenuBarController::setWorkspace(std::shared_ptr<shunya::eng::EngineeringWorkspaceState> ws) {
    workspace_ = std::move(ws);
    if (workspace_ && workspace_->project()) {
        active_project_ = workspace_->project();
    }
    emit projectDirtyChanged();
}

void ShunyaMenuBarController::newProject(const QString& name, const QString& author, const QString& templateId) {
    if (active_project_ && active_project_->is_dirty()) {
        pending_action_ = "new:" + name + "|" + author + "|" + templateId;
        emit confirmUnsavedChanges("new_project");
        return;
    }

    if (workspace_ && templateId == "lithium_battery") {
        auto sr = workspace_->seed_lithium_battery_workflow();
        if (sr.is_ok()) {
            active_project_ = workspace_->project();
            if (active_project_ && !name.trimmed().isEmpty()) {
                active_project_->manifest().name = name.trimmed().toStdString();
                active_project_->clear_dirty();
            }
            emit projectLoaded(name.trimmed().isEmpty() ? "LiFePO4_Battery_Demo" : name, "");
            emit projectDirtyChanged();
            emit logMessage("[Project] Seeded complete Lithium Battery Engineering Workflow.");
            return;
        }
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
        if (workspace_) {
            workspace_->reset_project(active_project_);
            active_project_->clear_dirty();
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

    const QString local = toLocalPath(filePath);
    if (workspace_) {
        auto bres = workspace_->load_from_file(local.toStdString());
        if (bres.is_ok()) {
            active_project_ = workspace_->project();
            const QString projName = active_project_
                ? QString::fromStdString(active_project_->manifest().name)
                : QFileInfo(local).baseName();
            menu_model_.recent_projects().add_recent_project(
                local.toStdString(),
                projName.toStdString(),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()
            );
            emit recentProjectsChanged();
            emit projectLoaded(projName, local);
            emit projectDirtyChanged();
            emit logMessage(QString("[Project] Opened Phase 3 bundle from '%1'").arg(local));
            return;
        }
    }

    auto res = shunya::core::domain::Project::open_from_file(local.toStdString());
    if (res.is_ok()) {
        active_project_ = res.value();
        if (workspace_) workspace_->reset_project(active_project_);
        QString projName = QString::fromStdString(active_project_->manifest().name);
        menu_model_.recent_projects().add_recent_project(
            local.toStdString(),
            projName.toStdString(),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()
        );
        emit recentProjectsChanged();
        emit projectLoaded(projName, local);
        emit projectDirtyChanged();
        emit logMessage(QString("[Project] Opened project manifest from '%1'").arg(local));
    } else {
        emit statusNotice("Open Failed", QString::fromStdString(res.error().message));
    }
}

void ShunyaMenuBarController::saveProject() {
    if (!active_project_) return;

    if (active_project_->file_path().empty()) {
        pending_action_ = "save_as";
        emit confirmUnsavedChanges("save_as");
        return;
    }

    auto res = workspace_
        ? workspace_->save_to_file(active_project_->file_path())
        : active_project_->save_to_file();
    if (res.is_ok()) {
        emit projectDirtyChanged();
        emit logMessage(QString("[Project] Project saved to '%1'").arg(QString::fromStdString(active_project_->file_path())));
        emit statusNotice("Project Saved", QString::fromStdString(active_project_->file_path()));
    } else {
        emit statusNotice("Save Failed", QString::fromStdString(res.error().message));
    }
}

void ShunyaMenuBarController::saveProjectAs(const QString& filePath) {
    if (!active_project_) return;
    const QString local = toLocalPath(filePath);
    if (local.isEmpty()) return;

    auto res = workspace_
        ? workspace_->save_to_file(local.toStdString())
        : active_project_->save_to_file(local.toStdString());
    if (res.is_ok()) {
        QString projName = QString::fromStdString(active_project_->manifest().name);
        menu_model_.recent_projects().add_recent_project(
            local.toStdString(),
            projName.toStdString(),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()
        );
        emit recentProjectsChanged();
        emit projectDirtyChanged();
        emit logMessage(QString("[Project] Saved project bundle to '%1'").arg(local));
        emit statusNotice("Project Saved", local);
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

    if (workspace_) {
        auto empty = shunya::core::domain::Project::create_new("Untitled_Project", "Engineer");
        if (empty.is_ok()) {
            workspace_->reset_project(empty.value());
            active_project_ = workspace_->project();
            active_project_->clear_dirty();
        }
    } else {
        active_project_.reset();
    }
    emit projectClosed();
    emit projectDirtyChanged();
    emit logMessage("[Project] Project reset/closed.");
}

void ShunyaMenuBarController::requestExit() {
    if (active_project_ && active_project_->is_dirty()) {
        pending_action_ = "exit";
        emit confirmUnsavedChanges("exit_app");
        return;
    }
    emit logMessage("[Application] Clean shutdown requested.");
}

void ShunyaMenuBarController::forceProceedPendingAction() {
    if (active_project_) active_project_->clear_dirty();
    const QString act = pending_action_;
    pending_action_.clear();
    if (act.startsWith("new:")) {
        const QStringList p = act.mid(4).split('|');
        newProject(p.value(0, "New_Project"), p.value(1, "Engineer"), p.value(2, "blank"));
    } else if (act.startsWith("open:")) {
        openProject(act.mid(5));
    } else if (act == "close") {
        closeProject(true);
    }
}

void ShunyaMenuBarController::saveThenProceedPendingAction() {
    if (active_project_ && !active_project_->file_path().empty()) {
        saveProject();
    }
    forceProceedPendingAction();
}

void ShunyaMenuBarController::importFile(const QString& format, const QString& filePath) {
    const QString local = toLocalPath(filePath);
    if (local.isEmpty()) return;
    const QString fmt = format.toUpper();

    if (fmt == "DXF" && sketch_canvas_) {
        shunya::cad2d::DxfDocument doc;
        if (shunya::cad2d::DxfParser::parse_file(local.toStdString(), doc)) {
            sketch_canvas_->setDocument(std::move(doc.sketch));
            emit logMessage(QString("[Import] Imported DXF sketch from '%1'").arg(local));
            emit statusNotice("DXF Import Complete", QFileInfo(local).fileName());
            return;
        }
        emit statusNotice("Import Failed", "Could not parse DXF file: " + local);
        return;
    }

    if (fmt == "SVG" && sketch_canvas_) {
        std::ifstream is(local.toStdString(), std::ios::binary);
        if (is.is_open()) {
            std::ostringstream ss;
            ss << is.rdbuf();
            shunya::cad2d::SvgDocument sdoc;
            if (shunya::cad2d::SvgExchange::import_svg(ss.str(), sdoc)) {
                sketch_canvas_->setDocument(std::move(sdoc.sketch));
                emit logMessage(QString("[Import] Imported SVG sketch from '%1'").arg(local));
                emit statusNotice("SVG Import Complete", QFileInfo(local).fileName());
                return;
            }
        }
        emit statusNotice("Import Failed", "Could not parse SVG file: " + local);
        return;
    }

    if (fmt == "MATERIAL_JSON" && workspace_) {
        std::ifstream is(local.toStdString(), std::ios::binary);
        if (is.is_open()) {
            std::ostringstream ss;
            ss << is.rdbuf();
            auto mr = shunya::core::domain::Material::from_json(ss.str());
            if (mr.is_ok()) {
                auto reg = workspace_->material_registry().register_material(mr.value());
                if (reg.is_ok()) {
                    workspace_->notify_change(shunya::eng::DependencyChangeKind::MaterialUpdated, mr.value().id(),
                                              "Imported material '" + mr.value().name() + "'");
                    emit statusNotice("Material Imported", QString::fromStdString(mr.value().name()));
                    return;
                }
            }
        }
        emit statusNotice("Import Failed", "Invalid Material JSON: " + local);
        return;
    }

    emit logMessage(QString("[Import] Dispatched %1 import from '%2'").arg(format, local));
}

void ShunyaMenuBarController::exportFile(const QString& format, const QString& filePath) {
    const QString local = toLocalPath(filePath);
    if (local.isEmpty()) return;
    const QString fmt = format.toUpper();

    if (fmt == "DXF" && sketch_canvas_) {
        shunya::cad2d::LayerManager lm;
        shunya::cad2d::BlockManager bm;
        if (shunya::cad2d::DxfWriter::write_file(local.toStdString(), sketch_canvas_->document(), lm, bm)) {
            emit logMessage(QString("[Export] Exported DXF to '%1'").arg(local));
            emit statusNotice("DXF Export Complete", QFileInfo(local).fileName());
            return;
        }
        emit statusNotice("Export Failed", "Unable to write DXF: " + local);
        return;
    }

    if (fmt == "SVG" && sketch_canvas_) {
        shunya::cad2d::LayerManager lm;
        if (shunya::cad2d::SvgExchange::export_svg_file(local.toStdString(), sketch_canvas_->document(), lm)) {
            emit logMessage(QString("[Export] Exported SVG to '%1'").arg(local));
            emit statusNotice("SVG Export Complete", QFileInfo(local).fileName());
            return;
        }
        emit statusNotice("Export Failed", "Unable to write SVG: " + local);
        return;
    }

    if (fmt == "BUNDLE" && workspace_) {
        auto sr = workspace_->save_to_file(local.toStdString());
        if (sr.is_ok()) {
            emit statusNotice("Project Bundle Exported", QFileInfo(local).fileName());
        } else {
            emit statusNotice("Export Failed", QString::fromStdString(sr.error().message));
        }
        return;
    }

    emit logMessage(QString("[Export] Dispatched %1 export to '%2'").arg(format, local));
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
    if (actionId == "help.about") {
        emit statusNotice("About Shunya",
            "Shunya v3.0 — Chemistry-Based Material Engineering, Object Modeling, Battery, Circuit & Multiphysics Studio.");
    } else if (actionId == "tools.validate" && workspace_) {
        const auto rep = workspace_->validate_workspace();
        if (rep.is_valid()) {
            emit statusNotice("Validation Passed", "All engineering pipeline links verified without errors.");
        } else {
            emit statusNotice("Validation Issues Found",
                              QString::number(rep.issues().size()) + " issue(s) reported.");
        }
    }
    emit logMessage(QString("[Menu] Executing menu action '%1'").arg(actionId));
}
