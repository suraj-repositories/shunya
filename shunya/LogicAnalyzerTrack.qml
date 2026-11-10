import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0d1117"
    border.color: "#30363d"
    border.width: 1
    radius: 4

    property var controller: typeof oscilloscopeController !== "undefined" ? oscilloscopeController : null
    property var logicChannels: []
    property var busWords: []

    function refreshData() {
        if (root.controller) {
            root.logicChannels = root.controller.getLogicChannels();
            root.busWords = root.controller.getDecodedBusWords();
            logicCanvas.requestPaint();
        }
    }

    Component.onCompleted: refreshData()

    Connections {
        target: root.controller
        function onWaveformChanged() { root.refreshData(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "LOGIC ANALYZER (4-BIT BUS DECODER)"
                color: "#98c379"
                font.pixelSize: 10
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.busWords.length + " decoded packets (Hex/Bin)"
                color: "#8b949e"
                font.pixelSize: 9
                font.family: "Monospace"
            }
        }

        Canvas {
            id: logicCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);

                var labelW = 68;
                var plotW = Math.max(10, width - labelW - 6);
                var rowCount = root.logicChannels.length + 1; // +1 for Decoded Parallel Bus row
                if (rowCount <= 1) return;
                var rowH = height / rowCount;

                // Draw each digital bit track
                for (var r = 0; r < root.logicChannels.length; ++r) {
                    var ch = root.logicChannels[r];
                    var yTop = r * rowH + 3;
                    var yBot = (r + 1) * rowH - 3;
                    var yMid = (yTop + yBot) * 0.5;

                    // Channel label
                    ctx.fillStyle = "#abb2bf";
                    ctx.font = "10px Monospace";
                    ctx.fillText(ch.name, 4, yMid + 3);

                    // Background track separator
                    ctx.strokeStyle = "#1c2128";
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.moveTo(labelW, (r + 1) * rowH);
                    ctx.lineTo(width, (r + 1) * rowH);
                    ctx.stroke();

                    // Digital waveform trace
                    var segs = ch.segments;
                    ctx.strokeStyle = (r === 0) ? "#e5c07b" : "#98c379";
                    ctx.lineWidth = 1.6;
                    ctx.beginPath();
                    var prevY = yBot;
                    for (var s = 0; s < segs.length; ++s) {
                        var sx = labelW + segs[s].startNorm * plotW;
                        var ex = labelW + segs[s].endNorm * plotW;
                        var sy = (segs[s].level === 1) ? yTop : yBot;
                        if (s === 0) {
                            ctx.moveTo(sx, sy);
                        } else {
                            ctx.lineTo(sx, prevY);
                            ctx.lineTo(sx, sy);
                        }
                        ctx.lineTo(ex, sy);
                        prevY = sy;
                    }
                    ctx.stroke();
                }

                // Draw Decoded Parallel Bus track at bottom row
                var busRowIdx = root.logicChannels.length;
                var bTop = busRowIdx * rowH + 2;
                var bH = rowH - 4;
                ctx.fillStyle = "#61afef";
                ctx.font = "bold 10px Monospace";
                ctx.fillText("BUS[3:0]", 4, bTop + bH * 0.5 + 3);

                for (var w = 0; w < root.busWords.length; ++w) {
                    var word = root.busWords[w];
                    var wx = labelW + word.startNorm * plotW;
                    var ww = Math.max(2, (word.endNorm - word.startNorm) * plotW - 2);

                    ctx.fillStyle = word.hasUnknown ? "#e06c7544" : "#2c3e50aa";
                    ctx.fillRect(wx, bTop, ww, bH);
                    ctx.strokeStyle = word.hasUnknown ? "#e06c75" : "#61afef";
                    ctx.lineWidth = 1;
                    ctx.strokeRect(wx, bTop, ww, bH);

                    if (ww > 26) {
                        ctx.fillStyle = "#ffffff";
                        ctx.font = "bold 9px Monospace";
                        ctx.fillText(word.hex, wx + 4, bTop + bH * 0.5 + 3);
                    }
                }
            }
        }
    }
}
