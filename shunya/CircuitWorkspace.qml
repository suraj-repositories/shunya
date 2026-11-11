import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Rectangle {
    id: root
    color: Theme.bgCanvas

    property var circuitsList: []
    property var libraryComps: []
    property var snap: ({})
    property var dcResult: ({})
    property var trResult: ({})
    property string activeTool: "select" // "select", "wire"
    property var pendingWirePts: []
    property string selectedCompId: ""
    property double zoomScale: 3.0
    property double panX: 160.0
    property double panY: 220.0

    function refreshCircuit() {
        if (typeof pipeline === "undefined") return;
        circuitsList = pipeline.getCircuits();
        libraryComps = pipeline.getComponents();
        snap = pipeline.getActiveCircuitSnapshot();
        canvasItem.requestPaint();
    }

    function runDc() {
        dcResult = pipeline.runCircuitDc();
        canvasItem.requestPaint();
    }

    function runTransient() {
        trResult = pipeline.runCircuitTransient(60.0, 2.0);
        if (trResult.ok && circuitPlot && circuitPlot.plotItem) {
            var series = [];
            if ((trResult.voltageSeries || []).length > 0) series.push({"name": "Voltage (V)", "color": Theme.chartColors.voltage, "points": trResult.voltageSeries});
            if ((trResult.currentSeries || []).length > 0) series.push({"name": "Current (A)", "color": Theme.chartColors.current, "points": trResult.currentSeries});
            if ((trResult.powerSeries || []).length > 0) series.push({"name": "Power (W)", "color": Theme.chartColors.power, "points": trResult.powerSeries});
            if ((trResult.tempSeries || []).length > 0) series.push({"name": "Temp (C)", "color": Theme.chartColors.temperature, "points": trResult.tempSeries});
            if ((trResult.socSeries || []).length > 0) series.push({"name": "SOC (%)", "color": Theme.chartColors.soc, "points": trResult.socSeries});
            circuitPlot.plotItem.setMultiSeries(series);
        }
    }

    function screenToWorld(sx, sy) {
        return {
            "x": Math.round(((sx - panX) / zoomScale) / 5.0) * 5.0,
            "y": Math.round(((sy - panY) / zoomScale) / 5.0) * 5.0
        };
    }

    Component.onCompleted: { refreshCircuit(); runDc(); }
    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onCircuitChanged() { root.refreshCircuit(); }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12; spacing: 10

        // Top Toolbar: Circuit Doc Switcher + Undo/Redo + Tools + Run DC/Transient
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Text { text: "Circuit Design & Simulator"; font.family: Theme.fontSans; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold; color: Theme.textPrimary }

            ComboBox {
                Layout.preferredWidth: 200
                model: root.circuitsList
                textRole: "name"
                currentIndex: typeof pipeline !== "undefined" ? pipeline.activeCircuitIndex : 0
                onActivated: pipeline.selectCircuit(currentIndex)
            }
            MonoButton { text: "New"; iconName: "plus"; compact: true; onClicked: pipeline.newCircuit("") }
            MonoButton { text: "Duplicate"; compact: true; onClicked: pipeline.duplicateActiveCircuit("") }

            Rectangle { width: 1; height: 20; color: Theme.borderDefault }
            MonoButton { text: "Select"; compact: true; active: root.activeTool === "select"; onClicked: { root.activeTool = "select"; root.pendingWirePts = []; canvasItem.requestPaint(); } }
            MonoButton { text: "Wire"; iconName: "wire"; compact: true; active: root.activeTool === "wire"; onClicked: { root.activeTool = "wire"; root.pendingWirePts = []; canvasItem.requestPaint(); } }
            MonoButton { iconName: "undo"; compact: true; enabled: typeof pipeline !== "undefined" && pipeline.canCircuitUndo; onClicked: pipeline.circuitUndo() }
            MonoButton { iconName: "redo"; compact: true; enabled: typeof pipeline !== "undefined" && pipeline.canCircuitRedo; onClicked: pipeline.circuitRedo() }

            Item { Layout.fillWidth: true }
            MonoButton { text: "Run DC"; iconName: "play"; variant: "primary"; compact: true; onClicked: root.runDc() }
            MonoButton { text: "Transient (60s)"; iconName: "simulation"; compact: true; onClicked: root.runTransient() }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10

            // LEFT PALETTE: Standard Components & Object-Generated Components
            Rectangle {
                Layout.preferredWidth: 230; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 6
                    Text { text: "Standard Symbols"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; color: Theme.textMuted }
                    GridLayout {
                        columns: 2; columnSpacing: 4; rowSpacing: 4; Layout.fillWidth: true
                        Repeater {
                            model: [
                                {k: "VoltageSourceDC", lbl: "DC 5V", v: 5.0},
                                {k: "VoltageSourceAC", lbl: "AC 10V", v: 10.0},
                                {k: "Resistor", lbl: "Res 100", v: 100.0},
                                {k: "Capacitor", lbl: "Cap 10u", v: 10e-6},
                                {k: "Inductor", lbl: "Ind 1mH", v: 1e-3},
                                {k: "Diode", lbl: "Diode", v: 0.7},
                                {k: "LED", lbl: "LED", v: 2.0},
                                {k: "Switch", lbl: "Switch", v: 1e-3},
                                {k: "Battery", lbl: "Battery", v: 3.7},
                                {k: "Ground", lbl: "Ground", v: 0.0},
                                {k: "Voltmeter", lbl: "Voltmeter", v: 1e12},
                                {k: "Ammeter", lbl: "Ammeter", v: 0.0}
                            ]
                            delegate: MonoButton {
                                Layout.fillWidth: true
                                text: modelData.lbl; compact: true
                                onClicked: {
                                    var x = 20 + ((root.snap.components ? root.snap.components.length : 0) % 4) * 35;
                                    var y = 15 + Math.floor((root.snap.components ? root.snap.components.length : 0) / 4) * 30;
                                    pipeline.placeCircuitComponent(modelData.k, modelData.lbl, "", x, y, modelData.v, 0.05, 2.0);
                                }
                            }
                        }
                    }

                    Text { text: "Project Generated Library"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; color: Theme.textMuted }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                        model: root.libraryComps
                        delegate: MonoButton {
                            width: ListView.view.width
                            text: "+ " + modelData.name
                            compact: true
                            onClicked: pipeline.placeCircuitComponent(modelData.kind, modelData.name, modelData.id, 45, 25, modelData.primaryValue, 0.05, modelData.capacityAh)
                        }
                    }
                }
            }

            // CENTER SCHEMATIC CANVAS (IEEE/IEC Vector Symbols & Wire Editing)
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: Theme.bgPrimary; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd
                clip: true

                Canvas {
                    id: canvasItem
                    anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.fillStyle = Theme.bgPrimary;
                        ctx.fillRect(0, 0, width, height);

                        // Subtle Grid
                        ctx.strokeStyle = Theme.gridLine;
                        ctx.lineWidth = 1;
                        var step = 10 * root.zoomScale;
                        for (var gx = root.panX % step; gx < width; gx += step) {
                            ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, height); ctx.stroke();
                        }
                        for (var gy = root.panY % step; gy < height; gy += step) {
                            ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke();
                        }

                        function toSx(wx) { return root.panX + wx * root.zoomScale; }
                        function toSy(wy) { return root.panY + wy * root.zoomScale; }

                        // Wires
                        var wires = root.snap.wires || [];
                        ctx.lineWidth = 2;
                        for (var wi = 0; wi < wires.length; ++wi) {
                            var pts = wires[wi].points || [];
                            if (pts.length < 2) continue;
                            ctx.strokeStyle = Theme.statusSuccess;
                            ctx.beginPath();
                            ctx.moveTo(toSx(pts[0].x), toSy(pts[0].y));
                            for (var k = 1; k < pts.length; ++k) ctx.lineTo(toSx(pts[k].x), toSy(pts[k].y));
                            ctx.stroke();
                        }

                        // Pending wire preview
                        if (root.pendingWirePts.length > 0) {
                            ctx.strokeStyle = Theme.statusWarning;
                            ctx.beginPath();
                            ctx.moveTo(toSx(root.pendingWirePts[0].x), toSy(root.pendingWirePts[0].y));
                            for (var p = 1; p < root.pendingWirePts.length; ++p) ctx.lineTo(toSx(root.pendingWirePts[p].x), toSy(root.pendingWirePts[p].y));
                            ctx.stroke();
                        }

                        // Lookup solved DC metrics per component
                        var metricsMap = {};
                        var ms = root.dcResult.componentMetrics || [];
                        for (var mi = 0; mi < ms.length; ++mi) metricsMap[ms[mi].componentId] = ms[mi];

                        // Components (IEEE vector drawings)
                        var comps = root.snap.components || [];
                        for (var ci = 0; ci < comps.length; ++ci) {
                            var c = comps[ci];
                            var cx = toSx(c.xMm), cy = toSy(c.yMm);
                            var isSel = (c.id === root.selectedCompId);
                            var met = metricsMap[c.id];

                            ctx.save();
                            ctx.translate(cx, cy);
                            ctx.rotate((c.rotationDeg || 0) * Math.PI / 180.0);

                            ctx.strokeStyle = (met && met.overLimit) ? Theme.statusError : (isSel ? Theme.textPrimary : Theme.textSecondary);
                            ctx.fillStyle = ctx.strokeStyle;
                            ctx.lineWidth = isSel ? 2.5 : 1.75;
                            var z = root.zoomScale;

                            // Leads
                            if (c.kind !== "Ground") {
                                ctx.beginPath(); ctx.moveTo(-10*z, 0); ctx.lineTo(-4*z, 0); ctx.moveTo(4*z, 0); ctx.lineTo(10*z, 0); ctx.stroke();
                            } else {
                                ctx.beginPath(); ctx.moveTo(0, -5*z); ctx.lineTo(0, 0);
                                ctx.moveTo(-5*z, 0); ctx.lineTo(5*z, 0);
                                ctx.moveTo(-3*z, 2.5*z); ctx.lineTo(3*z, 2.5*z);
                                ctx.moveTo(-1.2*z, 5*z); ctx.lineTo(1.2*z, 5*z);
                                ctx.stroke();
                            }

                            if (c.kind === "Resistor" || c.kind === "HeatingElement") {
                                ctx.strokeRect(-4*z, -2*z, 8*z, 4*z);
                            } else if (c.kind === "Capacitor") {
                                ctx.beginPath(); ctx.moveTo(-1.2*z, -4*z); ctx.lineTo(-1.2*z, 4*z);
                                ctx.moveTo(1.2*z, -4*z); ctx.lineTo(1.2*z, 4*z); ctx.stroke();
                            } else if (c.kind === "Battery" || c.kind === "VoltageSourceDC" || c.kind === "VoltageSourceAC") {
                                ctx.beginPath(); ctx.arc(0, 0, 4.2*z, 0, Math.PI * 2); ctx.stroke();
                                ctx.font = "bold 11px sans-serif"; ctx.textAlign = "center"; ctx.fillText(c.kind === "Battery" ? "BAT" : "V", 0, 4);
                            } else if (c.kind === "Switch") {
                                ctx.beginPath(); ctx.moveTo(-4*z, 0);
                                if (c.switchClosed) ctx.lineTo(4*z, 0); else ctx.lineTo(3*z, -3.5*z);
                                ctx.stroke();
                            } else if (c.kind === "LED" || c.kind === "Diode") {
                                if (met && met.ledLit) {
                                    ctx.fillStyle = Theme.statusWarning;
                                    ctx.beginPath(); ctx.arc(0, 0, 5*z, 0, Math.PI * 2); ctx.fill();
                                }
                                ctx.beginPath(); ctx.moveTo(-3*z, -3*z); ctx.lineTo(3*z, 0); ctx.lineTo(-3*z, 3*z); ctx.closePath(); ctx.stroke();
                                ctx.beginPath(); ctx.moveTo(3*z, -3*z); ctx.lineTo(3*z, 3*z); ctx.stroke();
                            } else if (c.kind !== "Ground") {
                                ctx.beginPath(); ctx.moveTo(-4*z, 0); ctx.lineTo(4*z, 0); ctx.stroke();
                            }
                            ctx.restore();

                            // Labels & Live current readout
                            ctx.fillStyle = Theme.textPrimary;
                            ctx.font = "11px Consolas";
                            ctx.textAlign = "center";
                            var valTxt = c.id + (met ? (" (" + met.currentA.toFixed(2) + "A)") : "");
                            ctx.fillText(valTxt, cx, cy - 16);

                            // Terminal Dots
                            var terms = c.terminals || [];
                            for (var ti = 0; ti < terms.length; ++ti) {
                                ctx.fillStyle = Theme.borderFocus;
                                ctx.beginPath(); ctx.arc(toSx(terms[ti].xMm), toSy(terms[ti].yMm), 3.5, 0, Math.PI * 2); ctx.fill();
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (m) => {
                            var w = root.screenToWorld(m.x, m.y);
                            if (m.button === Qt.RightButton) {
                                if (root.pendingWirePts.length >= 2) pipeline.addCircuitWire(root.pendingWirePts);
                                root.pendingWirePts = [];
                                canvasItem.requestPaint();
                                return;
                            }
                            if (root.activeTool === "wire") {
                                var pts = root.pendingWirePts.slice();
                                pts.push(w);
                                root.pendingWirePts = pts;
                                if (pts.length >= 2) {
                                    // Commit 2-point orthogonal segment on second click cleanly, or wait until right click?
                                }
                                canvasItem.requestPaint();
                                return;
                            }
                            // Hit test component
                            var comps = root.snap.components || [];
                            for (var i = 0; i < comps.length; ++i) {
                                if (Math.hypot(comps[i].xMm - w.x, comps[i].yMm - w.y) < 12.0) {
                                    root.selectedCompId = comps[i].id;
                                    canvasItem.requestPaint();
                                    return;
                                }
                            }
                        }
                        onDoubleClicked: {
                            if (root.activeTool === "wire" && root.pendingWirePts.length >= 2) {
                                pipeline.addCircuitWire(root.pendingWirePts);
                                root.pendingWirePts = [];
                            }
                        }
                        onWheel: (wh) => {
                            root.zoomScale = Math.max(1.0, Math.min(10.0, root.zoomScale * (wh.angleDelta.y > 0 ? 1.15 : 0.87)));
                            canvasItem.requestPaint();
                        }
                    }
                }
            }

            // RIGHT: Component Actions, ERC Validation & Multi-Color Simulation Plot
            Rectangle {
                Layout.preferredWidth: 340; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 8

                    Text { text: "Selected: " + (root.selectedCompId || "None"); font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        enabled: root.selectedCompId !== ""
                        MonoButton { text: "Rotate 90"; compact: true; onClicked: pipeline.rotateCircuitComponent(root.selectedCompId, 90) }
                        MonoButton { text: "Switch Toggle"; compact: true; onClicked: pipeline.toggleCircuitSwitch(root.selectedCompId) }
                        MonoButton { iconName: "trash"; compact: true; onClicked: { pipeline.deleteCircuitComponent(root.selectedCompId); root.selectedCompId = ""; } }
                    }
                    RowLayout {
                        visible: root.activeTool === "wire" && root.pendingWirePts.length >= 2
                        MonoButton {
                            text: "Finish Wire (" + root.pendingWirePts.length + " pts)"
                            variant: "primary"; compact: true
                            onClicked: { pipeline.addCircuitWire(root.pendingWirePts); root.pendingWirePts = []; }
                        }
                    }

                    Text { text: "ERC & Diagnostics"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    RowLayout {
                        spacing: 6
                        MonoBadge { text: (root.snap.errorCount || 0) === 0 ? "ERC Clean" : ((root.snap.errorCount || 0) + " ERC Errors"); tone: (root.snap.errorCount || 0) === 0 ? "success" : "error" }
                        MonoBadge { visible: root.dcResult.ok === true; text: "KCL Res: " + Number(root.dcResult.kclResidualA || 0).toExponential(1) + " A"; tone: "neutral" }
                    }
                    Text { visible: root.dcResult.ok === false; text: root.dcResult.error || ""; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.statusError; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    ListView {
                        Layout.fillWidth: true; Layout.preferredHeight: 115; clip: true; spacing: 3
                        model: root.dcResult.componentMetrics || []
                        delegate: Rectangle {
                            width: ListView.view.width; height: 26; radius: Theme.radiusSm; color: Theme.bgCanvas
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 6
                                Text { text: modelData.componentId; Layout.preferredWidth: 46; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; color: Theme.textPrimary }
                                Text { text: "V=" + Number(modelData.voltageV).toFixed(2) + "V  I=" + Number(modelData.currentA).toFixed(3) + "A  P=" + Number(modelData.powerW).toFixed(2) + "W"; Layout.fillWidth: true; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textSecondary }
                            }
                        }
                    }

                    ScientificPlot {
                        id: circuitPlot
                        Layout.fillWidth: true; Layout.fillHeight: true
                        title: "Circuit Transient Curves"
                        xLabel: "Time (s)"
                        yLabel: "V / A / W / C / SOC"
                    }
                }
            }
        }
    }
}
