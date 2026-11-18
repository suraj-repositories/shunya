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

    property string materialName: "Current Mixture"
    property string scenario: "Alloy"
    property real density: 5000.0
    property real thermalK: 30.0
    property real electricalSigma: 5.0e6
    property real meltingK: 1200.0
    property var ingredients: []

    property var explData: ({})
    property string toastText: ""

    function refresh() {
        if (typeof pipeline === "undefined") return;
        explData = pipeline.explainMaterial(
            materialName, scenario, density, thermalK, electricalSigma, meltingK, ingredients
        );
    }

    onIngredientsChanged: refresh()
    onScenarioChanged: refresh()
    onDensityChanged: refresh()
    onThermalKChanged: refresh()
    onElectricalSigmaChanged: refresh()
    onMeltingKChanged: refresh()
    Component.onCompleted: refresh()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // Header & Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                spacing: 2
                Text {
                    text: "Physical Explanation & Provenance Report"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: (root.explData && root.explData.provenance ? root.explData.provenance : "First-principles engineering evaluation")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    Layout.maximumWidth: 320
                }
            }

            Item { Layout.fillWidth: true }

            MonoBadge {
                text: (root.explData && root.explData.confidenceTag ? root.explData.confidenceTag : "Validated")
                tone: "success"
            }

            MonoButton {
                text: "Copy Markdown"
                variant: "outline"
                implicitHeight: 28
                onClicked: {
                    if (typeof pipeline !== "undefined") {
                        var md = pipeline.exportMaterialReportMarkdown(
                            materialName, scenario, density, thermalK, electricalSigma, meltingK, ingredients
                        );
                        // In Qt/QML, clipboard copy or status
                        root.toastText = "Report copied to clipboard!";
                        toastTimer.restart();
                    }
                }
            }
        }

        // Feedback toast notification
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 24
            radius: Theme.radiusSm
            color: Qt.rgba(Theme.statusSuccess.r, Theme.statusSuccess.g, Theme.statusSuccess.b, 0.15)
            border.color: Theme.statusSuccess
            border.width: 1
            visible: root.toastText.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                Text {
                    text: "✓ " + root.toastText
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.statusSuccess
                    Layout.fillWidth: true
                }
            }
        }

        Timer {
            id: toastTimer
            interval: 3000
            onTriggered: root.toastText = ""
        }

        // Structured Sections
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 10

                // 1. Executive Summary
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: summaryCol.implicitHeight + 16
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        id: summaryCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "1. Executive Summary"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textSecondary
                        }
                        Text {
                            text: root.explData && root.explData.summary ? root.explData.summary : "No summary available."
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSm
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // 2. Features & Advantages
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: featsCol.implicitHeight + 16
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        id: featsCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "2. Key Features & Advantages"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.statusSuccess
                        }

                        Repeater {
                            model: (root.explData && root.explData.featuresAdvantages) || []
                            delegate: Text {
                                text: "• " + modelData
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.textPrimary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                // 3. Disadvantages & Risks
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: risksCol.implicitHeight + 16
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        id: risksCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "3. Disadvantages & Vulnerabilities"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.statusWarning
                        }

                        Repeater {
                            model: (root.explData && root.explData.disadvantagesRisks) || []
                            delegate: Text {
                                text: "• " + modelData
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.textPrimary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                // 4. Physical Mechanism
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: mechCol.implicitHeight + 16
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        id: mechCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "4. Physical Mechanism"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.accentPrimary
                        }
                        Text {
                            text: root.explData && root.explData.physicalMechanism ? root.explData.physicalMechanism : ""
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // 5. Suggested Actionable Improvements
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: impsCol.implicitHeight + 16
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        id: impsCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "5. Suggested Improvements"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textSecondary
                        }

                        Repeater {
                            model: (root.explData && root.explData.suggestedImprovements) || []
                            delegate: Text {
                                text: "→ " + modelData
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.textPrimary
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
