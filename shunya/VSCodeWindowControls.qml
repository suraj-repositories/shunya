import QtQuick
import QtQuick.Controls.Basic

Row {
    id: root
    spacing: 0
    height: 30

    property var targetWindow: null

    function getTargetWindow() {
        if (targetWindow) return targetWindow;
        if (typeof window !== "undefined") return window;
        return root.Window.window;
    }

    // 1. Minimize Button
    Rectangle {
        id: minBtn
        width: 46
        height: 30
        color: minMa.pressed
            ? Theme.bgElevated
            : (minMa.containsMouse ? Theme.bgHover : "transparent")

        property color currentColor: color

        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 1
            color: minMa.containsMouse ? Theme.textPrimary : Theme.textSecondary
        }

        MouseArea {
            id: minMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            onClicked: {
                var w = root.getTargetWindow();
                if (w) w.showMinimized();
            }
        }

        ToolTip.visible: minMa.containsMouse
        ToolTip.delay: 500
        ToolTip.text: qsTr("Minimize")
    }

    // 2. Maximize / Restore Button
    Rectangle {
        id: maxBtn
        width: 46
        height: 30
        color: maxMa.pressed
            ? Theme.bgElevated
            : (maxMa.containsMouse ? Theme.bgHover : "transparent")

        property color currentColor: color
        readonly property bool isMaximized: {
            var w = root.getTargetWindow();
            return w && w.visibility === Window.Maximized;
        }

        // Single Rectangle (Restore down / Maximize)
        Item {
            anchors.centerIn: parent
            width: 10
            height: 10

            // Normal mode: Single 10x10 square
            Rectangle {
                visible: !maxBtn.isMaximized
                anchors.fill: parent
                color: "transparent"
                border.width: 1
                border.color: maxMa.containsMouse ? Theme.textPrimary : Theme.textSecondary
            }

            // Maximized mode: Two overlapping squares (Restore)
            Item {
                visible: maxBtn.isMaximized
                anchors.fill: parent

                Rectangle {
                    x: 2; y: 0
                    width: 8; height: 8
                    color: "transparent"
                    border.width: 1
                    border.color: maxMa.containsMouse ? Theme.textPrimary : Theme.textSecondary
                }

                Rectangle {
                    x: 0; y: 2
                    width: 8; height: 8
                    color: maxBtn.currentColor === "transparent" ? Theme.bgSurface : maxBtn.currentColor
                    border.width: 1
                    border.color: maxMa.containsMouse ? Theme.textPrimary : Theme.textSecondary
                }
            }
        }

        MouseArea {
            id: maxMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            onClicked: {
                var w = root.getTargetWindow();
                if (w) {
                    if (w.visibility === Window.Maximized) {
                        w.showNormal();
                    } else {
                        w.showMaximized();
                    }
                }
            }
        }

        ToolTip.visible: maxMa.containsMouse
        ToolTip.delay: 500
        ToolTip.text: maxBtn.isMaximized ? qsTr("Restore Down") : qsTr("Maximize")
    }

    // 3. Close Button (VS Code / Windows Standard Red Hover)
    Rectangle {
        id: closeBtn
        width: 46
        height: 30
        color: closeMa.pressed
            ? "#c42b1c"
            : (closeMa.containsMouse ? "#e81123" : "transparent")

        Item {
            anchors.centerIn: parent
            width: 10
            height: 10

            Rectangle {
                width: 12
                height: 1.2
                rotation: 45
                color: closeMa.containsMouse ? "#ffffff" : Theme.textSecondary
                anchors.centerIn: parent
                antialiasing: true
            }

            Rectangle {
                width: 12
                height: 1.2
                rotation: -45
                color: closeMa.containsMouse ? "#ffffff" : Theme.textSecondary
                anchors.centerIn: parent
                antialiasing: true
            }
        }

        MouseArea {
            id: closeMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            onClicked: {
                if (typeof menuController !== "undefined") {
                    menuController.requestExit();
                } else {
                    var w = root.getTargetWindow();
                    if (w) w.close();
                    else Qt.quit();
                }
            }
        }

        ToolTip.visible: closeMa.containsMouse
        ToolTip.delay: 500
        ToolTip.text: qsTr("Close (Alt+F4)")
    }
}
