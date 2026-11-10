import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0f172a"
    border.color: "#334155"
    border.width: 1

    property string sheetTitle: "Root Schematic"
    property double gridPitchMm: 2.54
    property double pixelsPerMm: 4.0
    property var componentsModel: []
    property var wiresModel: []
    property var junctionsModel: []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Schematic • " + root.sheetTitle
                color: "#f8fafc"
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Label {
                text: "Grid: " + root.gridPitchMm.toFixed(2) + " mm"
                color: "#94a3b8"
                font.pixelSize: 11
            }
        }

        Canvas {
            id: schCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const step = root.gridPitchMm * root.pixelsPerMm * 2.0;

                // Subtle orthogonal grid dots
                ctx.fillStyle = "#1e293b";
                for (let x = step; x < width; x += step) {
                    for (let y = step; y < height; y += step) {
                        ctx.fillRect(x, y, 2, 2);
                    }
                }

                // Orthogonal schematic wires
                ctx.strokeStyle = "#22c55e";
                ctx.lineWidth = 2.0;
                ctx.beginPath();
                for (let i = 0; i < root.wiresModel.length; ++i) {
                    const w = root.wiresModel[i];
                    ctx.moveTo(w.x1 * root.pixelsPerMm, w.y1 * root.pixelsPerMm);
                    ctx.lineTo(w.x2 * root.pixelsPerMm, w.y2 * root.pixelsPerMm);
                }
                ctx.stroke();

                // Solder junction dots
                ctx.fillStyle = "#4ade80";
                for (let j = 0; j < root.junctionsModel.length; ++j) {
                    const jp = root.junctionsModel[j];
                    ctx.beginPath();
                    ctx.arc(jp.x * root.pixelsPerMm, jp.y * root.pixelsPerMm, 4.0, 0, 2 * Math.PI);
                    ctx.fill();
                }

                // Component symbols
                for (let c = 0; c < root.componentsModel.length; ++c) {
                    const comp = root.componentsModel[c];
                    const rx = (comp.cx - comp.hw) * root.pixelsPerMm;
                    const ry = (comp.cy - comp.hh) * root.pixelsPerMm;
                    const rw = comp.hw * 2.0 * root.pixelsPerMm;
                    const rh = comp.hh * 2.0 * root.pixelsPerMm;
                    ctx.fillStyle = "#1e293b";
                    ctx.fillRect(rx, ry, rw, rh);
                    ctx.strokeStyle = "#f59e0b";
                    ctx.lineWidth = 1.5;
                    ctx.strokeRect(rx, ry, rw, rh);
                    ctx.fillStyle = "#f8fafc";
                    ctx.font = "bold 11px sans-serif";
                    ctx.fillText(comp.refDes, rx + 4, ry + 14);
                }
            }
        }
    }
}
