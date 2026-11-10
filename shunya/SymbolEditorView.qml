import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property string symbolName: "Resistor"
    property string standardMode: "IEEE_US" // or "IEC_EU"
    property var primitivesModel: []
    property var pinsModel: [
        { number: "1", x: -5.08, y: 0.0 },
        { number: "2", x:  5.08, y: 0.0 }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Symbol Editor • " + root.symbolName + " (" + root.standardMode + ")"
                color: "#f8fafc"
                font.bold: true
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Button {
                text: root.standardMode === "IEEE_US" ? "Switch to IEC (EU)" : "Switch to IEEE (US)"
                onClicked: {
                    root.standardMode = (root.standardMode === "IEEE_US") ? "IEC_EU" : "IEEE_US";
                    symCanvas.requestPaint();
                }
            }
        }

        Canvas {
            id: symCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width * 0.5;
                const cy = height * 0.5;
                const s = 14.0;

                ctx.strokeStyle = "#f59e0b";
                ctx.lineWidth = 2.0;
                if (root.symbolName === "Resistor") {
                    if (root.standardMode === "IEEE_US") {
                        ctx.beginPath();
                        ctx.moveTo(cx - 5.08 * s, cy);
                        ctx.lineTo(cx - 3 * s, cy);
                        ctx.lineTo(cx - 2 * s, cy - s);
                        ctx.lineTo(cx - s, cy + s);
                        ctx.lineTo(cx, cy - s);
                        ctx.lineTo(cx + s, cy + s);
                        ctx.lineTo(cx + 2 * s, cy - s);
                        ctx.lineTo(cx + 3 * s, cy);
                        ctx.lineTo(cx + 5.08 * s, cy);
                        ctx.stroke();
                    } else {
                        ctx.strokeRect(cx - 3 * s, cy - 1.2 * s, 6 * s, 2.4 * s);
                        ctx.beginPath();
                        ctx.moveTo(cx - 5.08 * s, cy);
                        ctx.lineTo(cx - 3 * s, cy);
                        ctx.moveTo(cx + 3 * s, cy);
                        ctx.lineTo(cx + 5.08 * s, cy);
                        ctx.stroke();
                    }
                }

                // Draw pins
                ctx.fillStyle = "#ef4444";
                for (let i = 0; i < root.pinsModel.length; ++i) {
                    const p = root.pinsModel[i];
                    ctx.beginPath();
                    ctx.arc(cx + p.x * s, cy - p.y * s, 4, 0, 2 * Math.PI);
                    ctx.fill();
                }
            }
        }
    }
}
