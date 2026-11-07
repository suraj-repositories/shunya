import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: guardDialog
    title: qsTr("Save Changes to Project?")
    modal: true
    anchors.centerIn: parent
    width: 440
    height: 180

    property string pendingAction: ""

    background: Rectangle {
        color: "#222222"
        border.color: "#ef476f"
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: qsTr("The current project contains unsaved modifications.")
            font.bold: true
            font.pixelSize: 13
            color: "#ffffff"
        }

        Label {
            text: qsTr("If you proceed without saving, all uncommitted simulation setups and parameters will be permanently discarded.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.pixelSize: 12
            color: "#8b949e"
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            Button {
                text: qsTr("Save Changes")
                background: Rectangle { implicitWidth: 100; implicitHeight: 30; radius: 4; color: "#06d6a0" }
                contentItem: Text { text: parent.text; color: "#000000"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: {
                    guardDialog.close();
                    if (typeof menuController !== "undefined") menuController.saveProject();
                }
            }

            Button {
                text: qsTr("Don't Save")
                background: Rectangle { implicitWidth: 90; implicitHeight: 30; radius: 4; color: "#ef476f" }
                contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: {
                    guardDialog.close();
                    if (typeof menuController !== "undefined") menuController.closeProject(true);
                }
            }

            Button {
                text: qsTr("Cancel")
                background: Rectangle { implicitWidth: 70; implicitHeight: 30; radius: 4; color: "#333333"; border.color: "#555555" }
                contentItem: Text { text: parent.text; color: "#e0e0e0"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: guardDialog.close()
            }
        }
    }
}
