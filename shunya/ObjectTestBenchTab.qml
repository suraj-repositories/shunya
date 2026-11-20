import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var pipeline: typeof pipelineController !== "undefined" ? pipelineController : null
    property string objectId: ""
    property string activePreset: "Standard_Room"
    property double appliedVoltage: 120.0
    property double ambientTempK: 298.15
    property double appliedForceN: 0.0
    property double ambientPressurePa: 101325.0

    property var reportData: null
    property bool isRunning: false

    function runQualification() {
        if (!pipeline) return;
        isRunning = true;
        reportData = pipeline.runObjectTestBench(
            objectId,
            activePreset,
            appliedVoltage,
            ambientTempK,
            appliedForceN,
            ambientPressurePa
        );
        isRunning = false;
    }

    Component.onCompleted: {
        runQualification();
    }

    onObjectIdChanged: {
        runQualification();
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 12

            // =========================================================================
            // Stimuli & Environment Configuration Card
            // =========================================================================
            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusMd
                color: Theme.bgSurface
                border.color: Theme.borderSubtle
                border.width: 1
                implicitHeight: configCol.implicitHeight + 24

                ColumnLayout {
                    id: configCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "TEST BENCH STIMULI & PRESETS"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.bold: true
                            color: Theme.textMuted
                            Layout.fillWidth: true
                        }

                        MonoInfoButton {
                            title: "Multiphysics Object Test Bench"
                            description: "Subject 3D physical objects with terminals and cavities to coupled Electrical, Thermal, Mechanical, and Pressure qualification tests."
                            formula: "P = V²/R = I²R,  q = k*A*ΔT/L,  σ = F/A"
                            parameterRange: "V: 0-1000V, T: 4-3000K, F: 0-100kN"
                            helpAnchor: "virtual-test-bench"
                        }

                        MonoBadge {
                            text: activePreset.replace("_", " ")
                            color: Theme.accentPrimary
                        }
                    }

                    // Presets Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Repeater {
                            model: [
                                { id: "Standard_Room", label: "Room (120V)" },
                                { id: "Spacecraft_LEO", label: "Space LEO (28V)" },
                                { id: "High_Voltage_Step", label: "Step (240V)" },
                                { id: "Lunar_Surface", label: "Lunar Daytime" }
                            ]
                            delegate: MonoButton {
                                text: modelData.label
                                compact: true
                                active: root.activePreset === modelData.id
                                onClicked: {
                                    root.activePreset = modelData.id;
                                    if (modelData.id === "Standard_Room") {
                                        root.appliedVoltage = 120.0; root.ambientTempK = 298.15; root.ambientPressurePa = 101325.0; root.appliedForceN = 0.0;
                                    } else if (modelData.id === "Spacecraft_LEO") {
                                        root.appliedVoltage = 28.0; root.ambientTempK = 250.0; root.ambientPressurePa = 1e-4; root.appliedForceN = 0.0;
                                    } else if (modelData.id === "High_Voltage_Step") {
                                        root.appliedVoltage = 240.0; root.ambientTempK = 300.0; root.ambientPressurePa = 101325.0; root.appliedForceN = 5.0;
                                    } else if (modelData.id === "Lunar_Surface") {
                                        root.appliedVoltage = 120.0; root.ambientTempK = 380.0; root.ambientPressurePa = 1e-9; root.appliedForceN = 0.0;
                                    }
                                    root.runQualification();
                                }
                            }
                        }
                    }

                    // Numeric stimulus inputs
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 6
                        columnSpacing: 8

                        Text { text: "Applied Voltage (V):"; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; Layout.alignment: Qt.AlignVCenter }
                        MonoInput {
                            Layout.fillWidth: true
                            text: root.appliedVoltage.toString()
                            onEditingFinished: { root.appliedVoltage = parseFloat(text) || 120.0; root.runQualification(); }
                        }

                        Text { text: "Ambient Temp (K):"; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; Layout.alignment: Qt.AlignVCenter }
                        MonoInput {
                            Layout.fillWidth: true
                            text: root.ambientTempK.toString()
                            onEditingFinished: { root.ambientTempK = parseFloat(text) || 298.15; root.runQualification(); }
                        }

                        Text { text: "Ambient Pressure (Pa):"; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; Layout.alignment: Qt.AlignVCenter }
                        MonoInput {
                            Layout.fillWidth: true
                            text: root.ambientPressurePa.toString()
                            onEditingFinished: { root.ambientPressurePa = parseFloat(text) || 101325.0; root.runQualification(); }
                        }

                        Text { text: "Applied Load (N):"; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; Layout.alignment: Qt.AlignVCenter }
                        MonoInput {
                            Layout.fillWidth: true
                            text: root.appliedForceN.toString()
                            onEditingFinished: { root.appliedForceN = parseFloat(text) || 0.0; root.runQualification(); }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        MonoButton {
                            text: root.isRunning ? "Running..." : "Run Qualification Bench"
                            iconName: "play"
                            primary: true
                            Layout.fillWidth: true
                            onClicked: root.runQualification()
                        }

                        MonoButton {
                            text: "Export Dossier (MD)"
                            iconName: "export"
                            onClicked: {
                                if (pipeline && root.reportData) {
                                    var md = root.reportData.markdown || "";
                                    pipeline.pushStatusMessage("Exported qualification dossier for " + (root.reportData.objectName || "object") + ".");
                                }
                            }
                        }
                    }
                }
            }

            // =========================================================================
            // Qualification Summary Scorecard Banner
            // =========================================================================
            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusMd
                color: root.reportData && root.reportData.overallPassed ? Theme.bgSurface : Theme.bgElevated
                border.color: root.reportData && root.reportData.overallPassed ? Theme.textSuccess : Theme.textDanger
                border.width: 1
                implicitHeight: summaryRow.implicitHeight + 20

                RowLayout {
                    id: summaryRow
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        width: 36
                        height: 36
                        radius: Theme.radiusSm
                        color: root.reportData && root.reportData.overallPassed ? Theme.statusSuccessBg : Theme.statusDangerBg

                        Text {
                            anchors.centerIn: parent
                            text: root.reportData && root.reportData.overallPassed ? "✓" : "!"
                            font.pixelSize: 18
                            font.bold: true
                            color: root.reportData && root.reportData.overallPassed ? Theme.textSuccess : Theme.textDanger
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: root.reportData && root.reportData.overallPassed ? "MULTIPHYSICS QUALIFICATION: PASSED" : "MULTIPHYSICS QUALIFICATION: FAILED / WARNINGS"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSm
                            font.bold: true
                            color: root.reportData && root.reportData.overallPassed ? Theme.textSuccess : Theme.textDanger
                        }

                        Text {
                            text: root.reportData
                                ? ("Composite Score: " + root.reportData.compositeScore.toFixed(1) + "% across 5 physics domains")
                                : "No qualification data computed."
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textSecondary
                        }
                    }
                }
            }

            // =========================================================================
            // Domain Scorecards List
            // =========================================================================
            Text {
                text: "DOMAIN SCORECARDS & GOVERNING EQUATIONS"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                font.bold: true
                color: Theme.textMuted
                Layout.topMargin: 4
            }

            Repeater {
                model: root.reportData ? root.reportData.domains : []
                delegate: Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusSm
                    color: Theme.bgSurface
                    border.color: Theme.borderSubtle
                    border.width: 1
                    implicitHeight: cardCol.implicitHeight + 16

                    ColumnLayout {
                        id: cardCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: modelData.name.toUpperCase()
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontSm
                                font.bold: true
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }

                            MonoBadge {
                                text: modelData.passed ? "PASS" : "FAIL"
                                color: modelData.passed ? Theme.textSuccess : Theme.textDanger
                            }

                            Text {
                                text: modelData.score.toFixed(0) + "%"
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontXs
                                font.bold: true
                                color: Theme.accentPrimary
                            }
                        }

                        Text {
                            text: modelData.metricSummary
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontXs
                            color: Theme.textPrimary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Model: " + modelData.physicalModel
                            font.family: Theme.fontSans
                            font.pixelSize: 10
                            color: Theme.textMuted
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Provenance: " + modelData.provenance
                            font.family: Theme.fontSans
                            font.pixelSize: 10
                            color: Theme.textSecondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        // Warnings / Failures display
                        Repeater {
                            model: modelData.warnings || []
                            delegate: Text {
                                text: "⚠ " + modelData
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.textWarning
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }

                        Repeater {
                            model: modelData.failures || []
                            delegate: Text {
                                text: "✖ " + modelData
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.textDanger
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            // =========================================================================
            // Plain-Language Recommendations Card
            // =========================================================================
            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusSm
                color: Theme.bgElevated
                border.color: Theme.borderSubtle
                border.width: 1
                implicitHeight: recCol.implicitHeight + 16

                ColumnLayout {
                    id: recCol
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    Text {
                        text: "ENGINEERING RECOMMENDATIONS"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        font.bold: true
                        color: Theme.accentPrimary
                    }

                    Repeater {
                        model: root.reportData ? root.reportData.recommendations : []
                        delegate: Text {
                            text: "• " + modelData
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textPrimary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }
}
