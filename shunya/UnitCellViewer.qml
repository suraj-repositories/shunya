import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#161922"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property string crystalId: "Cu-FCC"
    property string crystalTitle: "Copper (FCC)"
    property string spaceGroup: "225 (Fm-3m)"
    property double latticeA: 3.6149
    property double apfValue: 0.7405
    property double netAtoms: 4.0
    property var atomSites: [
        { x: -0.5, y: -0.5, z: -0.5, occ: 0.125 },
        { x:  0.5, y: -0.5, z: -0.5, occ: 0.125 },
        { x:  0.5, y:  0.5, z: -0.5, occ: 0.125 },
        { x: -0.5, y:  0.5, z: -0.5, occ: 0.125 },
        { x: -0.5, y: -0.5, z:  0.5, occ: 0.125 },
        { x:  0.5, y: -0.5, z:  0.5, occ: 0.125 },
        { x:  0.5, y:  0.5, z:  0.5, occ: 0.125 },
        { x: -0.5, y:  0.5, z:  0.5, occ: 0.125 },
        { x:  0.0, y:  0.0, z: -0.5, occ: 0.5 },
        { x:  0.0, y:  0.0, z:  0.5, occ: 0.5 },
        { x:  0.0, y: -0.5, z:  0.0, occ: 0.5 },
        { x:  0.0, y:  0.5, z:  0.0, occ: 0.5 },
        { x: -0.5, y:  0.0, z:  0.0, occ: 0.5 },
        { x:  0.5, y:  0.0, z:  0.0, occ: 0.5 }
    ]
    property real yawDeg: 28.0
    property real pitchDeg: 22.0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: root.crystalTitle + " • SG " + root.spaceGroup
                color: "#e6ebf5"
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Label {
                text: "a = " + root.latticeA.toFixed(4) + " Å | APF = " +
                      (root.apfValue * 100.0).toFixed(1) + "% | Z = " + root.netAtoms.toFixed(1)
                color: "#8fa8cc"
                font.pixelSize: 11
            }
        }

        Canvas {
            id: cellCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width * 0.5;
                const cy = height * 0.5;
                const scale = Math.min(width, height) * 0.42;

                const yaw = root.yawDeg * Math.PI / 180.0;
                const pitch = root.pitchDeg * Math.PI / 180.0;
                const cyaw = Math.cos(yaw), syaw = Math.sin(yaw);
                const cpit = Math.cos(pitch), spit = Math.sin(pitch);

                function proj(p) {
                    const x1 = p.x * cyaw - p.z * syaw;
                    const z1 = p.x * syaw + p.z * cyaw;
                    const y2 = p.y * cpit - z1 * spit;
                    const z2 = p.y * spit + z1 * cpit;
                    return { sx: cx + x1 * scale, sy: cy - y2 * scale, depth: z2 };
                }

                // Cube wireframe edges
                const corners = [
                    {x:-0.5,y:-0.5,z:-0.5}, {x:0.5,y:-0.5,z:-0.5},
                    {x:0.5,y:0.5,z:-0.5},   {x:-0.5,y:0.5,z:-0.5},
                    {x:-0.5,y:-0.5,z:0.5},  {x:0.5,y:-0.5,z:0.5},
                    {x:0.5,y:0.5,z:0.5},    {x:-0.5,y:0.5,z:0.5}
                ];
                const edges = [
                    [0,1],[1,2],[2,3],[3,0],
                    [4,5],[5,6],[6,7],[7,4],
                    [0,4],[1,5],[2,6],[3,7]
                ];

                ctx.strokeStyle = "#4b5a78";
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                for (let i = 0; i < edges.length; ++i) {
                    const p0 = proj(corners[edges[i][0]]);
                    const p1 = proj(corners[edges[i][1]]);
                    ctx.moveTo(p0.sx, p0.sy);
                    ctx.lineTo(p1.sx, p1.sy);
                }
                ctx.stroke();

                // Depth-sorted atomic spheres
                const list = [];
                for (let i = 0; i < root.atomSites.length; ++i) {
                    const s = root.atomSites[i];
                    const pr = proj(s);
                    list.push({ sx: pr.sx, sy: pr.sy, depth: pr.depth, occ: s.occ });
                }
                list.sort((a, b) => a.depth - b.depth);

                for (let i = 0; i < list.length; ++i) {
                    const it = list[i];
                    const r = 10 + 10 * Math.cbrt(it.occ);
                    ctx.fillStyle = it.occ >= 0.99 ? "#f59e0b" : (it.occ >= 0.49 ? "#38bdf8" : "#60a5fa");
                    ctx.beginPath();
                    ctx.arc(it.sx, it.sy, r, 0, 2 * Math.PI);
                    ctx.fill();
                    ctx.strokeStyle = "#0f172a";
                    ctx.lineWidth = 1.2;
                    ctx.stroke();
                }
            }

            MouseArea {
                anchors.fill: parent
                property real lastX: 0
                property real lastY: 0
                onPressed: (mouse) => { lastX = mouse.x; lastY = mouse.y; }
                onPositionChanged: (mouse) => {
                    if (pressed) {
                        root.yawDeg += (mouse.x - lastX) * 0.6;
                        root.pitchDeg += (mouse.y - lastY) * 0.6;
                        lastX = mouse.x;
                        lastY = mouse.y;
                        cellCanvas.requestPaint();
                    }
                }
            }
        }
    }
}
