import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtCore

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
    property int currentBasis: 1 // 1: wt% (mass fraction), 2: mol% (mole fraction)
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

    // Draggable Splitter Dimensions & Persistence
    property real leftCardWidth: 340
    property real rightCardWidth: 280
    property real compTableHeight: 180

    Settings {
        id: mixLabSettings
        category: "MixingLabLayout"
        property alias leftCardWidth: root.leftCardWidth
        property alias rightCardWidth: root.rightCardWidth
        property alias compTableHeight: root.compTableHeight
    }

    function totalComposition() {
        var sum = 0.0;
        for (var i = 0; i < root.ingredients.length; ++i) {
            sum += Number(root.ingredients[i].amount) || 0.0;
        }
        return sum;
    }

    function isCompositionBalanced() {
        return Math.abs(totalComposition() - 100.0) < 0.05;
    }

    function normalizeIngredients() {
        var tot = totalComposition();
        if (tot <= 0.0) return;
        var next = [];
        for (var i = 0; i < root.ingredients.length; ++i) {
            var ing = Object.assign({}, root.ingredients[i]);
            // Proportional scaling: w_i' = (w_i / sum) * 100.0
            ing.amount = Math.round((Number(ing.amount) / tot) * 10000.0) / 100.0;
            next.push(ing);
        }
        root.ingredients = next;
        root.evaluateAndRefresh();
        root.actionFeedback = "Composition normalized to 100.0%.";
    }

    function addOrMergeConstituent() {
        var sym = root.newSymbol.trim();
        if (sym === "" || root.newAmount <= 0) return;
        var next = [];
        var merged = false;
        for (var i = 0; i < root.ingredients.length; ++i) {
            var ing = Object.assign({}, root.ingredients[i]);
            if (ing.symbol.toUpperCase() === sym.toUpperCase()) {
                ing.amount = Number(ing.amount) + Number(root.newAmount);
                merged = true;
            }
            next.push(ing);
        }
        if (!merged) {
            next.push({
                "symbol": sym,
                "basis": root.currentBasis,
                "amount": Number(root.newAmount),
                "layerThickness": 0.001,
                "isMatrix": (next.length === 0)
            });
        }
        root.ingredients = next;
        root.newSymbol = "";
        root.evaluateAndRefresh();
    }

    function evaluateAndRefresh() {
        if (typeof pipeline === "undefined") return;
        evalResult = pipeline.evaluateMixture(expName, scenario, tempK, pressurePa, oxidizing, densityOverride, ingredients);
        savedMaterials = pipeline.getMaterials();
        savedExperiments = pipeline.getMixtureExperiments();
        presetList = pipeline.getStandardMixturePresets();

        // Update T-sweep scientific plot across calibrated [200.0, 1200.0] K range
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

    Component.onCompleted: {
        evaluateAndRefresh();
    }

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
        spacing: 10

        // Top Header Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                spacing: 2
                Text {
                    text: "Material Mixing Laboratory & Property Engine"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: "Combine elements across 6 formation scenarios with real chemical rules, comparison metrics, and reactive propagation"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    color: Theme.textMuted
                }
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

        // Main 3-Pane Responsive Layout with Draggable StudioSplitters
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 1. LEFT CARD: Recipe, Scenario & Ingredients Composer
            Rectangle {
                Layout.preferredWidth: root.leftCardWidth
                Layout.minimumWidth: 280
                Layout.maximumWidth: 600
                Layout.fillHeight: true
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd
                clip: true

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 12
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        Text {
                            text: "Presets & Saved Experiments"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            MonoComboBox {
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
                                        root.actionFeedback = "Loaded preset '" + p.name + "'.";
                                    }
                                }
                            }
                        }

                        Text {
                            text: "Mixture Name"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textMuted
                        }
                        MonoInput {
                            Layout.fillWidth: true
                            text: root.expName
                            onTextEdited: (t) => { root.expName = t; }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: "Formation Scenario"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.textMuted
                            }
                            Item { Layout.fillWidth: true }
                            MonoInfoButton {
                                title: "Formation Scenarios"
                                description: "Governs the thermodynamic and lattice assumptions used to compute mixture properties."
                                formula: "rho_eff = sum(w_i * rho_i)"
                                helpAnchor: "formation-scenarios"
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 4
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
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: "T (K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            MonoInput {
                                Layout.preferredWidth: 74
                                monoFont: true
                                text: root.tempK.toFixed(1)
                                onEditingFinished: { root.tempK = Math.max(1.0, parseFloat(text) || 298.15); root.evaluateAndRefresh(); }
                            }
                            Text { text: "P (kPa):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            MonoInput {
                                Layout.preferredWidth: 74
                                monoFont: true
                                text: (root.pressurePa / 1000.0).toFixed(2)
                                onEditingFinished: { root.pressurePa = Math.max(0.0, (parseFloat(text) || 101.325) * 1000.0); root.evaluateAndRefresh(); }
                            }
                        }

                        // Constituent Elements Header & Live Total Percentage Badge
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: "Constituent Elements"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            MonoInfoButton {
                                title: "Constituent Stoichiometry"
                                description: "Input element symbols and percentage amounts. Sum must equal 100.0%."
                                formula: "sum(fraction_i) = 100.0%"
                                helpAnchor: "constituent-normalization"
                            }
                            Item { Layout.fillWidth: true }
                            MonoBadge {
                                text: "Total: " + root.totalComposition().toFixed(1) + "%"
                                tone: root.isCompositionBalanced() ? "success" : "warning"
                            }
                        }

                        // Imbalance Warning Banner with 1-Click Normalize Action
                        Rectangle {
                            visible: !root.isCompositionBalanced() && root.ingredients.length > 0
                            Layout.fillWidth: true
                            height: 32
                            radius: Theme.radiusSm
                            color: Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.12)
                            border.color: Theme.statusWarning
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 6
                                spacing: 6
                                Text {
                                    text: "Total is " + root.totalComposition().toFixed(1) + "%, normalize to 100%?"
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.statusWarning
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                MonoButton {
                                    text: "Normalize"
                                    compact: true
                                    onClicked: root.normalizeIngredients()
                                }
                            }
                        }

                        // Add Element Row with Symbol Autocomplete & Duplicate Merge
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            MonoInput {
                                id: symInput
                                Layout.preferredWidth: 74
                                placeholderText: "Sym (Fe)"
                                text: root.newSymbol
                                onTextEdited: (t) => root.newSymbol = t
                            }
                            MonoInput {
                                Layout.preferredWidth: 68
                                monoFont: true
                                text: String(root.newAmount)
                                onTextEdited: (t) => root.newAmount = parseFloat(t) || 0
                            }
                            MonoButton {
                                text: "Add"
                                compact: true
                                onClicked: root.addOrMergeConstituent()
                            }
                        }

                        // Ingredients List
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Repeater {
                                model: root.ingredients
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    height: 32
                                    color: Theme.bgCanvas
                                    border.color: Theme.borderSubtle
                                    border.width: 1
                                    radius: Theme.radiusSm

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            text: modelData.symbol
                                            Layout.preferredWidth: 38
                                            font.family: Theme.fontMono
                                            font.pixelSize: Theme.fontMd
                                            font.weight: Font.Bold
                                            color: Theme.textPrimary
                                        }
                                        Text {
                                            text: Number(modelData.amount).toFixed(2) + (modelData.basis === 2 ? " mol" : " %")
                                            Layout.fillWidth: true
                                            font.family: Theme.fontMono
                                            font.pixelSize: Theme.fontSm
                                            color: Theme.textSecondary
                                        }
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
                }
            }

            // Draggable Splitter between Left and Center Card
            StudioSplitter {
                orientation: Qt.Vertical
                minSize: 280
                maxSize: 600
                defaultSize: 340
                onDragDelta: (delta) => {
                    root.leftCardWidth = Math.max(280, Math.min(600, root.leftCardWidth + delta));
                }
                onResetRequested: {
                    root.leftCardWidth = 340;
                }
            }

            // 2. CENTER CARD: Results, Comparison Matrix & Sweep Chart with Internal Splitter
            Rectangle {
                Layout.fillWidth: true
                Layout.minimumWidth: 360
                Layout.fillHeight: true
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    // Status Chips Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        MonoBadge {
                            text: root.evalResult.isChemicallyValid ? "Chemically Valid Formation" : "Invalid Formation Rule"
                            tone: root.evalResult.isChemicallyValid ? "success" : "error"
                        }
                        MonoBadge {
                            text: root.evalResult.overallEpistemic || "ApproximateSimulation"
                            tone: "neutral"
                        }
                        MonoBadge {
                            text: "Phase: " + (root.evalResult.predictedPhase || "Solid")
                            tone: "neutral"
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            visible: root.actionFeedback !== ""
                            text: root.actionFeedback
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }
                    }

                    // Validation Errors & Warnings
                    Repeater {
                        model: root.evalResult.validationErrors || []
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 28
                            radius: Theme.radiusSm
                            color: Qt.rgba(Theme.statusError.r, Theme.statusError.g, Theme.statusError.b, 0.12)
                            border.color: Theme.statusError
                            border.width: 1
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.margins: 8
                                text: modelData
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                color: Theme.statusError
                            }
                        }
                    }
                    Repeater {
                        model: root.evalResult.warnings || []
                        delegate: MonoBadge { text: modelData; tone: "warning" }
                    }

                    // Element-vs-Mixture Comparison Matrix
                    Text {
                        text: "Base Elements vs. Resulting Mixture Comparison"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    ListView {
                        id: compListView
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.compTableHeight
                        Layout.minimumHeight: 120
                        clip: true
                        spacing: 3
                        model: root.evalResult.comparisonRows || []
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 28
                            radius: Theme.radiusSm
                            color: index % 2 === 0 ? Theme.bgCanvas : Theme.bgElevated

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                Text {
                                    text: modelData.property
                                    Layout.preferredWidth: 160
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.mixtureValue !== undefined ? Number(modelData.mixtureValue).toPrecision(4) + " " + modelData.unit : "Unsupported"
                                    Layout.preferredWidth: 140
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }
                                Text {
                                    text: modelData.elementsSummary || ""
                                    Layout.fillWidth: true
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    // Internal Draggable Horizontal Splitter
                    StudioSplitter {
                        orientation: Qt.Horizontal
                        minSize: 120
                        maxSize: 400
                        defaultSize: 180
                        onDragDelta: (delta) => {
                            root.compTableHeight = Math.max(120, Math.min(400, root.compTableHeight + delta));
                        }
                        onResetRequested: {
                            root.compTableHeight = 180;
                        }
                    }

                    // Scientific Property vs. Temperature Sweep Chart
                    ScientificPlot {
                        id: sweepPlot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 160
                        title: "Property vs. Temperature Sweep (200 K - 1200 K)"
                        xLabel: "Temperature (K)"
                        yLabel: "Value"
                    }
                }
            }

            // Draggable Splitter between Center and Right Card
            StudioSplitter {
                orientation: Qt.Vertical
                minSize: 240
                maxSize: 500
                defaultSize: 280
                onDragDelta: (delta) => {
                    root.rightCardWidth = Math.max(240, Math.min(500, root.rightCardWidth - delta));
                }
                onResetRequested: {
                    root.rightCardWidth = 280;
                }
            }

            // 3. RIGHT CARD: Registered Project Materials Catalog & Virtual Test Bench
            Rectangle {
                Layout.preferredWidth: root.rightCardWidth
                Layout.minimumWidth: 240
                Layout.maximumWidth: 500
                Layout.fillHeight: true
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd
                clip: true

                property string rightTab: "materials" // "materials" | "test_bench" | "atomic_bonds" | "compare" | "explain"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    // Header with Switchable Mode Tabs
                    Flow {
                        Layout.fillWidth: true
                        spacing: 4
                        MonoButton {
                            text: "Materials (" + root.savedMaterials.length + ")"
                            compact: true
                            active: parent.parent.parent.rightTab === "materials"
                            onClicked: parent.parent.parent.rightTab = "materials"
                        }
                        MonoButton {
                            text: "Test Bench"
                            compact: true
                            iconName: "play"
                            active: parent.parent.parent.rightTab === "test_bench"
                            onClicked: parent.parent.parent.rightTab = "test_bench"
                        }
                        MonoButton {
                            text: "Bonds"
                            compact: true
                            active: parent.parent.parent.rightTab === "atomic_bonds"
                            onClicked: parent.parent.parent.rightTab = "atomic_bonds"
                        }
                        MonoButton {
                            text: "Compare"
                            compact: true
                            active: parent.parent.parent.rightTab === "compare"
                            onClicked: parent.parent.parent.rightTab = "compare"
                        }
                        MonoButton {
                            text: "Explain"
                            compact: true
                            active: parent.parent.parent.rightTab === "explain"
                            onClicked: parent.parent.parent.rightTab = "explain"
                        }
                    }

                    // TAB 1: Registered Materials Catalog
                    ColumnLayout {
                        visible: parent.parent.rightTab === "materials"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6

                        Text {
                            text: "Click +10% Density to test reactive Object & Component recalculation"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: root.savedMaterials
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 74
                                radius: Theme.radiusSm
                                color: Theme.bgCanvas
                                border.color: Theme.borderSubtle
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 3
                                    Text {
                                        text: modelData.name
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontSm
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: "rho=" + Number(modelData.density).toFixed(1) + " kg/m³  k=" + Number(modelData.thermalConductivity).toFixed(1) + " W/mK"
                                        font.family: Theme.fontMono
                                        font.pixelSize: Theme.fontXs
                                        color: Theme.textMuted
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "sigma=" + Number(modelData.electricalConductivity).toExponential(1) + " S/m"
                                            font.family: Theme.fontMono
                                            font.pixelSize: Theme.fontXs
                                            color: Theme.textSecondary
                                            Layout.fillWidth: true
                                        }
                                        MonoButton {
                                            text: "+10% rho"
                                            compact: true
                                            onClicked: {
                                                pipeline.updateMaterialParameters(modelData.id, modelData.density * 1.1, -1, -1, -1);
                                                root.actionFeedback = "Perturbed " + modelData.name + " density to " + (modelData.density * 1.1).toFixed(1) + " kg/m³";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // TAB 2: Virtual Material Test Bench (7 Multiphysics Qualification Domains)
                    VirtualTestBenchPanel {
                        visible: parent.parent.rightTab === "test_bench"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        expName: root.expName
                        scenario: root.scenario
                        tempK: root.tempK
                        pressurePa: root.pressurePa
                        oxidizing: root.oxidizing
                        densityOverride: root.densityOverride
                        ingredients: root.ingredients
                    }

                    // TAB 3: First-Principles Atomic Bond Checker & Miscibility Engine
                    AtomicBondView {
                        visible: parent.parent.rightTab === "atomic_bonds"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        scenario: root.scenario
                        ingredients: root.ingredients
                    }

                    // TAB 4: Material Metric Comparison & Radar Spider Plot
                    MaterialComparisonView {
                        visible: parent.parent.rightTab === "compare"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        materialName: root.expName
                        scenario: root.scenario
                        density: root.densityOverride > 0 ? root.densityOverride : 5000.0
                        thermalK: 30.0
                        electricalSigma: 5.0e6
                        meltingK: 1200.0
                        ingredients: root.ingredients
                    }

                    // TAB 5: Structured Material Explanation & Provenance Report
                    MaterialExplanationView {
                        visible: parent.parent.rightTab === "explain"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        materialName: root.expName
                        scenario: root.scenario
                        density: root.densityOverride > 0 ? root.densityOverride : 5000.0
                        thermalK: 30.0
                        electricalSigma: 5.0e6
                        meltingK: 1200.0
                        ingredients: root.ingredients
                    }
                }
            }
        }
    }
}
