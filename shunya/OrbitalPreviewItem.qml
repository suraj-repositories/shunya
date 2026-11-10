import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property int quantumL: 2 // 0=s, 1=p, 2=d, 3=f
    property int quantumM: 0

    onQuantumLChanged: orbitalCanvas.requestPaint()
    onQuantumMChanged: orbitalCanvas.requestPaint()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Label {
            text: "Orbital |Y(" + root.quantumL + ", " + root.quantumM + ")|² Cross-Section"
            color: "#e2e8f0"
            font.pixelSize: 11
            font.bold: true
        }

        Canvas {
            id: orbitalCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width * 0.5;
                const cy = height * 0.5;
                const scale = Math.min(width, height) * 0.40;

                ctx.strokeStyle = "#334155";
                ctx.beginPath();
                ctx.moveTo(cx - scale, cy);
                ctx.lineTo(cx + scale, cy);
                ctx.moveTo(cx, cy - scale);
                ctx.lineTo(cx, cy + scale);
                ctx.stroke();

                // Polar nodal lobe plot of real spherical harmonic amplitude
                ctx.beginPath();
                for (let i = 0; i <= 180; ++i) {
                    const theta = (2 * Math.PI * i) / 180.0;
                    const ct = Math.cos(theta);
                    let amp = 0.6;
                    if (root.quantumL === 1) amp = Math.abs(ct);
                    else if (root.quantumL === 2) amp = Math.abs(0.5 * (3.0 * ct * ct - 1.0));
                    else if (root.quantumL === 3) amp = Math.abs(0.5 * (5.0 * ct * ct * ct - 3.0 * ct));

                    const rr = scale * amp * amp;
                    const px = cx + rr * Math.sin(theta);
                    const py = cy - rr * Math.cos(theta);
                    if (i === 0) ctx.moveTo(px, py);
                    else ctx.lineTo(px, py);
                }
                ctx.closePath();
                ctx.fillStyle = "rgba(56, 189, 248, 0.35)";
                ctx.fill();
                ctx.strokeStyle = "#38bdf8";
                ctx.lineWidth = 2.0;
                ctx.stroke();
            }
        }
    }
}
