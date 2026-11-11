#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "shunya/eng/engineering_pipeline.hpp"

namespace shunya::gui {

class ShunyaOscilloscopeController;

/**
 * @brief Unified QML controller bridging EngineeringWorkspaceState to every Phase 3 workspace:
 *        Elements CSV & Behavior, Mixing Laboratory, Materials, Objects, Components,
 *        Battery Engineering, Interactive Circuit Editor & Simulator, Physics & Settings.
 */
class ShunyaPipelineController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString activeWorkspace READ activeWorkspace WRITE setActiveWorkspace NOTIFY activeWorkspaceChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY workspaceChanged)
    Q_PROPERTY(QString projectAuthor READ projectAuthor NOTIFY workspaceChanged)
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY workspaceChanged)
    Q_PROPERTY(int elementCount READ elementCount NOTIFY elementDbChanged)
    Q_PROPERTY(int materialCount READ materialCount NOTIFY workspaceChanged)
    Q_PROPERTY(int experimentCount READ experimentCount NOTIFY workspaceChanged)
    Q_PROPERTY(int objectCount READ objectCount NOTIFY workspaceChanged)
    Q_PROPERTY(int componentCount READ componentCount NOTIFY workspaceChanged)
    Q_PROPERTY(int batteryCount READ batteryCount NOTIFY workspaceChanged)
    Q_PROPERTY(int circuitCount READ circuitCount NOTIFY workspaceChanged)
    Q_PROPERTY(int activeCircuitIndex READ activeCircuitIndex WRITE selectCircuit NOTIFY circuitChanged)
    Q_PROPERTY(bool canCircuitUndo READ canCircuitUndo NOTIFY circuitChanged)
    Q_PROPERTY(bool canCircuitRedo READ canCircuitRedo NOTIFY circuitChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QVariantList telemetryHistory READ telemetryHistory NOTIFY physicsChanged)

public:
    explicit ShunyaPipelineController(
        std::shared_ptr<eng::EngineeringWorkspaceState> workspace,
        QObject* parent = nullptr);
    ~ShunyaPipelineController() override = default;

    void setOscilloscopeController(ShunyaOscilloscopeController* osc) { osc_ = osc; }
    void emitAllChanged();

    QString activeWorkspace() const { return active_workspace_; }
    Q_INVOKABLE void setActiveWorkspace(const QString& ws);

    QString projectName() const;
    QString projectAuthor() const;
    bool isDirty() const;
    int elementCount() const;
    int materialCount() const;
    int experimentCount() const;
    int objectCount() const;
    int componentCount() const;
    int batteryCount() const;
    int circuitCount() const;
    int activeCircuitIndex() const;
    bool canCircuitUndo() const;
    bool canCircuitRedo() const;
    QString statusMessage() const { return status_message_; }
    QVariantList telemetryHistory() const { return telemetry_history_; }

    // =========================================================================
    // 1. Chemical Elements & Natural Behavior (ELEMDB + MATBEH)
    // =========================================================================
    Q_INVOKABLE QVariantList queryElements(
        const QString& searchText = "",
        const QString& phaseFilter = "",
        const QString& blockFilter = "",
        const QString& elecFilter = "",
        const QString& sortBy = "AtomicNumber",
        bool descending = false) const;

    Q_INVOKABLE QVariantMap getElementDetails(int atomicNumber) const;
    Q_INVOKABLE QVariantMap evaluateElementBehavior(
        int atomicNumber, double tempK, double pressurePa, bool oxidizing) const;

    // =========================================================================
    // 2. Materials & Mixing Laboratory (MIXLAB + MATPROP)
    // =========================================================================
    Q_INVOKABLE QVariantList getMaterials() const;
    Q_INVOKABLE QVariantList getMixtureExperiments() const;
    Q_INVOKABLE QVariantList getStandardMixturePresets() const;

    Q_INVOKABLE QVariantMap evaluateMixture(
        const QString& name,
        const QString& scenario,
        double tempK,
        double pressurePa,
        bool oxidizing,
        double densityOverride,
        const QVariantList& inputs) const;

    Q_INVOKABLE QVariantMap saveAndPromoteMixture(
        const QString& expId,
        const QString& name,
        const QString& scenario,
        double tempK,
        double pressurePa,
        bool oxidizing,
        double densityOverride,
        const QVariantList& inputs,
        bool alsoPromoteToMaterial);

    Q_INVOKABLE QVariantMap updateMaterialParameters(
        const QString& materialId,
        double densityKgM3,
        double thermalK,
        double specificHeat,
        double electricalSigma);

    Q_INVOKABLE QVariantList sweepMixtureTemperature(
        const QString& scenario,
        double pressurePa,
        bool oxidizing,
        const QVariantList& inputs,
        double tMinK,
        double tMaxK,
        int steps) const;

    // =========================================================================
    // 3. Physical Objects (OBJMAT + OBJGEO)
    // =========================================================================
    Q_INVOKABLE QVariantList getObjects() const;
    Q_INVOKABLE QVariantMap createObject(
        const QString& name,
        const QString& category,
        const QString& shape,
        double dimX, double dimY, double dimZ, double radius,
        const QString& materialId,
        double tempK = 298.15,
        double pressurePa = 101325.0);

    Q_INVOKABLE QVariantMap addPresetObject(const QString& presetName);
    Q_INVOKABLE QVariantMap updateObject(
        const QString& objectId,
        const QString& name,
        const QString& shape,
        double dimX, double dimY, double dimZ, double radius,
        double tempK, double pressurePa,
        const QVariantList& regions);
    Q_INVOKABLE QVariantMap updateObjectTransform(
        const QString& objectId,
        double px, double py, double pz,
        double rx, double ry, double rz,
        double sx, double sy, double sz);
    Q_INVOKABLE bool duplicateObject(const QString& objectId);
    Q_INVOKABLE bool deleteObject(const QString& objectId);

    // =========================================================================
    // 4. Functional Circuit Components (OBJCMP)
    // =========================================================================
    Q_INVOKABLE QVariantList getComponents() const;
    Q_INVOKABLE QVariantMap previewComponentFromObject(
        const QString& objectId,
        const QString& kind,
        const QVariantMap& cfg) const;
    Q_INVOKABLE QVariantMap generateComponentFromObject(
        const QString& name,
        const QString& objectId,
        const QString& kind,
        const QVariantMap& cfg);
    Q_INVOKABLE bool deleteComponent(const QString& compId);

    // =========================================================================
    // 5. Battery Engineering & Simulation (BAT)
    // =========================================================================
    Q_INVOKABLE QVariantList getBatteries() const;
    Q_INVOKABLE QVariantMap getDefaultBatterySpecMap() const;
    Q_INVOKABLE QVariantMap evaluateBatteryDesign(const QVariantMap& specMap) const;
    Q_INVOKABLE QVariantMap saveAndExportBattery(const QVariantMap& specMap);
    Q_INVOKABLE QVariantMap simulateBatteryChargeDischarge(
        const QVariantMap& specMap,
        double dischargeCRate,
        double dischargeDurationS,
        double chargeCRate,
        double chargeDurationS,
        double dtS = 2.0) const;

    // =========================================================================
    // 6. Interactive Circuit Workspace & Simulation (CIRUI + CIRSIM)
    // =========================================================================
    Q_INVOKABLE QVariantList getCircuits() const;
    Q_INVOKABLE void selectCircuit(int index);
    Q_INVOKABLE void newCircuit(const QString& name);
    Q_INVOKABLE void duplicateActiveCircuit(const QString& newName);

    Q_INVOKABLE QVariantMap getActiveCircuitSnapshot() const;
    Q_INVOKABLE void placeCircuitComponent(
        const QString& kind,
        const QString& label,
        const QString& generatedCompId,
        double xMm, double yMm,
        double primaryVal,
        double secondaryVal = 0.0,
        double tertiaryVal = 0.0);
    Q_INVOKABLE void moveCircuitComponent(const QString& compId, double xMm, double yMm);
    Q_INVOKABLE void rotateCircuitComponent(const QString& compId, int deltaDeg = 90);
    Q_INVOKABLE void deleteCircuitComponent(const QString& compId);
    Q_INVOKABLE void setCircuitComponentValue(const QString& compId, double primaryVal);
    Q_INVOKABLE void toggleCircuitSwitch(const QString& compId);
    Q_INVOKABLE void addCircuitWire(const QVariantList& pointsMm);
    Q_INVOKABLE void deleteCircuitWire(const QString& wireId);
    Q_INVOKABLE void addCircuitJunction(double xMm, double yMm);
    Q_INVOKABLE void circuitUndo();
    Q_INVOKABLE void circuitRedo();

    Q_INVOKABLE QVariantMap runCircuitDc();
    Q_INVOKABLE QVariantMap runCircuitTransient(double durationS, double dtS);

    // =========================================================================
    // 7. Physics & Environmental Simulation (PHYSIM)
    // =========================================================================
    Q_INVOKABLE QVariantMap getEnvironmentConfig() const;
    Q_INVOKABLE void setEnvironmentConfig(
        double tempK, double pressurePa, double gravityMS2, double convectionH, bool oxidizing);
    Q_INVOKABLE QVariantList getPhysicsBodies() const;
    Q_INVOKABLE void initializePhysics();
    Q_INVOKABLE QVariantMap stepPhysics(double dtS);
    Q_INVOKABLE void resetPhysics();
    Q_INVOKABLE void applyBodyForce(const QString& objectId, double fx, double fy, double fz);
    Q_INVOKABLE void setBodyHeatSource(const QString& objectId, double watts);
    Q_INVOKABLE void setBodyStatic(const QString& objectId, bool isStatic);

    // =========================================================================
    // 8. Project Lifecycle & Settings (PERSIST)
    // =========================================================================
    Q_INVOKABLE void updateProjectMetadata(const QString& name, const QString& author, const QString& desc);
    Q_INVOKABLE QVariantMap saveProjectBundle(const QString& path);
    Q_INVOKABLE QVariantMap loadProjectBundle(const QString& path);
    Q_INVOKABLE QVariantMap seedLithiumBatteryDemo();
    Q_INVOKABLE QVariantMap validateWorkspaceReport() const;

signals:
    void activeWorkspaceChanged();
    void workspaceChanged();
    void elementDbChanged();
    void circuitChanged();
    void physicsChanged();
    void statusMessageChanged();

private:
    void pushStatus(const QString& msg);
    eng::BatteryDesignSpec specFromVariantMap(const QVariantMap& m) const;
    std::vector<mat::MixtureConstituentInput> inputsFromVariantList(const QVariantList& list) const;

    std::shared_ptr<eng::EngineeringWorkspaceState> ws_;
    ShunyaOscilloscopeController* osc_{nullptr};
    QString active_workspace_{"elements"};
    QString status_message_{"Ready"};
    QVariantList telemetry_history_;
    QVariantMap last_dc_result_;
};

} // namespace shunya::gui
