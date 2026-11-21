import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Dialog {
    id: root
    title: "Import Physical Object Package"
    width: 580
    height: 520
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
    property string conflictResolution: "KeepBoth" // KeepBoth, ReplaceExisting, Cancel

    signal importConfirmed(string filePath, string conflictStrategy)

    function loadPreview(path) {
        root.filePath = path;
        if (typeof pipeline !== "undefined") {
            root.previewData = pipeline.previewObjectFile(path);
        }
        root.conflictResolution = "KeepBoth";
        root.open();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // Header Title
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Physical Object Package Ingestion"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontLg
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            MonoBadge {
                text: root.previewData && root.previewData.shapeType ? root.previewData.shapeType : "Custom"
                tone: "info"
            }
        }

        // Preview Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: metaGrid.implicitHeight + 20
            radius: Theme.radiusSm
            color: Theme.bgElevated
            border.color: Theme.borderDefault
            border.width: 1

            GridLayout {
                id: metaGrid
                anchors.fill: parent
                anchors.margins: 10
                columns: 2
                rowSpacing: 6
                columnSpacing: 12

                Text { text: "Object Name:"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm }
                Text {
                    text: root.previewData && root.previewData.name ? root.previewData.name : "Unknown Object"
                    color: Theme.textPrimary; font.family: Theme.fontSans; font.weight: Font.DemiBold; font.pixelSize: Theme.fontSm
                }

                Text { text: "Author / Provenance:"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm }
                Text {
                    text: root.previewData && root.previewData.author ? root.previewData.author : "Standard CAD Export"
                    color: Theme.textPrimary; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm
                }

                Text { text: "Creation Date:"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm }
                Text {
                    text: root.previewData && root.previewData.creationDate ? root.previewData.creationDate : "Recent"
                    color: Theme.textSecondary; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs
                }

                Text { text: "Physical Mass / Vol:"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm }
                Text {
                    text: ((root.previewData && root.previewData.massKg ? root.previewData.massKg.toFixed(4) : "0.0000") + " kg  |  " +
                           (root.previewData && root.previewData.volumeM3 ? (root.previewData.volumeM3 * 1e6).toFixed(2) : "0.00") + " cm³")
                    color: Theme.textPrimary; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm
                }

                Text { text: "Regions / Layers:"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm }
                Text {
                    text: (root.previewData && root.previewData.regionCount ? root.previewData.regionCount : 1) + " material region(s)"
                    color: Theme.textSecondary; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm
                }
            }
        }

        // Materials & Missing Materials Resolution Notice
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: matCol.implicitHeight + 16
            radius: Theme.radiusSm
            color: root.previewData && root.previewData.hasMissingMaterials ? Theme.stateWarningBg : Theme.bgCanvas
            border.color: root.previewData && root.previewData.hasMissingMaterials ? Theme.stateWarningBorder : Theme.borderSubtle
            border.width: 1

            ColumnLayout {
                id: matCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    spacing: 6
                    Text {
                        text: "Constituent Materials:"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: root.previewData && root.previewData.materials ? root.previewData.materials.join(", ") : "Standard Material"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                Text {
                    visible: root.previewData && root.previewData.bundledMaterialsCount > 0
                    text: "✓ " + (root.previewData ? root.previewData.bundledMaterialsCount : 0) + " custom material definition(s) bundled in package. Will automatically register in project."
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.stateSuccessText
                }

                Text {
                    visible: Boolean(root.previewData && root.previewData.hasMissingMaterials)
                    text: "⚠ Notice: Some regions reference external materials that will fallback to standard properties."
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.stateWarningText
                }
            }
        }

        // Name Conflict Strategy
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "If Object Name Conflicts with Existing Library Object:"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSm
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            RowLayout {
                spacing: 12

                MonoButton {
                    text: "Keep Both (Auto-Rename)"
                    compact: true
                    active: root.conflictResolution === "KeepBoth"
                    onClicked: root.conflictResolution = "KeepBoth"
                }

                MonoButton {
                    text: "Replace Existing"
                    compact: true
                    active: root.conflictResolution === "ReplaceExisting"
                    onClicked: root.conflictResolution = "ReplaceExisting"
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Dialog Footer Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            MonoButton {
                text: "Cancel"
                compact: true
                onClicked: root.close()
            }

            MonoButton {
                text: "Import Object Package"
                variant: "primary"
                compact: true
                onClicked: {
                    root.importConfirmed(root.filePath, root.conflictResolution);
                    root.close();
                }
            }
        }
    }
}
