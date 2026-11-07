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

    property string selectedTemplateId: "template.multiphysics"

    background: Rectangle {
        color: "#222222"
        border.color: "#444444"
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 14

        Label {
            text: qsTr("Project Configuration & Physics Template")
            font.bold: true
            font.pixelSize: 14
            color: "#00b4d8"
        }

        GridLayout {
            columns: 2
            rowSpacing: 10
            columnSpacing: 12
            Layout.fillWidth: true

            Label { text: qsTr("Project Name:"); color: "#e0e0e0" }
            TextField {
                id: projNameField
                Layout.fillWidth: true
                text: "MySimulationProject"
                color: "#ffffff"
                background: Rectangle { color: "#181818"; border.color: "#555555"; radius: 4 }
            }

            Label { text: qsTr("Author / Lead:"); color: "#e0e0e0" }
            TextField {
                id: authorField
                Layout.fillWidth: true
                text: "Lead Systems Engineer"
                color: "#ffffff"
                background: Rectangle { color: "#181818"; border.color: "#555555"; radius: 4 }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#333333" }

        Label {
            text: qsTr("Select Domain Template:")
            font.bold: true
            font.pixelSize: 13
            color: "#e0e0e0"
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
                radius: 4
                color: wizardDialog.selectedTemplateId === modelData.id ? "#2c3e50" : "#282828"
                border.color: wizardDialog.selectedTemplateId === modelData.id ? "#00b4d8" : "#383838"

                MouseArea {
                    anchors.fill: parent
                    onClicked: wizardDialog.selectedTemplateId = modelData.id
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    Rectangle {
                        width: 8
                        Layout.fillHeight: true
                        radius: 2
                        color: wizardDialog.selectedTemplateId === modelData.id ? "#00b4d8" : "#555555"
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        RowLayout {
                            Label {
                                text: modelData.name
                                font.bold: true
                                color: "#ffffff"
                            }
                            Label {
                                text: "[" + modelData.category + "]"
                                font.pixelSize: 11
                                color: "#ffd166"
                            }
                        }
                        Label {
                            text: modelData.description
                            font.pixelSize: 11
                            color: "#8b949e"
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
