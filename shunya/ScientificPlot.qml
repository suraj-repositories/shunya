import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import shunya

Rectangle {
    id: root
    color: Theme.bgSurface
    border.color: Theme.borderDefault
    border.width: 1
    radius: Theme.radiusMd

    property alias plotItem: plotNode
    property string title: "SCIENTIFIC PLOTTER"
    property string xLabel: "X"
    property string yLabel: "Y"
    property bool showDemoControls: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Top Toolbar Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: root.title
                color: Theme.textPrimary
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSm
                font.bold: true
            }

            Rectangle {
                height: 18
                width: statsText.contentWidth + 10
                radius: 3
                color: Theme.bgCanvas
                border.color: Theme.borderSubtle
                Text {
                    id: statsText
                    anchors.centerIn: parent
                    text: plotNode.pointCount + " pts"
                    color: Theme.textMuted
                    font.pixelSize: 10
                    font.family: Theme.fontMono
                }
            }

            Item { Layout.fillWidth: true }

            // Preset selector buttons (optional demo mode)
            Repeater {
                model: root.showDemoControls ? [
                    { label: "Damped 10k", id: "damped_sine" },
                    { label: "Chirp 500k", id: "chirp_500k" },
                    { label: "Step 4k", id: "step_response" }
                ] : []
                Rectangle {
                    height: 20
                    width: pText.contentWidth + 10
                    radius: 3
                    color: plotNode.demoSignal === modelData.id ? Theme.bgActive : Theme.bgElevated
                    border.color: plotNode.demoSignal === modelData.id ? Theme.borderStrong : Theme.borderSubtle
                    Text {
                        id: pText
                        anchors.centerIn: parent
                        text: modelData.label
                        color: Theme.textPrimary
                        font.family: Theme.fontSans
                        font.pixelSize: 10
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
                color: Theme.bgElevated
                border.color: Theme.borderDefault
                Text {
                    id: mText
                    anchors.centerIn: parent
                    text: plotNode.plotMode === "line" ? "LINE" : "SCATTER"
                    color: Theme.textSecondary
                    font.family: Theme.fontMono
                    font.pixelSize: 10
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
                color: Theme.bgElevated
                border.color: Theme.borderDefault
                Text {
                    id: rText
                    anchors.centerIn: parent
                    text: "Reset Zoom"
                    color: Theme.textSecondary
                    font.family: Theme.fontSans
                    font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: plotNode.resetView()
                }
            }
        }

        // Dedicated Wrapping Legend Row (Zero Overlap)
        Flow {
            visible: plotNode.legendItems.length > 0
            Layout.fillWidth: true
            spacing: 10
            Repeater {
                model: plotNode.legendItems
                delegate: Row {
                    spacing: 4
                    Rectangle { width: 9; height: 9; radius: 2; color: modelData.color; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textSecondary; anchors.verticalCenter: parent.verticalCenter }
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
                    model: plotNode.ticksY
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
                    color: Theme.bgPrimary
                    border.color: Theme.borderDefault
                    border.width: 1
                    clip: true

                    // Grid lines
                    Repeater {
                        model: plotNode.ticksX
                        Rectangle {
                            x: modelData.norm * viewportFrame.width
                            y: 0
                            width: 1
                            height: viewportFrame.height
                            color: Theme.gridLine
                        }
                    }
                    Repeater {
                        model: plotNode.ticksY
                        Rectangle {
                            x: 0
                            y: modelData.norm * viewportFrame.height
                            width: viewportFrame.width
                            height: 1
                            color: Theme.gridLine
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
                        model: plotNode.ticksX
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
