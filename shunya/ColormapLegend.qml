import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#16191e"
    border.color: "#3e4451"
    border.width: 1
    radius: 6
    implicitWidth: 165

    property var controller: typeof colormapController !== "undefined" ? colormapController : null

    Connections {
        target: root.controller
        function onColormapChanged() { barCanvas.requestPaint(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Text {
            text: "SCALAR FIELD LEGEND"
            color: "#61afef"
            font.pixelSize: 10
            font.bold: true
        }

        // Preset selector grid
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 3
            columnSpacing: 3

            Repeater {
                model: ["Viridis", "Plasma", "Turbo", "Jet"]
                Rectangle {
                    Layout.fillWidth: true
                    height: 18
                    radius: 3
                    color: root.controller && root.controller.colormapName === modelData ? "#2c3e50" : "#21252b"
                    border.color: root.controller && root.controller.colormapName === modelData ? "#61afef" : "#3e4451"
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: root.controller && root.controller.colormapName === modelData ? "#ffffff" : "#abb2bf"
                        font.pixelSize: 9
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.controller) root.controller.colormapName = modelData;
                        }
                    }
                }
            }
        }

        // Vertical Colorbar + Tick Labels + Live Probe Indicator
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            Item {
                Layout.preferredWidth: 26
                Layout.fillHeight: true

                Canvas {
                    id: barCanvas
                    anchors.fill: parent

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        if (!root.controller) return;

                        var stops = root.controller.getGradientStops(32);
                        var grad = ctx.createLinearGradient(0, 0, 0, height);
                        for (var i = 0; i < stops.length; ++i) {
                            grad.addColorStop(stops[i].pos, stops[i].color);
                        }
                        ctx.fillStyle = grad;
                        ctx.fillRect(0, 0, width, height);
                        ctx.strokeStyle = "#4b5263";
                        ctx.lineWidth = 1;
                        ctx.strokeRect(0, 0, width, height);
                    }
                }

                // Live probe marker triangle / horizontal line
                Rectangle {
                    visible: root.controller && root.controller.probeActive
                    x: -2
                    y: (1.0 - (root.controller ? root.controller.probeNormalized : 0.5)) * parent.height - 1
                    width: parent.width + 4
                    height: 2
                    color: "#ffffff"
                    border.color: "#000000"
                    border.width: 0.5
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.CrossCursor
                    onPositionChanged: function(mouse) {
                        if (root.controller) {
                            var norm = 1.0 - Math.max(0.0, Math.min(1.0, mouse.y / Math.max(1.0, height)));
                            root.controller.updateProbeByNormalized(norm);
                        }
                    }
                }
            }

            // Min / Mid / Max Scale Ticks
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    anchors.top: parent.top
                    text: root.controller ? (root.controller.maxValue.toFixed(1) + " " + root.controller.unitSymbol) : "450.0 K"
                    color: "#e06c75"
                    font.pixelSize: 10
                    font.family: "Monospace"
                    font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.controller ? (((root.controller.maxValue + root.controller.minValue) * 0.5).toFixed(1) + " " + root.controller.unitSymbol) : "371.6 K"
                    color: "#abb2bf"
                    font.pixelSize: 9
                    font.family: "Monospace"
                }

                Text {
                    anchors.bottom: parent.bottom
                    text: root.controller ? (root.controller.minValue.toFixed(1) + " " + root.controller.unitSymbol) : "293.1 K"
                    color: "#61afef"
                    font.pixelSize: 10
                    font.family: "Monospace"
                    font.bold: true
                }
            }
        }

        // Probe Value Tooltip Footer
        Rectangle {
            Layout.fillWidth: true
            height: 28
            radius: 4
            color: "#0d1117"
            border.color: root.controller && root.controller.probeActive ? root.controller.probeColorHex : "#30363d"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 6

                Rectangle {
                    width: 12
                    height: 12
                    radius: 2
                    color: root.controller ? root.controller.probeColorHex : "#61afef"
                    border.color: "#ffffff44"
                }

                Text {
                    text: root.controller && root.controller.probeActive ? ("Probe: " + root.controller.probeLabel) : "Hover bar to probe"
                    color: "#ffffff"
                    font.pixelSize: 10
                    font.family: "Monospace"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
    }
}
