import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0b1120"
    border.color: "#1e293b"
    border.width: 1

    property double boardWidthMm: 100.0
    property double boardHeightMm: 80.0
    property int activeLayerIndex: 0
    property var tracksModel: []
    property var viasModel: []

    signal placeViaRequested(double xMm, double yMm)

    focus: true
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_V) {
            root.activeLayerIndex = (root.activeLayerIndex === 0) ? 1 : 0;
            root.placeViaRequested(boardWidthMm * 0.5, boardHeightMm * 0.5);
            event.accepted = true;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "PCB Routing • Layer " + (root.activeLayerIndex === 0 ? "F.Cu (Top)" : "B.Cu (Bottom)")
                color: root.activeLayerIndex === 0 ? "#ef4444" : "#3b82f6"
                font.bold: true
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Label {
                text: "45° Octilinear + Push & Shove ('V' places via)"
                color: "#94a3b8"
                font.pixelSize: 11
            }
        }

        Canvas {
            id: pcbCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const sx = (width - 20) / Math.max(1.0, root.boardWidthMm);
                const sy = (height - 20) / Math.max(1.0, root.boardHeightMm);
                const scale = Math.min(sx, sy);

                // Board outline
                ctx.strokeStyle = "#facc15";
                ctx.lineWidth = 1.5;
                ctx.strokeRect(10, 10, root.boardWidthMm * scale, root.boardHeightMm * scale);

                // Traces
                for (let i = 0; i < root.tracksModel.length; ++i) {
                    const t = root.tracksModel[i];
                    ctx.strokeStyle = (t.layer === 0) ? "#ef4444" : "#3b82f6";
                    ctx.lineWidth = Math.max(1.5, t.widthMm * scale);
                    ctx.lineCap = "round";
                    ctx.beginPath();
                    ctx.moveTo(10 + t.x1 * scale, 10 + t.y1 * scale);
                    ctx.lineTo(10 + t.x2 * scale, 10 + t.y2 * scale);
                    ctx.stroke();
                }

                // Vias
                for (let j = 0; j < root.viasModel.length; ++j) {
                    const v = root.viasModel[j];
                    ctx.fillStyle = "#e2e8f0";
                    ctx.beginPath();
                    ctx.arc(10 + v.x * scale, 10 + v.y * scale, Math.max(3, 0.5 * v.padMm * scale), 0, 2 * Math.PI);
                    ctx.fill();
                    ctx.fillStyle = "#020617";
                    ctx.beginPath();
                    ctx.arc(10 + v.x * scale, 10 + v.y * scale, Math.max(1.5, 0.5 * v.drillMm * scale), 0, 2 * Math.PI);
                    ctx.fill();
                }
            }
        }
    }
}
