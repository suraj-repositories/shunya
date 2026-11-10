import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import shunya

Rectangle {
    id: root
    color: "#16191e"
    border.color: "#3e4451"
    border.width: 1
    radius: 6

    property alias plotItem: plotNode

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Top Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "SCIENTIFIC PLOTTER (GPU RHI)"
                color: "#61afef"
                font.pixelSize: 11
                font.bold: true
            }

            Rectangle {
                height: 18
                width: statsText.contentWidth + 10
                radius: 3
                color: "#21252b"
                border.color: "#3e4451"
                Text {
                    id: statsText
                    anchors.centerIn: parent
                    text: plotNode.pointCount + " pts -> " + plotNode.decimatedCount + " GPU verts"
                    color: "#98c379"
                    font.pixelSize: 9
                    font.family: "Monospace"
                }
            }

            Item { Layout.fillWidth: true }

            // Preset selector buttons
            Repeater {
                model: [
                    { label: "Damped 10k", id: "damped_sine" },
                    { label: "Chirp 500k", id: "chirp_500k" },
                    { label: "Step 4k", id: "step_response" }
                ]
                Rectangle {
                    height: 20
                    width: pText.contentWidth + 10
                    radius: 3
                    color: plotNode.demoSignal === modelData.id ? "#2c3e50" : "#21252b"
                    border.color: plotNode.demoSignal === modelData.id ? "#61afef" : "#3e4451"
                    Text {
                        id: pText
                        anchors.centerIn: parent
                        text: modelData.label
                        color: plotNode.demoSignal === modelData.id ? "#ffffff" : "#abb2bf"
                        font.pixelSize: 9
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: plotNode.loadDemoSignal(modelData.id)
                    }
                }
            }

            // Line/Scatter toggle
            Rectangle {
                height: 20
                width: mText.contentWidth + 10
                radius: 3
                color: "#282c34"
                border.color: "#4b5263"
                Text {
                    id: mText
                    anchors.centerIn: parent
                    text: plotNode.plotMode === "line" ? "MODE: LINE" : "MODE: SCATTER"
                    color: "#e5c07b"
                    font.pixelSize: 9
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: plotNode.setPlotMode(plotNode.plotMode === "line" ? "scatter" : "line")
                }
            }

            // Reset Zoom button
            Rectangle {
                height: 20
                width: rText.contentWidth + 10
                radius: 3
                color: "#282c34"
                border.color: "#4b5263"
                Text {
                    id: rText
                    anchors.centerIn: parent
                    text: "Reset Zoom"
                    color: "#abb2bf"
                    font.pixelSize: 9
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: plotNode.resetView()
                }
            }
        }

        // Main Plot Region with Y ticks + GPU Canvas + X ticks
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            // Y Axis Labels Column
            Item {
                Layout.preferredWidth: 52
                Layout.fillHeight: true
                Layout.bottomMargin: 20

                Repeater {
                    model: plotNode.getTicksY()
                    Text {
                        x: 0
                        y: modelData.norm * parent.height - 6
                        width: 48
                        horizontalAlignment: Text.AlignRight
                        text: modelData.label
                        color: "#8b949e"
                        font.pixelSize: 9
                        font.family: "Monospace"
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 2

                // GPU Plot Viewport Box
                Rectangle {
                    id: viewportFrame
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0d1117"
                    border.color: "#30363d"
                    border.width: 1
                    clip: true

                    // Grid lines
                    Repeater {
                        model: plotNode.getTicksX()
                        Rectangle {
                            x: modelData.norm * viewportFrame.width
                            y: 0
                            width: 1
                            height: viewportFrame.height
                            color: "#1c2128"
                        }
                    }
                    Repeater {
                        model: plotNode.getTicksY()
                        Rectangle {
                            x: 0
                            y: modelData.norm * viewportFrame.height
                            width: viewportFrame.width
                            height: 1
                            color: "#1c2128"
                        }
                    }

                    // GPU Accelerated Plot Node
                    ShunyaScientificPlotItem {
                        id: plotNode
                        anchors.fill: parent
                        lineColor: "#61afef"
                    }

                    // Crosshair Lines & Tooltip
                    Rectangle {
                        visible: plotNode.crosshairValid
                        x: plotNode.crosshairNormX * parent.width
                        y: 0
                        width: 1
                        height: parent.height
                        color: "#e5c07b88"
                    }
                    Rectangle {
                        visible: plotNode.crosshairValid
                        x: 0
                        y: plotNode.crosshairNormY * parent.height
                        width: parent.width
                        height: 1
                        color: "#e5c07b88"
                    }
                    Rectangle {
                        visible: plotNode.crosshairValid
                        x: Math.min(parent.width - width - 6, plotNode.crosshairNormX * parent.width + 8)
                        y: Math.max(6, plotNode.crosshairNormY * parent.height - height - 6)
                        width: probeTxt.contentWidth + 12
                        height: probeTxt.contentHeight + 8
                        radius: 4
                        color: "#1e2227ee"
                        border.color: "#e5c07b"
                        Text {
                            id: probeTxt
                            anchors.centerIn: parent
                            text: "x=" + plotNode.crosshairDataX.toFixed(4) + ", y=" + plotNode.crosshairDataY.toFixed(4)
                            color: "#ffffff"
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    // Interaction MouseArea (Crosshair + Pan + Wheel Zoom)
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        property point lastDrag: Qt.point(0, 0)
                        property bool dragging: false

                        onPressed: function(mouse) {
                            lastDrag = Qt.point(mouse.x, mouse.y);
                            dragging = true;
                        }
                        onReleased: dragging = false
                        onExited: plotNode.clearCrosshair()
                        onPositionChanged: function(mouse) {
                            if (dragging) {
                                var dx = -(mouse.x - lastDrag.x) / Math.max(1.0, width);
                                var dy = (mouse.y - lastDrag.y) / Math.max(1.0, height);
                                lastDrag = Qt.point(mouse.x, mouse.y);
                                plotNode.panBy(dx, dy);
                            } else {
                                plotNode.updateCrosshair(mouse.x, mouse.y);
                            }
                        }
                        onWheel: function(wheel) {
                            var factor = wheel.angleDelta.y > 0 ? 0.85 : 1.18;
                            plotNode.zoomAt(wheel.x / Math.max(1.0, width), wheel.y / Math.max(1.0, height), factor);
                        }
                    }
                }

                // X Axis Labels Row
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 18

                    Repeater {
                        model: plotNode.getTicksX()
                        Text {
                            x: Math.min(parent.width - width, Math.max(0, modelData.norm * parent.width - width / 2))
                            y: 2
                            text: modelData.label
                            color: "#8b949e"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                    }
                }
            }
        }
    }
}
