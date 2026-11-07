import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent
    visible: active

    property bool active: false
    property int currentZone: 0 // 0: None, 1: North, 2: South, 3: East, 4: West, 5: CenterTab
    property string targetNodeId: ""

    signal dropTriggered(string targetId, int snapZone)

    // Semi-transparent backdrop during drag
    Rectangle {
        anchors.fill: parent
        color: "#18007acc"
        border.color: "#007acc"
        border.width: 1
    }

    // Drop Preview Indicator
    Rectangle {
        id: dropPreview
        color: "#40007acc"
        border.color: "#0098ff"
        border.width: 2
        radius: 4
        visible: root.currentZone > 0

        x: {
            if (root.currentZone === 4) return 0; // West
            if (root.currentZone === 3) return parent.width * 0.5; // East
            return 0;
        }
        y: {
            if (root.currentZone === 1) return 0; // North
            if (root.currentZone === 2) return parent.height * 0.5; // South
            return 0;
        }
        width: {
            if (root.currentZone === 3 || root.currentZone === 4) return parent.width * 0.5;
            return parent.width;
        }
        height: {
            if (root.currentZone === 1 || root.currentZone === 2) return parent.height * 0.5;
            return parent.height;
        }
    }

    // Directional Docking Compass in Center
    Rectangle {
        id: compass
        width: 120
        height: 120
        anchors.centerIn: parent
        color: "#252526"
        border.color: "#3f3f46"
        border.width: 1
        radius: 6

        // North zone (1)
        Rectangle {
            width: 32; height: 32
            anchors.top: parent.top; anchors.topMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.currentZone === 1 ? "#007acc" : "#333337"
            radius: 3
            Text {
                text: "▲"
                color: "white"
                anchors.centerIn: parent
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: root.currentZone = 1
            }
        }

        // South zone (2)
        Rectangle {
            width: 32; height: 32
            anchors.bottom: parent.bottom; anchors.bottomMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.currentZone === 2 ? "#007acc" : "#333337"
            radius: 3
            Text {
                text: "▼"
                color: "white"
                anchors.centerIn: parent
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: root.currentZone = 2
            }
        }

        // East zone (3)
        Rectangle {
            width: 32; height: 32
            anchors.right: parent.right; anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            color: root.currentZone === 3 ? "#007acc" : "#333337"
            radius: 3
            Text {
                text: "▶"
                color: "white"
                anchors.centerIn: parent
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: root.currentZone = 3
            }
        }

        // West zone (4)
        Rectangle {
            width: 32; height: 32
            anchors.left: parent.left; anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            color: root.currentZone === 4 ? "#007acc" : "#333337"
            radius: 3
            Text {
                text: "◀"
                color: "white"
                anchors.centerIn: parent
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: root.currentZone = 4
            }
        }

        // CenterTab zone (5)
        Rectangle {
            width: 36; height: 36
            anchors.centerIn: parent
            color: root.currentZone === 5 ? "#007acc" : "#3e3e42"
            radius: 4
            Text {
                text: "⊞"
                color: "white"
                anchors.centerIn: parent
                font.pixelSize: 16
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: root.currentZone = 5
            }
        }
    }
}
