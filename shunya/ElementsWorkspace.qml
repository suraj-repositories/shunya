import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Rectangle {
    id: root
    color: Theme.bgCanvas

    property int selectedZ: 1 // Default Hydrogen (H, Z=1) per PH5-01.02 / 1.2
    property double envTempK: 298.15
    property double envPressurePa: 101325.0
    property bool envOxidizing: true
    property string searchQuery: ""
    property string phaseFilter: ""
    property string sortBy: "AtomicNumber"
    property bool viewTableMode: false

    property var elementList: []
    property var elementDetails: ({})
    property var behaviorReport: ({})

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        elementList = pipeline.queryElements(searchQuery, phaseFilter, "", "", sortBy, false);
        elementDetails = pipeline.getElementDetails(selectedZ);
        behaviorReport = pipeline.evaluateElementBehavior(selectedZ, envTempK, envPressurePa, envOxidizing);
        if (typeof periodicTableController !== "undefined") {
            periodicTableController.setSelectedAtomicNumber(selectedZ);
            periodicTableController.setSearchText(searchQuery);
            periodicTableController.setPhaseFilter(phaseFilter);
            periodicTableController.setEnvTempK(envTempK);
            periodicTableController.setEnvPressurePa(envPressurePa);
        }
    }

    Component.onCompleted: refreshAll()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // Header & Search/Filter Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                spacing: 2
                Text {
                    text: "Chemical Elements & Natural Behavior"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: (typeof pipeline !== "undefined" ? pipeline.elementCount : 118) + " elements indexed from elements.csv (82 properties with provenance)"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    color: Theme.textMuted
                }
            }

            Item { Layout.fillWidth: true }

            MonoInput {
                id: searchBox
                Layout.preferredWidth: 220
                placeholderText: "Search element, symbol, Z..."
                clearable: true
                onTextEdited: (t) => { root.searchQuery = t; root.refreshAll(); }
            }

            Row {
                spacing: 4
                Repeater {
                    model: ["All", "Solid", "Liquid", "Gas"]
                    delegate: MonoButton {
                        text: modelData
                        compact: true
                        active: (modelData === "All" ? root.phaseFilter === "" : root.phaseFilter === modelData)
                        onClicked: {
                            root.phaseFilter = (modelData === "All") ? "" : modelData;
                            root.refreshAll();
                        }
                    }
                }
            }

            MonoButton {
                text: root.viewTableMode ? "Grid View" : "Table View"
                iconName: "elements"
                compact: true
                active: root.viewTableMode
                onClicked: root.viewTableMode = !root.viewTableMode
            }
        }

        // Main Split: Left = Periodic Table / Sortable Table, Right = 82-Property Inspector & Live Behavior
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            // LEFT PANE
            Rectangle {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 460
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd

                // Interactive Periodic Table View
                Item {
                    anchors.fill: parent
                    anchors.margins: 10
                    visible: !root.viewTableMode

                    PeriodicTableWindow {
                        anchors.fill: parent
                        onElementActivated: (z) => {
                            root.selectedZ = z;
                            root.refreshAll();
                        }
                    }
                }

                // Sortable Full Table View
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6
                    visible: root.viewTableMode

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text { text: "Sort by:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Repeater {
                            model: ["AtomicNumber", "Name", "Density", "MeltingPoint", "ThermalConductivity", "ElectricalConductivity"]
                            delegate: MonoButton {
                                text: modelData
                                compact: true
                                active: root.sortBy === modelData
                                onClicked: { root.sortBy = modelData; root.refreshAll(); }
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.elementList
                        spacing: 2
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 32
                            radius: Theme.radiusSm
                            color: modelData.atomicNumber === root.selectedZ ? Theme.bgActive : (rowMa.containsMouse ? Theme.bgHover : Theme.bgCanvas)
                            border.width: modelData.atomicNumber === root.selectedZ ? 1 : 0
                            border.color: Theme.borderStrong

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                Text { text: modelData.atomicNumber; Layout.preferredWidth: 32; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                                Text { text: modelData.iupacSymbol; Layout.preferredWidth: 36; font.family: Theme.fontMono; font.pixelSize: Theme.fontMd; font.weight: Font.Bold; color: Theme.textPrimary }
                                Text { text: modelData.name; Layout.fillWidth: true; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textPrimary; elide: Text.ElideRight }
                                Text { text: modelData.phase; Layout.preferredWidth: 64; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                Text { text: modelData.density !== undefined ? Number(modelData.density).toFixed(1) + " kg/m3" : "--"; Layout.preferredWidth: 96; horizontalAlignment: Text.AlignRight; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textSecondary }
                                Text { text: modelData.meltingPoint !== undefined ? Number(modelData.meltingPoint).toFixed(1) + " K" : "--"; Layout.preferredWidth: 74; horizontalAlignment: Text.AlignRight; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textSecondary }
                            }

                            MouseArea {
                                id: rowMa
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: { root.selectedZ = modelData.atomicNumber; root.refreshAll(); }
                            }
                        }
                    }
                }
            }

            // RIGHT INSPECTOR: Live Behavior Engine + 82 CSV Properties
            Rectangle {
                SplitView.preferredWidth: 360
                SplitView.minimumWidth: 300
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    // Element Header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            width: 52; height: 52; radius: Theme.radiusMd
                            color: Theme.bgElevated; border.color: Theme.borderStrong; border.width: 1
                            Column {
                                anchors.centerIn: parent
                                Text { text: root.elementDetails.atomicNumber || ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textMuted; anchors.horizontalCenter: parent.horizontalCenter }
                                Text { text: root.elementDetails.iupacSymbol || root.elementDetails.symbol || ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXl; font.weight: Font.Bold; color: Theme.textPrimary; anchors.horizontalCenter: parent.horizontalCenter }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                spacing: 8
                                Text {
                                    text: root.elementDetails.name || ""
                                    font.family: Theme.fontSans; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold; color: Theme.textPrimary
                                }
                                Text {
                                    text: root.elementDetails.atomicMass ? Number(root.elementDetails.atomicMass).toFixed(3) + " u" : ""
                                    font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textSecondary
                                }
                            }
                            RowLayout {
                                spacing: 6
                                MonoBadge { text: "Phase: " + (root.behaviorReport.phase || root.elementDetails.phase || "--"); tone: "neutral" }
                                MonoBadge { text: "Block " + (root.elementDetails.block || "-"); tone: "neutral" }
                                MonoBadge { text: root.elementDetails.category || "Element"; tone: "neutral" }
                                MonoBadge {
                                    visible: root.behaviorReport.combustionRisk || root.behaviorReport.isSuperconducting
                                    text: root.behaviorReport.combustionRisk ? "Autoignition Risk" : (root.behaviorReport.isSuperconducting ? "Superconducting" : "Nominal")
                                    tone: root.behaviorReport.combustionRisk ? "error" : (root.behaviorReport.isSuperconducting ? "success" : "neutral")
                                }
                            }
                        }
                    }

                    // Key Physical Properties Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: keyPropsGrid.implicitHeight + 12
                        color: Theme.bgCanvas
                        border.color: Theme.borderSubtle
                        border.width: 1
                        radius: Theme.radiusSm

                        GridLayout {
                            id: keyPropsGrid
                            anchors.fill: parent
                            anchors.margins: 8
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 3

                            Text { text: "Density (STP):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            Text { text: root.elementDetails.density ? Number(root.elementDetails.density).toFixed(2) + " kg/m³" : "Not Measured"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                            Text { text: "Melting Point:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            Text { text: root.elementDetails.meltingPoint ? Number(root.elementDetails.meltingPoint).toFixed(1) + " K" : "Not Measured"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                            Text { text: "Boiling Point:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            Text { text: root.elementDetails.boilingPoint ? Number(root.elementDetails.boilingPoint).toFixed(1) + " K" : "Not Measured"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                            Text { text: "Electronegativity:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            Text { text: root.elementDetails.electronegativity ? Number(root.elementDetails.electronegativity).toFixed(2) : "Not Measured"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                            Text { text: "Electron Config:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            Text { text: root.elementDetails.electronConfig || "--"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                    }

                    // Natural Behavior Condition Sliders (T, P, Oxidizing)
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: behCol.implicitHeight + 16
                        color: Theme.bgCanvas
                        border.color: Theme.borderSubtle
                        border.width: 1
                        radius: Theme.radiusSm

                        ColumnLayout {
                            id: behCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 6

                            Text {
                                text: "Natural Element Behavior at T & P"
                                font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Temp (K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted; Layout.preferredWidth: 70 }
                                Slider {
                                    id: tSlider
                                    Layout.fillWidth: true
                                    from: 4; to: 3000; stepSize: 5
                                    value: root.envTempK
                                    onMoved: { root.envTempK = value; root.refreshAll(); }
                                }
                                Text { text: root.envTempK.toFixed(1) + " K"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; Layout.preferredWidth: 68; horizontalAlignment: Text.AlignRight }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Press (atm):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted; Layout.preferredWidth: 70 }
                                Slider {
                                    Layout.fillWidth: true
                                    from: 0.01; to: 500.0; stepSize: 1.0
                                    value: root.envPressurePa / 101325.0
                                    onMoved: { root.envPressurePa = value * 101325.0; root.refreshAll(); }
                                }
                                MonoButton {
                                    text: root.envOxidizing ? "Air/O2" : "Inert"
                                    compact: true
                                    onClicked: {
                                        root.envOxidizing = !root.envOxidizing;
                                        root.refreshAll();
                                    }
                                }
                            }

                            // Evaluated T/P metrics
                            GridLayout {
                                columns: 2
                                columnSpacing: 12
                                rowSpacing: 4
                                Layout.fillWidth: true
                                Text { text: "Density rho(T,P):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                Text { text: (root.behaviorReport.density && root.behaviorReport.density.available) ? Number(root.behaviorReport.density.value).toPrecision(4) + " kg/m3" : "Unsupported"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                                Text { text: "Electrical sigma(T):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                Text { text: (root.behaviorReport.electricalConductivity && root.behaviorReport.electricalConductivity.available) ? Number(root.behaviorReport.electricalConductivity.value).toExponential(2) + " S/m" : "Unsupported"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                                Text { text: "Reactivity & Magnetic:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                Text { text: (root.behaviorReport.reactivityClass || "-") + " / " + (root.behaviorReport.magneticState || "-"); font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
                            }

                            Repeater {
                                model: root.behaviorReport.warnings || []
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: warnTxt.implicitHeight + 10
                                    radius: Theme.radiusSm
                                    color: Theme.statusWarningBg
                                    border.color: Theme.statusWarning
                                    border.width: 1
                                    Text {
                                        id: warnTxt
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        text: modelData
                                        wrapMode: Text.Wrap
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        color: Theme.statusWarning
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        text: "Complete 82 CSV Properties (elements.csv + Provenance)"
                        font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 3
                        model: root.elementDetails.allProperties || []
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 26
                            color: index % 2 === 0 ? Theme.bgCanvas : Theme.bgSurface
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6; anchors.rightMargin: 6
                                spacing: 6
                                Text { text: modelData.column; Layout.preferredWidth: 145; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted; elide: Text.ElideRight }
                                Text { text: modelData.rawValue; Layout.fillWidth: true; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; elide: Text.ElideRight }
                                Text { text: modelData.provenance; Layout.preferredWidth: 90; horizontalAlignment: Text.AlignRight; font.family: Theme.fontSans; font.pixelSize: 10; color: modelData.provenance === "Missing / Unsupported" ? Theme.textSubtle : Theme.textMuted; elide: Text.ElideRight }
                            }
                        }
                    }
                }
            }
        }
    }
}
