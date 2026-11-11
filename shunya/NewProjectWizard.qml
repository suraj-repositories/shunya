import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: wizardDialog
    title: qsTr("Create New Engineering Project")
    modal: true
    anchors.centerIn: parent
    width: 600
    height: 480
    standardButtons: Dialog.Ok | Dialog.Cancel

    property string selectedTemplateId: "lithium_battery"

    background: Rectangle {
        color: Theme.bgSurface
        border.color: Theme.borderDefault
        border.width: 1
        radius: Theme.radiusMd
    }

    contentItem: ColumnLayout {
        spacing: 14

        Label {
            text: qsTr("Project Configuration & Workflow Template")
            font.family: Theme.fontSans
            font.bold: true
            font.pixelSize: Theme.fontLg
            color: Theme.textPrimary
        }

        GridLayout {
            columns: 2
            rowSpacing: 10
            columnSpacing: 12
            Layout.fillWidth: true

            Label { text: qsTr("Project Name:"); color: Theme.textSecondary; font.family: Theme.fontSans }
            MonoInput {
                id: projNameField
                Layout.fillWidth: true
                text: "LiFePO4_Battery_Project"
            }

            Label { text: qsTr("Author / Lead:"); color: Theme.textSecondary; font.family: Theme.fontSans }
            MonoInput {
                id: authorField
                Layout.fillWidth: true
                text: "Lead Systems Engineer"
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

        Label {
            text: qsTr("Select Workflow Template:")
            font.family: Theme.fontSans
            font.bold: true
            font.pixelSize: Theme.fontMd
            color: Theme.textPrimary
        }

        ListView {
            id: templateList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: typeof menuController !== "undefined" ? menuController.projectTemplates : []

            delegate: Rectangle {
                width: templateList.width
                height: 54
                radius: Theme.radiusSm
                color: wizardDialog.selectedTemplateId === modelData.id ? Theme.bgActive : Theme.bgCanvas
                border.color: wizardDialog.selectedTemplateId === modelData.id ? Theme.borderFocus : Theme.borderSubtle
                border.width: 1

                MouseArea {
                    anchors.fill: parent
                    onClicked: wizardDialog.selectedTemplateId = modelData.id
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    Rectangle {
                        width: 4
                        Layout.fillHeight: true
                        radius: 2
                        color: wizardDialog.selectedTemplateId === modelData.id ? Theme.btnPrimaryBg : Theme.borderDefault
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        RowLayout {
                            Label {
                                text: modelData.name
                                font.family: Theme.fontSans
                                font.bold: true
                                color: Theme.textPrimary
                            }
                            Label {
                                text: "[" + modelData.category + "]"
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontXs
                                color: Theme.textMuted
                            }
                        }
                        Label {
                            text: modelData.description
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textMuted
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    onAccepted: {
        if (typeof menuController !== "undefined") {
            menuController.newProject(projNameField.text, authorField.text, wizardDialog.selectedTemplateId);
        }
    }
}
