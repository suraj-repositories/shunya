import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root
    property string tooltipText: ""
    property bool active: false
    property int panelType: 0 // 0: Left Sidebar, 1: Bottom Panel, 2: Right Sidebar
    signal clicked()

    implicitWidth: 26
    implicitHeight: 22
    radius: Theme.radiusSm
    color: btnMa.pressed
        ? Theme.bgElevated
        : (active
            ? (btnMa.containsMouse ? Theme.bgHover : Theme.bgElevated)
            : (btnMa.containsMouse ? Theme.bgHover : "transparent"))
    border.color: active ? Theme.borderDefault : "transparent"
    border.width: 1

    Item {
        anchors.centerIn: parent
        width: 16
        height: 12

        // Outer window frame
        Rectangle {
            anchors.fill: parent
            radius: 2
            color: "transparent"
            border.color: root.active ? Theme.textPrimary : (btnMa.containsMouse ? Theme.textPrimary : Theme.textSecondary)
            border.width: 1
        }

        // Left Sidebar indicator
        Rectangle {
            visible: root.panelType === 0
            x: 2
            y: 2
            width: 3
            height: parent.height - 4
            radius: 1
            color: root.active ? Theme.textPrimary : Theme.textMuted
        }

        // Bottom Panel indicator (VS Code Toggle Panel format)
        Rectangle {
            visible: root.panelType === 1
            x: 2
            y: parent.height - 5
            width: parent.width - 4
            height: 3
            radius: 1
            color: root.active ? Theme.textPrimary : Theme.textMuted
        }

        // Right Sidebar indicator
        Rectangle {
            visible: root.panelType === 2
            x: parent.width - 5
            y: 2
            width: 3
            height: parent.height - 4
            radius: 1
            color: root.active ? Theme.textPrimary : Theme.textMuted
        }
    }

    MouseArea {
        id: btnMa
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: btnMa.containsMouse
    ToolTip.delay: 400
    ToolTip.text: root.tooltipText
}
