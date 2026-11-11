import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Rectangle {
    id: root
    color: Theme.bgCanvas

    property string expId: ""
    property string expName: "Custom_Cu_Ni_Alloy"
    property string scenario: "Alloy"
    property double tempK: 298.15
    property double pressurePa: 101325.0
    property bool oxidizing: true
    property double densityOverride: 0.0
    property var ingredients: [
        {"symbol": "Cu", "basis": 1, "amount": 70.0, "layerThickness": 0.001, "isMatrix": true},
        {"symbol": "Ni", "basis": 1, "amount": 30.0, "layerThickness": 0.001, "isMatrix": false}
    ]
    property string newSymbol: "Fe"
    property double newAmount: 10.0

    property var evalResult: ({})
    property var savedMaterials: []
    property var savedExperiments: []
    property var presetList: []
    property string actionFeedback: ""

    function evaluateAndRefresh() {
        if (typeof pipeline === "undefined") return;
        evalResult = pipeline.evaluateMixture(expName, scenario, tempK, pressurePa, oxidizing, densityOverride, ingredients);
        savedMaterials = pipeline.getMaterials();
        savedExperiments = pipeline.getMixtureExperiments();
        presetList = pipeline.getStandardMixturePresets();

        // Update T-sweep scientific plot
        var sweep = pipeline.sweepMixtureTemperature(scenario, pressurePa, oxidizing, ingredients, 200.0, 1200.0, 25);
        var kPts = [], rhoPts = [];
        for (var i = 0; i < sweep.length; ++i) {
            kPts.push({"x": sweep[i].tempK, "y": sweep[i].thermalK});
            rhoPts.push({"x": sweep[i].tempK, "y": sweep[i].density / 100.0});
        }
        if (sweepPlot && sweepPlot.plotItem) {
            sweepPlot.plotItem.setMultiSeries([
                {"name": "Thermal k (W/mK)", "color": Theme.chartColors.temperature, "points": kPts},
                {"name": "Density / 100 (kg/m3)", "color": Theme.chartColors.density, "points": rhoPts}
            ]);
        }
    }

    Component.onCompleted: evaluateAndRefresh()
    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onWorkspaceChanged() {
            root.savedMaterials = pipeline.getMaterials();
            root.savedExperiments = pipeline.getMixtureExperiments();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // Top Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                spacing: 2
                Text { text: "Material Mixing Laboratory & Property Engine"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXl; font.weight: Font.DemiBold; color: Theme.textPrimary }
                Text { text: "Combine elements across 6 formation scenarios with real chemical rules, comparison metrics, and reactive propagation"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
            }
            Item { Layout.fillWidth: true }
            MonoButton {
                text: "Save Experiment"
                iconName: "save"
                onClicked: {
                    var r = pipeline.saveAndPromoteMixture(root.expId, root.expName, root.scenario, root.tempK, root.pressurePa, root.oxidizing, root.densityOverride, root.ingredients, false);
                    root.actionFeedback = r.ok ? "Saved experiment '" + root.expName + "'." : ("Error: " + r.error);
                    root.evaluateAndRefresh();
                }
            }
            MonoButton {
                text: "Promote to Material"
                variant: "primary"
                iconName: "plus"
                onClicked: {
                    var r = pipeline.saveAndPromoteMixture(root.expId, root.expName, root.scenario, root.tempK, root.pressurePa, root.oxidizing, root.densityOverride, root.ingredients, true);
                    root.actionFeedback = r.ok ? "Promoted '" + root.expName + "' to reusable Project Material." : ("Rejected: " + r.error);
                    root.evaluateAndRefresh();
                }
            }
        }

        // Main Split: Left Composer | Center Results & Comparison | Right Material Catalog
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // LEFT: Recipe & Conditions
            Rectangle {
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    Text { text: "Presets & Saved Experiments"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        ComboBox {
                            id: presetCombo
                            Layout.fillWidth: true
                            model: root.presetList
                            textRole: "name"
                        }
                        MonoButton {
                            text: "Load"
                            compact: true
                            onClicked: {
                                if (presetCombo.currentIndex >= 0 && presetCombo.currentIndex < root.presetList.length) {
                                    var p = root.presetList[presetCombo.currentIndex];
                                    root.expId = "";
                                    root.expName = p.name;
                                    root.scenario = p.scenario;
                                    root.tempK = p.tempK;
                                    root.pressurePa = p.pressurePa;
                                    root.oxidizing = p.oxidizing;
                                    root.ingredients = p.inputs;
                                    root.evaluateAndRefresh();
                                }
                            }
                        }
                    }

                    Text { text: "Mixture Name"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    MonoInput {
                        Layout.fillWidth: true
                        text: root.expName
                        onTextEdited: (t) => { root.expName = t; }
                    }

                    Text { text: "Formation Scenario"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    Flow {
                        Layout.fillWidth: true; spacing: 4
                        Repeater {
                            model: ["PhysicalMixture", "Alloy", "Solution", "ChemicalCompound", "LayeredMaterial", "CompositeMaterial"]
                            delegate: MonoButton {
                                text: modelData
                                compact: true
                                active: root.scenario === modelData
                                onClicked: { root.scenario = modelData; root.evaluateAndRefresh(); }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Text { text: "T (K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.preferredWidth: 74; monoFont: true; text: root.tempK.toFixed(1); onEditingFinished: { root.tempK = Math.max(1.0, parseFloat(text) || 298.15); root.evaluateAndRefresh(); } }
                        Text { text: "P (kPa):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.preferredWidth: 74; monoFont: true; text: (root.pressurePa / 1000.0).toFixed(2); onEditingFinished: { root.pressurePa = Math.max(0.0, (parseFloat(text) || 101.325) * 1000.0); root.evaluateAndRefresh(); } }
                    }

                    Text { text: "Constituent Elements"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        MonoInput { Layout.preferredWidth: 68; placeholderText: "Sym (Li)"; text: root.newSymbol; onTextEdited: (t) => root.newSymbol = t }
                        MonoInput { Layout.preferredWidth: 72; monoFont: true; text: String(root.newAmount); onTextEdited: (t) => root.newAmount = parseFloat(t) || 0 }
                        MonoButton {
                            text: "Add"
                            compact: true
                            onClicked: {
                                if (root.newSymbol.trim() !== "" && root.newAmount > 0) {
                                    var next = root.ingredients.slice();
                                    next.push({"symbol": root.newSymbol.trim(), "basis": 1, "amount": root.newAmount, "layerThickness": 0.001, "isMatrix": false});
                                    root.ingredients = next;
                                    root.evaluateAndRefresh();
                                }
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: root.ingredients
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 32
                            color: Theme.bgCanvas; border.color: Theme.borderSubtle; border.width: 1; radius: Theme.radiusSm
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 6; spacing: 6
                                Text { text: modelData.symbol; Layout.preferredWidth: 42; font.family: Theme.fontMono; font.pixelSize: Theme.fontMd; font.weight: Font.Bold; color: Theme.textPrimary }
                                Text { text: Number(modelData.amount).toFixed(2) + (modelData.basis === 2 ? " mol" : " %"); Layout.fillWidth: true; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textSecondary }
                                MonoButton {
                                    iconName: "trash"
                                    compact: true
                                    onClicked: {
                                        var next = root.ingredients.slice();
                                        next.splice(index, 1);
                                        root.ingredients = next;
                                        root.evaluateAndRefresh();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // CENTER: Validation, Properties, Comparison & Sweep Graph
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        MonoBadge {
                            text: root.evalResult.isChemicallyValid ? "Chemically Valid Formation" : "Invalid Formation Rule"
                            tone: root.evalResult.isChemicallyValid ? "success" : "error"
                        }
                        MonoBadge { text: root.evalResult.overallEpistemic || "ApproximateSimulation"; tone: "neutral" }
                        MonoBadge { text: "Phase: " + (root.evalResult.predictedPhase || "--"); tone: "neutral" }
                        Item { Layout.fillWidth: true }
                        Text { visible: root.actionFeedback !== ""; text: root.actionFeedback; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textSecondary }
                    }

                    // Errors & Warnings
                    Repeater {
                        model: root.evalResult.validationErrors || []
                        delegate: Rectangle {
                            Layout.fillWidth: true; implicitHeight: 28; radius: Theme.radiusSm
                            color: Qt.rgba(Theme.statusError.r, Theme.statusError.g, Theme.statusError.b, 0.12)
                            border.color: Theme.statusError; border.width: 1
                            Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.margins: 8; text: modelData; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.statusError }
                        }
                    }
                    Repeater {
                        model: root.evalResult.warnings || []
                        delegate: MonoBadge { text: modelData; tone: "warning" }
                    }

                    // Element-vs-Mixture Comparison Matrix
                    Text { text: "Base Elements vs. Resulting Mixture Comparison"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 195
                        clip: true
                        spacing: 3
                        model: root.evalResult.comparisonRows || []
                        delegate: Rectangle {
                            width: ListView.view.width; height: 28; radius: Theme.radiusSm
                            color: index % 2 === 0 ? Theme.bgCanvas : Theme.bgElevated
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 8
                                Text { text: modelData.property; Layout.preferredWidth: 150; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textSecondary }
                                Text { text: modelData.mixtureValue !== undefined ? Number(modelData.mixtureValue).toPrecision(4) + " " + modelData.unit : "Unsupported"; Layout.preferredWidth: 130; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; color: Theme.textPrimary }
                                Text { text: modelData.elementsSummary; Layout.fillWidth: true; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textMuted; elide: Text.ElideRight }
                            }
                        }
                    }

                    // Temperature Property Curve Plot (Scientific Multi-Color)
                    ScientificPlot {
                        id: sweepPlot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "Property vs. Temperature Sweep (200 K - 1200 K)"
                        xLabel: "Temperature (K)"
                        yLabel: "Value"
                    }
                }
            }

            // RIGHT: Registered Project Materials (with live reactive density/conductivity edit)
            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    Text { text: "Project Materials (" + root.savedMaterials.length + ")"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    Text { text: "Click +10% Density to test reactive Object & Component recalculation"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                        model: root.savedMaterials
                        delegate: Rectangle {
                            width: ListView.view.width; height: 74; radius: Theme.radiusSm
                            color: Theme.bgCanvas; border.color: Theme.borderSubtle; border.width: 1
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 8; spacing: 3
                                Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
                                Text {
                                    text: "rho=" + Number(modelData.density).toFixed(1) + " kg/m3  k=" + Number(modelData.thermalConductivity).toFixed(1) + " W/mK"
                                    font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textMuted
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { text: "sigma=" + Number(modelData.electricalConductivity).toExponential(1) + " S/m"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; Layout.fillWidth: true }
                                    MonoButton {
                                        text: "+10% rho"
                                        compact: true
                                        onClicked: {
                                            pipeline.updateMaterialParameters(modelData.id, modelData.density * 1.1, -1, -1, -1);
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
}
