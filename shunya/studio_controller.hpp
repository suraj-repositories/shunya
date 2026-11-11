#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "shunya/core/base/app_context.hpp"
#include "shunya/core/base/logging.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/command_stack.hpp"
#include "shunya/core/domain/scene_graph.hpp"
#include "shunya/core/domain/simulation_clock.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/thermal.hpp"
#include "shunya/core/domain/mechanics.hpp"
#include "shunya/eng/engineering_pipeline.hpp"

namespace shunya::gui { class ShunyaPipelineController; }

class ShunyaStudioController : public QObject {
    Q_OBJECT
    Q_PROPERTY(double simTime READ simTime NOTIFY simTimeChanged)
    Q_PROPERTY(QString simStatus READ simStatus NOTIFY simStatusChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectNameChanged)
    Q_PROPERTY(QStringList activeEntities READ activeEntities NOTIFY activeEntitiesChanged)
    Q_PROPERTY(QString benchmarkOutput READ benchmarkOutput NOTIFY benchmarkOutputChanged)
    Q_PROPERTY(QString logOutput READ logOutput NOTIFY logOutputChanged)

public:
    explicit ShunyaStudioController(QObject* parent = nullptr);
    ~ShunyaStudioController() override;

    [[nodiscard]] double simTime() const noexcept { return sim_time_; }
    [[nodiscard]] QString simStatus() const noexcept { return sim_status_; }
    [[nodiscard]] QString projectName() const noexcept { return project_name_; }
    [[nodiscard]] QStringList activeEntities() const noexcept { return active_entities_; }
    [[nodiscard]] QString benchmarkOutput() const noexcept { return benchmark_output_; }
    [[nodiscard]] QString logOutput() const noexcept { return log_output_; }
    [[nodiscard]] std::shared_ptr<shunya::core::domain::Project> currentProject() const noexcept { return current_project_; }
    [[nodiscard]] std::shared_ptr<shunya::eng::EngineeringWorkspaceState> workspace() const noexcept { return workspace_; }
    [[nodiscard]] shunya::core::domain::CommandStack& commandStack() noexcept { return workspace_ ? workspace_->command_stack() : command_stack_; }

    void setMenuBarController(class ShunyaMenuBarController* ctrl);
    void setPipelineController(shunya::gui::ShunyaPipelineController* ctrl) { pipeline_ = ctrl; }

public slots:
    void startSimulation();
    void pauseSimulation();
    void stopSimulation();
    void stepSimulation();
    void resetSimulation();

    void createProject(const QString& name, const QString& author);
    void openProject(const QString& path);
    void saveProject(const QString& path);

    void runBenchmark();
    void runSweep(const QString& param, double minVal, double maxVal, int steps);
    void appendLog(const QString& msg);

signals:
    void simTimeChanged();
    void simStatusChanged();
    void projectNameChanged();
    void activeEntitiesChanged();
    void benchmarkOutputChanged();
    void logOutputChanged();

private slots:
    void onTimerTick();

private:
    double sim_time_{0.0};
    QString sim_status_{"Stopped"};
    QString project_name_{"ElectroThermoMechanical_Rig"};
    QStringList active_entities_;
    QString benchmark_output_;
    QString log_output_;

    QTimer* timer_{nullptr};
    std::shared_ptr<shunya::eng::EngineeringWorkspaceState> workspace_;
    shunya::gui::ShunyaPipelineController* pipeline_{nullptr};
    std::shared_ptr<shunya::core::domain::Project> current_project_;
    shunya::core::domain::SimulationClock clock_;
    shunya::core::domain::CommandStack command_stack_{100};
};
