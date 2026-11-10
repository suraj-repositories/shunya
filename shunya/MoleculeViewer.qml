import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property string moleculeName: "Water (H2O)"
    property string renderStyle: "BallAndStick" // or "SpaceFillingCpk"
    property var atomNodes: [
        { x: 0.0, y: 0.0, z: 0.0, r: 0.31, color: "#dc2626" },
        { x: 0.76, y: 0.59, z: 0.0, r: 0.22, color: "#f8fafc" },
        { x: -0.76, y: 0.59, z: 0.0, r: 0.22, color: "#f8fafc" }
    ]
    property var bondSegments: [
        { ax: 0.0, ay: 0.0, az: 0.0, bx: 0.76, by: 0.59, bz: 0.0 },
        { ax: 0.0, ay: 0.0, az: 0.0, bx: -0.76, by: 0.59, bz: 0.0 }
    ]
    property real yawDeg: 20.0
    property real pitchDeg: 15.0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: root.moleculeName + " (" + root.renderStyle + ")"
                color: "#f1f5f9"
                font.bold: true
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Button {
                text: root.renderStyle === "BallAndStick" ? "CPK Space-Fill" : "Ball & Stick"
                onClicked: {
                    root.renderStyle = (root.renderStyle === "BallAndStick")
                        ? "SpaceFillingCpk" : "BallAndStick";
                    molCanvas.requestPaint();
                }
            }
        }

        Canvas {
            id: molCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width * 0.5;
                const cy = height * 0.5;
                const pxScale = Math.min(width, height) * 0.18;
                const yaw = root.yawDeg * Math.PI / 180.0;
                const pitch = root.pitchDeg * Math.PI / 180.0;

                function proj(x, y, z) {
                    const x1 = x * Math.cos(yaw) - z * Math.sin(yaw);
                    const z1 = x * Math.sin(yaw) + z * Math.cos(yaw);
                    const y2 = y * Math.cos(pitch) - z1 * Math.sin(pitch);
                    const z2 = y * Math.sin(pitch) + z1 * Math.cos(pitch);
                    return { sx: cx + x1 * pxScale, sy: cy - y2 * pxScale, depth: z2 };
                }

                if (root.renderStyle === "BallAndStick") {
                    ctx.strokeStyle = "#94a3b8";
                    ctx.lineWidth = 4.0;
                    ctx.beginPath();
                    for (let i = 0; i < root.bondSegments.length; ++i) {
                        const b = root.bondSegments[i];
                        const p0 = proj(b.ax, b.ay, b.az);
                        const p1 = proj(b.bx, b.by, b.bz);
                        ctx.moveTo(p0.sx, p0.sy);
                        ctx.lineTo(p1.sx, p1.sy);
                    }
                    ctx.stroke();
                }

                const items = [];
                const radScale = root.renderStyle === "SpaceFillingCpk" ? 2.6 : 1.0;
                for (let i = 0; i < root.atomNodes.length; ++i) {
                    const a = root.atomNodes[i];
                    const pr = proj(a.x, a.y, a.z);
                    items.push({ sx: pr.sx, sy: pr.sy, depth: pr.depth, r: a.r * pxScale * radScale, color: a.color });
                }
                items.sort((a, b) => a.depth - b.depth);
                for (let i = 0; i < items.length; ++i) {
                    const it = items[i];
                    ctx.fillStyle = it.color;
                    ctx.beginPath();
                    ctx.arc(it.sx, it.sy, it.r, 0, 2 * Math.PI);
                    ctx.fill();
                    ctx.strokeStyle = "#0f172a";
                    ctx.lineWidth = 1.0;
                    ctx.stroke();
                }
            }
        }
    }
}
