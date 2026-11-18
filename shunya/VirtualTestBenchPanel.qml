import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Rectangle {
    id: root
    color: Theme.bgSurface
    border.color: Theme.borderDefault
    border.width: 1
    radius: Theme.radiusMd
    clip: true

    property string expName: ""
    property string scenario: "Alloy"
    property double tempK: 298.15
    property double pressurePa: 101325.0
    property bool oxidizing: true
    property double densityOverride: 0.0
    property var ingredients: []

    property string selectedPreset: "Spacecraft (LEO)"
    property var qualificationReport: ({})
    property bool hasRun: false

    function runTestBench() {
        if (typeof pipeline === "undefined") return;
        qualificationReport = pipeline.qualifyMixtureTestBench(
            expName, scenario, tempK, pressurePa, oxidizing, densityOverride, ingredients, selectedPreset);
        hasRun = true;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Header Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ColumnLayout {
                spacing: 2
                Text {
                    text: "Virtual Material Test Bench"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: "7 Environmental Qualification Domains & Gating"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }
            }
            Item { Layout.fillWidth: true }
            MonoButton {
                text: "Run Qualification"
                variant: "primary"
                compact: true
                iconName: "play"
                onClicked: root.runTestBench()
            }
        }

        // Environment Preset Selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                text: "Environment:"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                color: Theme.textMuted
            }
            MonoComboBox {
                id: envCombo
                Layout.fillWidth: true
                model: [
                    "Spacecraft (LEO)",
                    "Deep Sea Submersible",
                    "Industrial Furnace",
                    "Underground Power Cable",
                    "Outdoor Coastal Weathering"
                ]
                onActivated: (index, val) => {
                    root.selectedPreset = val;
                    if (root.hasRun) root.runTestBench();
                }
            }
        }

        // Overall Scorecard Banner (When Evaluated)
        Rectangle {
            visible: root.hasRun && root.qualificationReport.ok
            Layout.fillWidth: true
            height: 38
            radius: Theme.radiusSm
            color: root.qualificationReport.overallPassed
                ? Qt.rgba(Theme.statusSuccess.r, Theme.statusSuccess.g, Theme.statusSuccess.b, 0.12)
                : Qt.rgba(Theme.statusError.r, Theme.statusError.g, Theme.statusError.b, 0.12)
            border.color: root.qualificationReport.overallPassed ? Theme.statusSuccess : Theme.statusError
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                MonoBadge {
                    text: root.qualificationReport.overallPassed ? "QUALIFIED" : "DISQUALIFIED"
                    tone: root.qualificationReport.overallPassed ? "success" : "error"
                }
                Text {
                    text: "Score: " + Number(root.qualificationReport.aggregateScore || 0).toFixed(1) + "%"
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Bold
                    color: root.qualificationReport.overallPassed ? Theme.statusSuccess : Theme.statusError
                }
                Text {
                    text: root.qualificationReport.overallPassed
                        ? "Meets all operational thresholds."
                        : (root.qualificationReport.criticalFailures ? root.qualificationReport.criticalFailures.length + " critical failures detected." : "Thresholds violated.")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        // Domain Test Cards List
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 6

                Repeater {
                    model: (root.qualificationReport && root.qualificationReport.domainResults) || []
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: domainCol.implicitHeight + 14
                        color: Theme.bgCanvas
                        border.color: modelData.passed ? Theme.borderSubtle : Theme.statusError
                        border.width: 1
                        radius: Theme.radiusSm

                        ColumnLayout {
                            id: domainCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: modelData.name
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }
                                MonoBadge {
                                    text: modelData.passed ? "PASS" : "FAIL"
                                    tone: modelData.passed ? "success" : "error"
                                }
                                MonoBadge {
                                    text: Number(modelData.score).toFixed(0) + "%"
                                    tone: modelData.passed ? "neutral" : "warning"
                                }
                                MonoBadge {
                                    visible: modelData.isApproximate
                                    text: "Approximate"
                                    tone: "neutral"
                                }
                            }

                            // Metric Summary Line
                            Text {
                                text: modelData.metricSummary || ""
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontXs
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Physical Model & Provenance
                            Text {
                                text: (modelData.physicalModel || "") + " | " + (modelData.provenance || "")
                                font.family: Theme.fontSans
                                font.pixelSize: 10
                                color: Theme.textMuted
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Failure Reasons (if failed)
                            Repeater {
                                model: modelData.failureReasons || []
                                delegate: Text {
                                    text: "⚠️ " + modelData
                                    font.family: Theme.fontSans
                                    font.pixelSize: 10
                                    color: Theme.statusError
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
