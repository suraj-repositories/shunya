import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Rectangle {
    id: root
    color: Theme.bgCanvas

    property int subTab: 0 // 0 = Object-to-Component Converter, 1 = Battery Engineering & Simulation
    property var objectsList: []
    property var componentsList: []
    property var materialsList: []
    property var previewSpec: ({})
    property string compKind: "Resistor"
    property string compName: "Custom_Component"
    property double paramLengthM: 0.20
    property double paramAreaM2: 0.0
    property int paramTurns: 100
    property double paramDielectricM: 0.001
    property string compFeedback: ""

    // Battery state
    property var batSpec: ({})
    property var batEval: ({})
    property var batSim: ({})
    property double dischargeC: 1.0
    property double dischargeDur: 600.0
    property double chargeC: 0.5
    property double chargeDur: 600.0

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        objectsList = pipeline.getObjects();
        componentsList = pipeline.getComponents();
        materialsList = pipeline.getMaterials();
        if (!batSpec || !batSpec.name) {
            batSpec = pipeline.getDefaultBatterySpecMap();
        }
        updateCompPreview();
        if (batSpec && batSpec.name) {
            batEval = pipeline.evaluateBatteryDesign(batSpec);
        }
    }

    function updateCompPreview() {
        if (typeof pipeline === "undefined" || objCombo.currentIndex < 0 || objCombo.currentIndex >= objectsList.length) return;
        var oid = objectsList[objCombo.currentIndex].id;
        previewSpec = pipeline.previewComponentFromObject(oid, compKind, {
            "conductorLength": paramLengthM,
            "crossSectionArea": paramAreaM2,
            "coilTurns": paramTurns,
            "dielectricThickness": paramDielectricM
        });
    }

    function runBatterySim() {
        if (!batSpec || !batSpec.name) return;
        batSim = pipeline.simulateBatteryChargeDischarge(batSpec, dischargeC, dischargeDur, chargeC, chargeDur, 5.0);
        if (batSim.ok && batPlot && batPlot.plotItem) {
            batPlot.plotItem.setMultiSeries([
                {"name": "Voltage (V)", "color": Theme.chartColors.voltage, "points": batSim.voltageSeries || []},
                {"name": "Current (A)", "color": Theme.chartColors.current, "points": batSim.currentSeries || []},
                {"name": "Power (W)", "color": Theme.chartColors.power, "points": batSim.powerSeries || []},
                {"name": "Temp (C)", "color": Theme.chartColors.temperature, "points": batSim.tempSeries || []},
                {"name": "SOC (% / 10)", "color": Theme.chartColors.soc, "points": (batSim.socSeries || []).map(function(p){ return {"x": p.x, "y": p.y / 10.0}; })}
            ]);
        }
    }

    Component.onCompleted: { refreshAll(); runBatterySim(); }
    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onWorkspaceChanged() { root.refreshAll(); }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 14; spacing: 12

        // Header + Sub-mode Tabs
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            ColumnLayout {
                spacing: 2
                Text { text: "Components & Battery Engineering"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXl; font.weight: Font.DemiBold; color: Theme.textPrimary }
                Text { text: "Qualify physical objects into functional components and engineer validated Lithium-ion cells"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
            }
            Item { Layout.fillWidth: true }
            MonoButton { text: "Object-to-Component"; iconName: "components"; active: root.subTab === 0; onClicked: root.subTab = 0 }
            MonoButton { text: "Battery Engineering"; iconName: "battery"; active: root.subTab === 1; onClicked: root.subTab = 1 }
        }

        // TAB 0: Object-to-Component Converter
        RowLayout {
            visible: root.subTab === 0
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12

            Rectangle {
                Layout.preferredWidth: 420; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 10
                    Text { text: "Convert Physical Object to Circuit Component"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary }

                    Text { text: "1. Source Physical Object"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    ComboBox {
                        id: objCombo
                        Layout.fillWidth: true
                        model: root.objectsList
                        textRole: "name"
                        onActivated: root.updateCompPreview()
                    }

                    Text { text: "2. Component Type"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    Flow {
                        Layout.fillWidth: true; spacing: 4
                        Repeater {
                            model: ["Resistor", "ConductorWire", "Switch", "Capacitor", "Inductor", "Connector", "HeatingElement", "Motor"]
                            delegate: MonoButton {
                                text: modelData; compact: true; active: root.compKind === modelData
                                onClicked: { root.compKind = modelData; root.updateCompPreview(); }
                            }
                        }
                    }

                    Text { text: "3. Component Name & Parameters"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    MonoInput { Layout.fillWidth: true; text: root.compName; onTextEdited: (t) => root.compName = t }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Text { text: "Len (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.preferredWidth: 75; monoFont: true; text: String(root.paramLengthM); onEditingFinished: { root.paramLengthM = parseFloat(text) || 0; root.updateCompPreview(); } }
                        Text { text: "Turns:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.preferredWidth: 65; monoFont: true; text: String(root.paramTurns); onEditingFinished: { root.paramTurns = parseInt(text) || 100; root.updateCompPreview(); } }
                    }

                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: qualCol.implicitHeight + 16
                        color: Theme.bgCanvas; border.color: root.previewSpec.isQualified ? Theme.statusSuccess : Theme.statusError; border.width: 1; radius: Theme.radiusSm
                        ColumnLayout {
                            id: qualCol
                            anchors.fill: parent; anchors.margins: 8; spacing: 4
                            RowLayout {
                                MonoBadge {
                                    text: root.previewSpec.isQualified ? "QUALIFIED FOR CIRCUIT" : "DISQUALIFIED"
                                    tone: root.previewSpec.isQualified ? "success" : "error"
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: "Val: " + (root.previewSpec.primaryValue !== undefined ? Number(root.previewSpec.primaryValue).toExponential(3) : "--")
                                    font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary
                                }
                            }
                            Text {
                                text: "R_series=" + Number(root.previewSpec.seriesResistance || 0).toExponential(2) + " Ohm | I_max=" + Number(root.previewSpec.maxCurrent || 0).toFixed(2) + " A | P_max=" + Number(root.previewSpec.maxPower || 0).toFixed(2) + " W"
                                font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textSecondary
                            }
                            Repeater {
                                model: root.previewSpec.errors || []
                                delegate: Text { text: "Error: " + modelData; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.statusError; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            }
                        }
                    }

                    MonoButton {
                        Layout.fillWidth: true
                        text: "Generate & Save to Component Library"
                        variant: "primary"
                        enabled: !!root.previewSpec.isQualified
                        onClicked: {
                            var oid = root.objectsList[objCombo.currentIndex].id;
                            var r = pipeline.generateComponentFromObject(root.compName, oid, root.compKind, {
                                "conductorLength": root.paramLengthM, "crossSectionArea": root.paramAreaM2,
                                "coilTurns": root.paramTurns, "dielectricThickness": root.paramDielectricM
                            });
                            root.compFeedback = r.ok ? "Generated component saved to Library." : ("Failed: " + r.error);
                            root.refreshAll();
                        }
                    }
                    Text { visible: root.compFeedback !== ""; text: root.compFeedback; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textSecondary }
                    Item { Layout.fillHeight: true }
                }
            }

            // Library Table
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 8
                    Text { text: "Component Library (" + root.componentsList.length + " qualified components)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                        model: root.componentsList
                        delegate: Rectangle {
                            width: ListView.view.width; height: 56; radius: Theme.radiusSm
                            color: Theme.bgCanvas; border.color: Theme.borderSubtle; border.width: 1
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 10
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 2
                                    Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                                    Text {
                                        text: modelData.kind + "  |  Primary=" + Number(modelData.primaryValue).toPrecision(4) +
                                              "  |  R=" + Number(modelData.seriesResistance).toExponential(2) +
                                              " Ohm  |  I_max=" + Number(modelData.maxCurrent).toFixed(2) + " A"
                                        font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textMuted
                                    }
                                }
                                MonoBadge { text: modelData.isQualified ? "Ready" : "Invalid"; tone: modelData.isQualified ? "success" : "error" }
                                MonoButton { iconName: "trash"; compact: true; onClicked: pipeline.deleteComponent(modelData.id) }
                            }
                        }
                    }
                }
            }
        }

        // TAB 1: Battery Engineering & Charge/Discharge Simulation
        RowLayout {
            visible: root.subTab === 1
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12

            Rectangle {
                Layout.preferredWidth: 380; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    Text { text: "Lithium Cell Regions & Validation"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    MonoBadge { text: root.batEval.isCompatible ? "Electrochemically Compatible" : "Compatibility Error"; tone: root.batEval.isCompatible ? "success" : "error" }

                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: fedText.implicitHeight + 12; radius: Theme.radiusSm
                        color: Theme.bgElevated; border.color: Theme.borderDefault; border.width: 1
                        Text { id: fedText; anchors.fill: parent; anchors.margins: 6; text: root.batEval.fidelityDisclosure || "L2 Engineering Approximation"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted; wrapMode: Text.WordWrap }
                    }

                    // Derived cell metrics
                    GridLayout {
                        columns: 2; columnSpacing: 12; rowSpacing: 4; Layout.fillWidth: true
                        Text { text: "Nominal Voltage (V):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Text { text: Number(root.batEval.nominalVoltageV || 0).toFixed(2) + " V"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; color: Theme.textPrimary }
                        Text { text: "Capacity (Ah):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Text { text: Number(root.batEval.nominalCapacityAh || 0).toFixed(2) + " Ah"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; color: Theme.textPrimary }
                        Text { text: "Internal R0 (Ohm):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Text { text: Number(root.batEval.internalR0Ohm || 0).toFixed(4) + " Ohm"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                        Text { text: "Total Cell Mass (kg):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Text { text: Number(root.batEval.totalMassKg || 0).toFixed(3) + " kg"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                        Text { text: "Specific Energy (Wh/kg):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Text { text: Number(root.batEval.specificEnergyWhKg || 0).toFixed(1) + " Wh/kg"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                    }

                    // Separator swap test / region override selector
                    Text { text: "Test Region Material Override (Separator)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    ComboBox {
                        id: sepOverrideCombo
                        Layout.fillWidth: true
                        model: root.materialsList
                        textRole: "name"
                        onActivated: {
                            var next = Object.assign({}, root.batSpec);
                            next.separatorMatId = root.materialsList[currentIndex].id;
                            root.batSpec = next;
                            root.batEval = pipeline.evaluateBatteryDesign(root.batSpec);
                        }
                    }

                    Repeater {
                        model: root.batEval.errors || []
                        delegate: MonoBadge { text: modelData; tone: "error" }
                    }
                    Repeater {
                        model: root.batEval.warnings || []
                        delegate: MonoBadge { text: modelData; tone: "warning" }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Text { text: "Disch C:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.preferredWidth: 52; monoFont: true; text: String(root.dischargeC); onEditingFinished: root.dischargeC = parseFloat(text) || 1.0 }
                        Text { text: "Chg C:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.preferredWidth: 52; monoFont: true; text: String(root.chargeC); onEditingFinished: root.chargeC = parseFloat(text) || 0.5 }
                        MonoButton { text: "Simulate"; variant: "primary"; compact: true; onClicked: root.runBatterySim() }
                    }

                    MonoButton {
                        Layout.fillWidth: true
                        text: "Export Battery to Component Library"
                        iconName: "battery"
                        enabled: !!root.batEval.isCompatible
                        onClicked: {
                            var r = pipeline.saveAndExportBattery(root.batSpec);
                            root.compFeedback = r.ok ? "Battery exported to Component Library." : ("Error: " + r.error);
                            root.refreshAll();
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            // Right: Charge/Discharge Telemetry & Multi-Color Plot
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10
                        Text { text: "Charge / Discharge Performance"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary }
                        Item { Layout.fillWidth: true }
                        MonoBadge { text: "Peak Temp: " + Number(root.batSim.peakTempC || 25).toFixed(1) + " C"; tone: root.batSim.hadLimitWarnings ? "warning" : "neutral" }
                        MonoBadge { text: "Energy: " + Number(root.batSim.totalEnergyWh || 0).toFixed(2) + " Wh"; tone: "neutral" }
                    }
                    Repeater {
                        model: root.batSim.warnings || []
                        delegate: MonoBadge { text: modelData; tone: "warning" }
                    }
                    ScientificPlot {
                        id: batPlot
                        Layout.fillWidth: true; Layout.fillHeight: true
                        title: "Battery Terminal Voltage, Current, Power, Temp & SOC"
                        xLabel: "Time (s)"
                        yLabel: "Magnitude"
                    }
                }
            }
        }
    }
}
