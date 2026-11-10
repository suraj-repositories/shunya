import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#16191e"
    border.color: "#3e4451"
    border.width: 1
    radius: 6

    property var controller: typeof oscilloscopeController !== "undefined" ? oscilloscopeController : null
    property var ch1Trace: []
    property var ch2Trace: []

    function refreshScope() {
        if (root.controller) {
            root.ch1Trace = root.controller.getAnalogTraceNormalized(0);
            root.ch2Trace = root.controller.getAnalogTraceNormalized(1);
            scopeCanvas.requestPaint();
        }
    }

    Component.onCompleted: refreshScope()

    Connections {
        target: root.controller
        function onWaveformChanged() { root.refreshScope(); }
        function onTriggerChanged() { root.refreshScope(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Scope Header & Trigger Controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "OSCILLOSCOPE & MIXED-SIGNAL"
                color: "#e5c07b"
                font.pixelSize: 11
                font.bold: true
            }

            Rectangle {
                height: 18
                width: trigStatusTxt.contentWidth + 10
                radius: 3
                color: root.controller && root.controller.triggered ? "#1b3a24" : "#3a1b1b"
                border.color: root.controller && root.controller.triggered ? "#98c379" : "#e06c75"
                Text {
                    id: trigStatusTxt
                    anchors.centerIn: parent
                    text: root.controller && root.controller.triggered ? "TRIG'D" : "AUTO"
                    color: root.controller && root.controller.triggered ? "#98c379" : "#e06c75"
                    font.pixelSize: 9
                    font.bold: true
                    font.family: "Monospace"
                }
            }

            Item { Layout.fillWidth: true }

            // Signal capture preset toggle
            Repeater {
                model: [
                    { label: "RLC Sine", id: "mixed_signal" },
                    { label: "PWM Drive", id: "pwm" }
                ]
                Rectangle {
                    height: 18
                    width: mTxt.contentWidth + 8
                    radius: 3
                    color: "#21252b"
                    border.color: "#3e4451"
                    Text {
                        id: mTxt
                        anchors.centerIn: parent
                        text: modelData.label
                        color: "#abb2bf"
                        font.pixelSize: 9
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.controller) root.controller.loadDemoCapture(modelData.id);
                        }
                    }
                }
            }

            // Trigger edge buttons
            Repeater {
                model: ["Rising", "Falling"]
                Rectangle {
                    height: 18
                    width: eTxt.contentWidth + 10
                    radius: 3
                    color: root.controller && root.controller.triggerEdge === modelData ? "#2c3e50" : "#21252b"
                    border.color: root.controller && root.controller.triggerEdge === modelData ? "#e5c07b" : "#3e4451"
                    Text {
                        id: eTxt
                        anchors.centerIn: parent
                        text: modelData === "Rising" ? "Rise \u2191" : "Fall \u2193"
                        color: root.controller && root.controller.triggerEdge === modelData ? "#e5c07b" : "#abb2bf"
                        font.pixelSize: 9
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.controller) root.controller.setTriggerEdge(modelData);
                        }
                    }
                }
            }
        }

        // Analog Trace Canvas
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 160
            color: "#0d1117"
            border.color: "#30363d"
            border.width: 1
            clip: true

            Canvas {
                id: scopeCanvas
                anchors.fill: parent

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    // Graticule (10x8 divisions)
                    ctx.strokeStyle = "#1c2128";
                    ctx.lineWidth = 1;
                    for (var i = 1; i < 10; ++i) {
                        var gx = (i / 10.0) * width;
                        ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, height); ctx.stroke();
                    }
                    for (var j = 1; j < 8; ++j) {
                        var gy = (j / 8.0) * height;
                        ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke();
                    }

                    // 0V center ground reference
                    ctx.strokeStyle = "#30363d";
                    ctx.beginPath(); ctx.moveTo(0, height * 0.5); ctx.lineTo(width, height * 0.5); ctx.stroke();

                    // Pre-trigger time marker line (x = 0.25)
                    ctx.strokeStyle = "#e06c7588";
                    ctx.beginPath(); ctx.moveTo(width * 0.25, 0); ctx.lineTo(width * 0.25, height); ctx.stroke();

                    // Trigger voltage level line
                    if (root.controller) {
                        var trigY = (0.5 - (root.controller.triggerLevel / 10.0)) * height;
                        ctx.strokeStyle = "#e5c07b66";
                        ctx.beginPath(); ctx.moveTo(0, trigY); ctx.lineTo(width, trigY); ctx.stroke();
                    }

                    function drawTrace(pts, color) {
                        if (!pts || pts.length < 2) return;
                        ctx.strokeStyle = color;
                        ctx.lineWidth = 1.8;
                        ctx.beginPath();
                        for (var k = 0; k < pts.length; ++k) {
                            var sx = pts[k].x * width;
                            var sy = pts[k].y * height;
                            if (k === 0) ctx.moveTo(sx, sy);
                            else ctx.lineTo(sx, sy);
                        }
                        ctx.stroke();
                    }

                    drawTrace(root.ch1Trace, "#e5c07b"); // Gold CH1
                    drawTrace(root.ch2Trace, "#61afef"); // Blue CH2
                }
            }
        }

        // Measurements Readout Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: root.controller
                    ? ("CH1 Vpp=" + root.controller.ch1Vpp.toFixed(2) + "V  Vrms=" + root.controller.ch1Vrms.toFixed(2) + "V  f=" + root.controller.ch1FreqHz.toFixed(1) + "Hz")
                    : "CH1"
                color: "#e5c07b"
                font.pixelSize: 9
                font.family: "Monospace"
            }

            Text {
                text: root.controller
                    ? ("CH2 Vpp=" + root.controller.ch2Vpp.toFixed(2) + "V")
                    : "CH2"
                color: "#61afef"
                font.pixelSize: 9
                font.family: "Monospace"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.controller ? ("Trig Lvl: " + root.controller.triggerLevel.toFixed(2) + "V") : ""
                color: "#8b949e"
                font.pixelSize: 9
                font.family: "Monospace"
            }
        }

        // Stacked Logic Analyzer Track
        LogicAnalyzerTrack {
            Layout.fillWidth: true
            Layout.preferredHeight: 135
            controller: root.controller
        }
    }
}
