import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Rectangle {
    id: root
    color: Theme.bgSurface
    border.color: Theme.borderDefault
    border.width: 1
    radius: Theme.radiusMd
    clip: true

    property string materialName: "Current Mixture"
    property string scenario: "Alloy"
    property real density: 5000.0
    property real thermalK: 30.0
    property real electricalSigma: 5.0e6
    property real meltingK: 1200.0
    property var ingredients: []

    property var compData: ({})
    property var rankingData: ({})
    property string activeGoal: "Maximum Electrical Conductivity"

    readonly property var metricLabels: [
        "Elec. Cond.",
        "Thermal Cond.",
        "Temp. Tol.",
        "Corrosion Res.",
        "Lightweight",
        "Mech. Strength",
        "Abundance",
        "EMI Shielding"
    ]

    function refresh() {
        if (typeof pipeline === "undefined") return;
        compData = pipeline.compareMaterialMetrics(
            materialName, scenario, density, thermalK, electricalSigma, meltingK, ingredients
        );
        rankingData = pipeline.rankMaterialsForGoal(
            activeGoal, materialName, scenario, density, thermalK, electricalSigma, meltingK, ingredients
        );
        radarCanvas.requestPaint();
    }

    onIngredientsChanged: refresh()
    onScenarioChanged: refresh()
    onDensityChanged: refresh()
    onThermalKChanged: refresh()
    onElectricalSigmaChanged: refresh()
    onMeltingKChanged: refresh()
    Component.onCompleted: refresh()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // Header & Goal Selection
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                spacing: 2
                Text {
                    text: "Material Comparison & Spider Radar Plot"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: "8-Axis Normalized Engineering Metric Benchmark"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Goal:"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                color: Theme.textSecondary
            }

            MonoComboBox {
                id: goalCombo
                implicitWidth: 220
                implicitHeight: 28
                model: [
                    "Maximum Electrical Conductivity",
                    "High-Temperature Structural Service",
                    "Marine & Harsh Corrosion Resistance",
                    "Ultra-Lightweight High Strength",
                    "Electromagnetic Shielding (EMI)"
                ]
                onActivated: {
                    root.activeGoal = currentText;
                    root.refresh();
                }
            }
        }

        // Winner Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 28
            radius: Theme.radiusSm
            color: Qt.rgba(Theme.statusSuccess.r, Theme.statusSuccess.g, Theme.statusSuccess.b, 0.12)
            border.color: Theme.statusSuccess
            border.width: 1
            visible: !!(root.rankingData && root.rankingData.winnerName)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6
                Text {
                    text: "🏆 " + (root.rankingData && root.rankingData.explanation ? root.rankingData.explanation : "")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.Medium
                    color: Theme.statusSuccess
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // Main Visual Area: Radar Plot (left) + Comparative Ranking / Matrix (right)
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // Radar Canvas
            Rectangle {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                color: Theme.bgElevated
                radius: Theme.radiusSm
                border.color: Theme.borderDefault
                border.width: 1

                Canvas {
                    id: radarCanvas
                    anchors.fill: parent
                    anchors.margins: 10

                    onPaint: {
                        var ctx = getContext("2d");
                        var w = width;
                        var h = height;
                        ctx.clearRect(0, 0, w, h);

                        var cx = w / 2;
                        var cy = h / 2;
                        var r = Math.min(cx, cy) - 24;
                        if (r <= 10) return;

                        var numAxes = 8;
                        // Draw concentric guide webs
                        ctx.strokeStyle = Theme.borderDefault;
                        ctx.lineWidth = 1;

                        for (var level = 1; level <= 4; ++level) {
                            var curR = r * (level / 4.0);
                            ctx.beginPath();
                            for (var a = 0; a < numAxes; ++a) {
                                var angle = (a * 2 * Math.PI / numAxes) - Math.PI / 2;
                                var x = cx + curR * Math.cos(angle);
                                var y = cy + curR * Math.sin(angle);
                                if (a === 0) ctx.moveTo(x, y);
                                else ctx.lineTo(x, y);
                            }
                            ctx.closePath();
                            ctx.stroke();
                        }

                        // Draw Axes & Labels
                        ctx.fillStyle = Theme.textMuted;
                        ctx.font = "9px " + Theme.fontSans;
                        ctx.textAlign = "center";
                        ctx.textBaseline = "middle";

                        for (var i = 0; i < numAxes; ++i) {
                            var ang = (i * 2 * Math.PI / numAxes) - Math.PI / 2;
                            var axX = cx + r * Math.cos(ang);
                            var axY = cy + r * Math.sin(ang);
                            ctx.beginPath();
                            ctx.moveTo(cx, cy);
                            ctx.lineTo(axX, axY);
                            ctx.stroke();

                            // Label placement
                            var lx = cx + (r + 14) * Math.cos(ang);
                            var ly = cy + (r + 14) * Math.sin(ang);
                            ctx.fillText(root.metricLabels[i], lx, ly);
                        }

                        // Draw Candidate Polygons
                        var mats = (root.compData && root.compData.materials) || [];
                        var colors = [
                            Theme.chartLine1 || "#2563eb",
                            Theme.chartLine2 || "#10b981",
                            Theme.chartLine3 || "#f59e0b",
                            Theme.chartLine4 || "#ef4444"
                        ];

                        for (var mIdx = 0; mIdx < Math.min(mats.length, 4); ++mIdx) {
                            var mat = mats[mIdx];
                            var scores = mat.radarScores || [];
                            if (scores.length < numAxes) continue;

                            var col = colors[mIdx % colors.length];
                            ctx.strokeStyle = col;
                            ctx.lineWidth = (mIdx === 0) ? 2.5 : 1.5;
                            ctx.fillStyle = Qt.rgba(
                                parseInt(col.substr(1,2), 16)/255,
                                parseInt(col.substr(3,2), 16)/255,
                                parseInt(col.substr(5,2), 16)/255,
                                (mIdx === 0) ? 0.25 : 0.08
                            );

                            ctx.beginPath();
                            for (var k = 0; k < numAxes; ++k) {
                                var polyAng = (k * 2 * Math.PI / numAxes) - Math.PI / 2;
                                var val = scores[k] || 0.15;
                                var px = cx + (r * val) * Math.cos(polyAng);
                                var py = cy + (r * val) * Math.sin(polyAng);
                                if (k === 0) ctx.moveTo(px, py);
                                else ctx.lineTo(px, py);
                            }
                            ctx.closePath();
                            ctx.fill();
                            ctx.stroke();
                        }
                    }
                }
            }

            // Right Table: Candidate Comparison Table & Ranked List
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                Text {
                    text: "Material Benchmarks & Rankings"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 4

                        Repeater {
                            model: (root.rankingData && root.rankingData.rankings) || []
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 32
                                radius: Theme.radiusSm
                                color: (index === 0) ? Qt.rgba(Theme.accentPrimary.r, Theme.accentPrimary.g, Theme.accentPrimary.b, 0.12) : Theme.bgElevated
                                border.color: (index === 0) ? Theme.accentPrimary : Theme.borderDefault
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    Text {
                                        text: "#" + (index + 1)
                                        font.family: Theme.fontMono
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.Bold
                                        color: (index === 0) ? Theme.accentPrimary : Theme.textMuted
                                    }

                                    Text {
                                        text: modelData.name
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: (index === 0) ? Font.DemiBold : Font.Normal
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: Number(modelData.score).toFixed(1) + " pts"
                                        font.family: Theme.fontMono
                                        font.pixelSize: Theme.fontXs
                                        color: (index === 0) ? Theme.accentPrimary : Theme.textSecondary
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
