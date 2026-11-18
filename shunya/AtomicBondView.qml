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

    property string scenario: "Alloy"
    property var ingredients: []
    property var bondReport: ({})

    function refresh() {
        if (typeof pipeline === "undefined") return;
        bondReport = pipeline.analyzeAtomicBonds(scenario, ingredients);
    }

    onScenarioChanged: refresh()
    onIngredientsChanged: refresh()
    Component.onCompleted: refresh()

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
                    text: "Atomic Bonding & Hume-Rothery Miscibility"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: "Electronegativity Delta, Lattice Isomorphism & Stoichiometry"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }
            }
            Item { Layout.fillWidth: true }
            MonoBadge {
                text: root.bondReport.primaryBondType || "Metallic"
                tone: "neutral"
            }
            MonoBadge {
                text: root.bondReport.overallMiscibility || "Complete Solid Solution"
                tone: root.bondReport.isCompatible ? "success" : "warning"
            }
        }

        // Scenario Compatibility Warning Banner (if mismatched)
        Repeater {
            model: (root.bondReport && root.bondReport.warnings) || []
            delegate: Rectangle {
                Layout.fillWidth: true
                implicitHeight: 30
                radius: Theme.radiusSm
                color: Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.12)
                border.color: Theme.statusWarning
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6
                    Text {
                        text: "⚠️ " + modelData
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        color: Theme.statusWarning
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Stoichiometric Suggestions (for Chemical Compounds)
        Repeater {
            model: (root.bondReport && root.bondReport.stoichiometricSuggestions) || []
            delegate: Rectangle {
                Layout.fillWidth: true
                height: 26
                radius: Theme.radiusSm
                color: Qt.rgba(Theme.statusSuccess.r, Theme.statusSuccess.g, Theme.statusSuccess.b, 0.10)
                border.color: Theme.statusSuccess
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6
                    Text {
                        text: "💡 Empirical suggestion: " + modelData
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontXs
                        color: Theme.statusSuccess
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Pairwise Bond & Miscibility Table
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 6

                Repeater {
                    model: (root.bondReport && root.bondReport.pairwise) || []
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: pairCol.implicitHeight + 14
                        color: Theme.bgCanvas
                        border.color: modelData.miscibility === "Immiscible / Phase-Separated" ? Theme.statusError : Theme.borderSubtle
                        border.width: 1
                        radius: Theme.radiusSm

                        ColumnLayout {
                            id: pairCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: modelData.elementA + " ↔ " + modelData.elementB
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.fontSm
                                    font.weight: Font.Bold
                                    color: Theme.textPrimary
                                }
                                MonoBadge {
                                    text: modelData.bondType
                                    tone: modelData.bondType === "Ionic" ? "warning" : "neutral"
                                }
                                MonoBadge {
                                    text: modelData.miscibility
                                    tone: modelData.miscibility === "Complete Solid Solution" ? "success" :
                                          (modelData.miscibility === "Immiscible / Phase-Separated" ? "error" : "warning")
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: "ΔEN = " + Number(modelData.deltaElectronegativity).toFixed(2) + "  |  Δr = " + Number(modelData.radiusDifferencePct).toFixed(1) + "%"
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textMuted
                                }
                            }

                            Text {
                                text: modelData.plainLanguageVerdict || ""
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.textSecondary
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
