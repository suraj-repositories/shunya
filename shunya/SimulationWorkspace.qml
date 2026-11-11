import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import shunya

Rectangle {
    id: root
    color: Theme.bgCanvas

    property var bodiesList: []
    property var envCfg: ({})
    property double tempInputK: 298.15
    property double pressureAtm: 1.0
    property double gravityMS2: 9.80665
    property double convectionH: 10.0
    property double injectedWatts: 35.0
    property int selectedBodyIdx: 0
    property int plotTab: 0 // 0 = Telemetry Plot, 1 = Live Oscilloscope

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        bodiesList = pipeline.getPhysicsBodies();
        envCfg = pipeline.getEnvironmentConfig();
        tempInputK = envCfg.temperatureK || 298.15;
        pressureAtm = (envCfg.pressurePa || 101325.0) / 101325.0;
        gravityMS2 = envCfg.gravityMS2 || 9.80665;
        convectionH = envCfg.convectionH || 10.0;
        if (phys3d) phys3d.refreshScene();

        var hist = pipeline.telemetryHistory || [];
        if (hist.length > 0 && simPlot && simPlot.plotItem) {
            var kePts = [], tempPts = [], pePts = [];
            for (var i = 0; i < hist.length; ++i) {
                kePts.push({"x": hist[i].timeS, "y": hist[i].kineticEnergyJ});
                pePts.push({"x": hist[i].timeS, "y": hist[i].potentialEnergyJ});
                tempPts.push({"x": hist[i].timeS, "y": hist[i].avgTempC});
            }
            simPlot.plotItem.setMultiSeries([
                {"name": "Kinetic Energy (J)", "color": Theme.chartColors.velocity, "points": kePts},
                {"name": "Potential Energy (J)", "color": Theme.chartColors.voltage, "points": pePts},
                {"name": "Avg Temp (C)", "color": Theme.chartColors.temperature, "points": tempPts}
            ]);
        }
    }

    Component.onCompleted: refreshAll()
    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onPhysicsChanged() { root.refreshAll(); }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12; spacing: 10

        // Simulation Execution Transport Bar
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Text { text: "Multiphysics Simulation & Telemetry"; font.family: Theme.fontSans; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold; color: Theme.textPrimary }
            MonoBadge {
                text: typeof studio !== "undefined" ? studio.simStatus : "Stopped"
                tone: (typeof studio !== "undefined" && studio.simStatus === "Running") ? "success" : "neutral"
            }
            Text {
                text: "t = " + (typeof studio !== "undefined" ? studio.simTime.toFixed(3) : "0.000") + " s"
                font.family: Theme.fontMono; font.pixelSize: Theme.fontMd; font.weight: Font.Bold; color: Theme.textPrimary
            }
            Item { Layout.fillWidth: true }
            MonoButton { text: "Run"; iconName: "play"; variant: "primary"; compact: true; onClicked: studio.startSimulation() }
            MonoButton { text: "Pause"; iconName: "pause"; compact: true; onClicked: studio.pauseSimulation() }
            MonoButton { text: "Step"; iconName: "step"; compact: true; onClicked: studio.stepSimulation() }
            MonoButton { text: "Stop"; iconName: "stop"; compact: true; onClicked: studio.stopSimulation() }
            MonoButton { text: "Reset"; iconName: "reset"; compact: true; onClicked: studio.resetSimulation() }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10

            // LEFT: Environment & Stimulus Controls
            Rectangle {
                Layout.preferredWidth: 320; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    Text { text: "Environment Conditions"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }

                    // Atmospheric / Vacuum Presets
                    GridLayout {
                        columns: 4; columnSpacing: 4; rowSpacing: 4; Layout.fillWidth: true
                        MonoButton {
                            Layout.fillWidth: true; text: "STP"; compact: true
                            onClicked: { root.tempInputK = 298.15; root.pressureAtm = 1.0; root.convectionH = 10.0; pipeline.setEnvironmentConfig(298.15, 101325.0, 9.80665, 10.0, true); }
                        }
                        MonoButton {
                            Layout.fillWidth: true; text: "Vacuum"; compact: true
                            onClicked: { root.tempInputK = 298.15; root.pressureAtm = 1e-8; root.convectionH = 0.01; pipeline.setEnvironmentConfig(298.15, 1e-3, 9.80665, 0.01, false); }
                        }
                        MonoButton {
                            Layout.fillWidth: true; text: "Arctic"; compact: true
                            onClicked: { root.tempInputK = 223.15; root.pressureAtm = 1.0; root.convectionH = 22.0; pipeline.setEnvironmentConfig(223.15, 101325.0, 9.80665, 22.0, true); }
                        }
                        MonoButton {
                            Layout.fillWidth: true; text: "High-T"; compact: true
                            onClicked: { root.tempInputK = 473.15; root.pressureAtm = 2.0; root.convectionH = 25.0; pipeline.setEnvironmentConfig(473.15, 202650.0, 9.80665, 25.0, true); }
                        }
                    }

                    GridLayout {
                        columns: 2; columnSpacing: 8; rowSpacing: 6; Layout.fillWidth: true
                        Text { text: "Ambient T (K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.tempInputK.toFixed(2); onEditingFinished: root.tempInputK = parseFloat(text) || 298.15 }
                        Text { text: "Pressure (atm):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.pressureAtm.toFixed(2); onEditingFinished: root.pressureAtm = parseFloat(text) || 1.0 }
                        Text { text: "Gravity (m/s2):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.gravityMS2.toFixed(3); onEditingFinished: root.gravityMS2 = parseFloat(text) || 9.80665 }
                        Text { text: "Convection h:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.convectionH.toFixed(1); onEditingFinished: root.convectionH = parseFloat(text) || 10.0 }
                    }
                    MonoButton {
                        Layout.fillWidth: true; text: "Apply Environment"; compact: true
                        onClicked: pipeline.setEnvironmentConfig(root.tempInputK, root.pressureAtm * 101325.0, root.gravityMS2, root.convectionH, true)
                    }

                    Text { text: "Simulated Rigid & Thermal Bodies (" + root.bodiesList.length + ")"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                        model: root.bodiesList
                        delegate: Rectangle {
                            width: ListView.view.width; height: 64; radius: Theme.radiusSm
                            color: index === root.selectedBodyIdx ? Theme.bgActive : Theme.bgCanvas
                            border.color: modelData.collisionActive ? Theme.statusWarning : Theme.borderSubtle; border.width: 1
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 2
                                RowLayout {
                                    Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.fillWidth: true; elide: Text.ElideRight }
                                    MonoBadge { visible: modelData.collisionActive; text: "Contact"; tone: "warning" }
                                }
                                Text { text: "Y=" + Number(modelData.posY).toFixed(3) + " m  Vy=" + Number(modelData.velY).toFixed(2) + " m/s"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textMuted }
                                Text { text: "T=" + Number(modelData.tempC).toFixed(2) + " C  strain=" + Number(modelData.thermalStrain).toExponential(2); font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textSecondary }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.selectedBodyIdx = index }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        enabled: root.selectedBodyIdx >= 0 && root.selectedBodyIdx < root.bodiesList.length
                        MonoButton { text: "Heat +50W"; compact: true; onClicked: pipeline.setBodyHeatSource(root.bodiesList[root.selectedBodyIdx].objectId, 50.0) }
                        MonoButton { text: "Up Force"; compact: true; onClicked: pipeline.applyBodyForce(root.bodiesList[root.selectedBodyIdx].objectId, 0, 5.0, 0) }
                        MonoButton { text: "Cool"; compact: true; onClicked: pipeline.setBodyHeatSource(root.bodiesList[root.selectedBodyIdx].objectId, 0.0) }
                    }
                }
            }

            // RIGHT: Split 3D Thermal/Physics Viewport + Live Multi-Color Graphs
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10

                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: Theme.bgPrimary; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd
                    clip: true

                    Viewport3DItem {
                        id: phys3d
                        anchors.fill: parent
                        darkMode: Theme.isDarkMode
                        physicsActive: true
                        showThermalOverlay: true
                        MouseArea {
                            anchors.fill: parent
                            property double lx: 0; property double ly: 0
                            onPressed: (m) => { lx = m.x; ly = m.y; }
                            onPositionChanged: (m) => { phys3d.orbitBy((m.x - lx) * 0.5, (m.y - ly) * 0.5); lx = m.x; ly = m.y; }
                            onWheel: (w) => phys3d.zoomBy(w.angleDelta.y > 0 ? 0.9 : 1.1)
                        }
                    }
                    MonoBadge { anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 10; text: "Live 3D Physics & Thermal Field"; tone: "neutral" }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 280
                    color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 8; spacing: 6
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            MonoButton { text: "Multiphysics Plot"; compact: true; active: root.plotTab === 0; onClicked: root.plotTab = 0 }
                            MonoButton { text: "Oscilloscope"; compact: true; active: root.plotTab === 1; onClicked: root.plotTab = 1 }
                            Item { Layout.fillWidth: true }
                        }
                        ScientificPlot {
                            id: simPlot
                            visible: root.plotTab === 0
                            Layout.fillWidth: true; Layout.fillHeight: true
                            title: "Energy & Thermal Response"
                            xLabel: "Time (s)"
                            yLabel: "J / C"
                        }
                        OscilloscopePanel {
                            visible: root.plotTab === 1
                            Layout.fillWidth: true; Layout.fillHeight: true
                        }
                    }
                }
            }
        }
    }
}
