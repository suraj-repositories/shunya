import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: "transparent"
    implicitWidth: 320
    implicitHeight: 200

    property string iconName: "info"
    property string title: "No Items Selected"
    property string description: "Select an item from the library or create a new one to begin editing."
    property string actionLabel: ""
    signal actionTriggered()

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        width: Math.min(parent.width - 32, 340)

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 48
            height: 48
            radius: 24
            color: Theme.bgElevated
            border.color: Theme.borderSubtle
            border.width: 1

            ShunyaIcon {
                anchors.centerIn: parent
                name: root.iconName
                size: 24
                color: Theme.textMuted
            }
        }

        Text {
            text: root.title
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontMd
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Text {
            text: root.description
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSm
            color: Theme.textMuted
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        MonoButton {
            visible: root.actionLabel !== ""
            Layout.alignment: Qt.AlignHCenter
            text: root.actionLabel
            variant: "primary"
            compact: true
            onClicked: root.actionTriggered()
        }
    }
}
