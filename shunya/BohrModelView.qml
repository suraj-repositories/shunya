import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property string elementSymbol: "Cu"
    property int atomicNumber: 29
    property var shellCounts: [2, 8, 18, 1]
    property real phaseRad: 0.0
    property bool animated: true

    Timer {
        interval: 33
        running: root.animated && root.visible
        repeat: true
        onTriggered: {
            root.phaseRad += 0.03;
            bohrCanvas.requestPaint();
        }
    }

    Canvas {
        id: bohrCanvas
        anchors.fill: parent
        anchors.margins: 8

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const cx = width * 0.5;
            const cy = height * 0.5;
            const maxR = Math.min(width, height) * 0.44;
            const count = root.shellCounts.length;

            // Nucleus
            ctx.fillStyle = "#f59e0b";
            ctx.beginPath();
            ctx.arc(cx, cy, 14, 0, 2 * Math.PI);
            ctx.fill();

            ctx.fillStyle = "#0f172a";
            ctx.font = "bold 11px sans-serif";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText(root.elementSymbol, cx, cy);

            // Concentric shells and orbiting electrons
            for (let s = 0; s < count; ++s) {
                const r = 24 + (maxR - 24) * ((s + 1) / Math.max(1, count));
                ctx.strokeStyle = "#334155";
                ctx.lineWidth = 1.2;
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                ctx.stroke();

                const nElec = root.shellCounts[s];
                const dir = (s % 2 === 0) ? 1.0 : -1.0;
                const offset = root.phaseRad * dir / (s + 1);
                for (let e = 0; e < nElec; ++e) {
                    const ang = offset + (2 * Math.PI * e) / Math.max(1, nElec);
                    const ex = cx + r * Math.cos(ang);
                    const ey = cy + r * Math.sin(ang);
                    ctx.fillStyle = "#38bdf8";
                    ctx.beginPath();
                    ctx.arc(ex, ey, 3.5, 0, 2 * Math.PI);
                    ctx.fill();
                }
            }
        }
    }
}
