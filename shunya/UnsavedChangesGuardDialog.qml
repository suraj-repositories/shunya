import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: guardDialog
    title: qsTr("Save Changes to Project?")
    modal: true
    anchors.centerIn: parent
    width: 450
    height: 185

    property string pendingAction: ""
    signal proceedPending()
    signal saveAndProceed()

    background: Rectangle {
        color: Theme.bgSurface
        border.color: Theme.borderStrong
        border.width: 1
        radius: Theme.radiusMd
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: qsTr("The current project contains unsaved modifications.")
            font.family: Theme.fontSans
            font.bold: true
            font.pixelSize: Theme.fontMd
            color: Theme.textPrimary
        }

        Label {
            text: qsTr("If you proceed without saving, uncommitted simulation setups and parameters will be lost.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSm
            color: Theme.textMuted
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            MonoButton {
                text: qsTr("Save Changes")
                variant: "primary"
                onClicked: {
                    guardDialog.close();
                    guardDialog.saveAndProceed();
                }
            }

            MonoButton {
                text: qsTr("Don't Save")
                variant: "danger"
                onClicked: {
                    guardDialog.close();
                    guardDialog.proceedPending();
                }
            }

            MonoButton {
                text: qsTr("Cancel")
                onClicked: guardDialog.close()
            }
        }
    }
}
