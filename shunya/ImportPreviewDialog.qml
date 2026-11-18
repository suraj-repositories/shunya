import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Dialog {
    id: root
    title: "Import Mixture Experiment Preview"
    width: 580
    height: 440
    modal: true
    anchors.centerIn: parent

    background: Rectangle {
        color: Theme.bgSurface
        border.color: Theme.borderDefault
        border.width: 1
        radius: Theme.radiusMd
    }

    property var previewData: ({})
    property string filePath: ""

    signal importConfirmed(var expData, string conflictStrategy)

    function loadPreview(path) {
        root.filePath = path;
        if (typeof pipeline !== "undefined") {
            root.previewData = pipeline.importExperimentFile(path);
        }
        root.open();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // File metadata
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: metaCol.implicitHeight + 16
            radius: Theme.radiusSm
            color: Theme.bgElevated
            border.color: Theme.borderDefault
            border.width: 1

            ColumnLayout {
                id: metaCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    spacing: 6
                    Text {
                        text: root.previewData && root.previewData.name ? root.previewData.name : "Unnamed Recipe"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontBase
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    MonoBadge {
                        text: root.previewData && root.previewData.scenario ? root.previewData.scenario : "Alloy"
                        tone: "neutral"
                    }
                    MonoBadge {
                        text: "v" + (root.previewData && root.previewData.schemaVersion ? root.previewData.schemaVersion : "5.0.0")
                        tone: "success"
                    }
                }

                Text {
                    text: "Author: " + (root.previewData && root.previewData.author ? root.previewData.author : "Unknown")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }

                Text {
                    text: "T = " + (root.previewData && root.previewData.temperatureK ? root.previewData.temperatureK : 298.15) + " K,  P = " + (root.previewData && root.previewData.pressurePa ? (root.previewData.pressurePa/1000.0).toFixed(1) : 101.3) + " kPa"
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontXs
                    color: Theme.textSecondary
                }
            }
        }

        // Warnings banner if unknown elements or schema upgrade
        Repeater {
            model: (root.previewData && root.previewData.warnings) || []
            delegate: Rectangle {
                Layout.fillWidth: true
                implicitHeight: 28
                radius: Theme.radiusSm
                color: Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.12)
                border.color: Theme.statusWarning
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
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

        // Constituents Summary Table
        Text {
            text: "Constituents Summary:"
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 4

                Repeater {
                    model: (root.previewData && root.previewData.inputs) || []
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 26
                        radius: Theme.radiusSm
                        color: Theme.bgElevated
                        border.color: Theme.borderSubtle
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8

                            Text {
                                text: modelData.symbol_or_id
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.Bold
                                color: Theme.accentPrimary
                                Layout.preferredWidth: 60
                            }

                            Text {
                                text: Number(modelData.amount).toFixed(2) + " % (" + modelData.basis + ")"
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontXs
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }

        // Action Buttons with Conflict Handling
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MonoButton {
                text: "Cancel"
                variant: "outline"
                onClicked: root.close()
            }

            Item { Layout.fillWidth: true }

            MonoButton {
                text: "Keep Both (Auto-Rename)"
                variant: "outline"
                onClicked: {
                    root.importConfirmed(root.previewData, "keep_both");
                    root.close();
                }
            }

            MonoButton {
                text: "Import & Load"
                onClicked: {
                    root.importConfirmed(root.previewData, "replace");
                    root.close();
                }
            }
        }
    }
}
