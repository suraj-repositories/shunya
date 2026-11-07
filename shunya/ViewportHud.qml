import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 240
    height: 110
    radius: 8
    color: "#1e2227ee"
    border.color: "#3e4451"
    border.width: 1

    property var controller: typeof viewportController !== "undefined" ? viewportController : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        // Top line: FPS & Status Dot
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: {
                    var f = root.controller ? root.controller.fps : 60.0;
                    if (f >= 55) return "#98c379"; // Green
                    if (f >= 30) return "#e5c07b"; // Yellow
                    return "#e06c75";            // Red
                }
            }

            Text {
                text: root.controller ? (root.controller.fps.toFixed(1) + " FPS") : "60.0 FPS"
                color: "#ffffff"
                font.pixelSize: 11
                font.bold: true
                font.family: "Monospace"
            }

            Text {
                text: "(" + (root.controller ? root.controller.frameTimeMs.toFixed(2) : "16.67") + " ms)"
                color: "#abb2bf"
                font.pixelSize: 10
                font.family: "Monospace"
                Layout.fillWidth: true
            }

            // Snapping quick toggle
            Rectangle {
                width: snapText.contentWidth + 10
                height: 18
                radius: 3
                color: root.controller && root.controller.snapEnabled ? "#2c3e50" : (sMouse.containsMouse ? "#3e4451" : "#282c34")
                border.color: root.controller && root.controller.snapEnabled ? "#61afef" : "#4b5263"

                Text {
                    id: snapText
                    anchors.centerIn: parent
                    text: root.controller && root.controller.snapEnabled ? "SNAP ON" : "SNAP OFF"
                    color: root.controller && root.controller.snapEnabled ? "#61afef" : "#6e7681"
                    font.pixelSize: 8
                    font.bold: true
                }

                MouseArea {
                    id: sMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.controller) root.controller.setSnapEnabled(!root.controller.snapEnabled);
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#282c34"
        }

        // Telemetry Metrics Grid
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 2
            columnSpacing: 8

            Text { text: "Solver Step:"; color: "#6e7681"; font.pixelSize: 9 }
            Text {
                text: root.controller ? (root.controller.solverStepTimeMs.toFixed(2) + " ms") : "2.40 ms"
                color: "#98c379"
                font.pixelSize: 9
                font.family: "Monospace"
            }

            Text { text: "Solver Residual:"; color: "#6e7681"; font.pixelSize: 9 }
            Text {
                text: root.controller ? root.controller.solverResidual.toExponential(2) : "1.42e-7"
                color: "#e5c07b"
                font.pixelSize: 9
                font.family: "Monospace"
            }

            Text { text: "Active Transform:"; color: "#6e7681"; font.pixelSize: 9 }
            Text {
                text: {
                    if (!root.controller) return "X: 0.00 Y: 0.00 Z: 0.00";
                    return "X: " + root.controller.targetPositionX.toFixed(2) +
                           " Y: " + root.controller.targetPositionY.toFixed(2) +
                           " Z: " + root.controller.targetPositionZ.toFixed(2);
                }
                color: "#61afef"
                font.pixelSize: 9
                font.family: "Monospace"
                elide: Text.ElideRight
            }
        }

        // Domain Badges
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: ["Circuit", "Thermal", "RigidBody"]

                Rectangle {
                    height: 14
                    width: bText.contentWidth + 6
                    radius: 2
                    color: "#21252b"
                    border.color: "#3e4451"

                    Text {
                        id: bText
                        anchors.centerIn: parent
                        text: modelData
                        color: "#98c379"
                        font.pixelSize: 8
                    }
                }
            }
        }
    }
}
