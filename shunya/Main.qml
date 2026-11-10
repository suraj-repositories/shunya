import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 650
    visible: true
    title: qsTr("Shunya Virtual Engineering & Multiphysics Studio")

    property bool lightMode: Application.styleHints.colorScheme === Qt.Light
    property color bgDark: "#181818"
    property color panelDark: "#222222"
    property color borderDark: "#333333"
    property color textDark: "#e0e0e0"
    property color textMuted: "#8b949e"
    property color accentColor: "#00b4d8"
    property color accentGreen: "#06d6a0"
    property color accentYellow: "#ffd166"
    property color accentRed: "#ef476f"

    property string currentPreset: typeof dockingController !== "undefined" ? dockingController.activePreset : "Modeling"
    property double simTime: typeof studio !== "undefined" ? studio.simTime : 0.0
    property string simStatus: typeof studio !== "undefined" ? studio.simStatus : "Stopped"
    property string projectName: typeof studio !== "undefined" ? studio.projectName : "ElectroThermoMechanical_Rig"
    property bool showConsoleDrawer: true

    Shortcut {
        sequence: "F9"
        onActivated: {
            if (typeof dockingController !== "undefined") dockingController.triggerShortcut("F9");
        }
    }
    Shortcut {
        sequence: "F10"
        onActivated: {
            if (typeof dockingController !== "undefined") dockingController.triggerShortcut("F10");
        }
    }
    Shortcut {
        sequence: "F12"
        onActivated: {
            if (typeof dockingController !== "undefined") dockingController.triggerShortcut("F12");
        }
    }
    Shortcut {
        sequence: "Ctrl+B"
        onActivated: {
            if (typeof dockingController !== "undefined") dockingController.triggerShortcut("Ctrl+B");
        }
    }
    Shortcut {
        sequence: "Ctrl+J"
        onActivated: {
            if (typeof dockingController !== "undefined") dockingController.triggerShortcut("Ctrl+J");
        }
    }
    Shortcut {
        sequence: "Ctrl+K"
        onActivated: commandPaletteDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+P"
        onActivated: commandPaletteDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // Desktop Application Menu Bar (UI-01-003)
        // =====================================================================
        MainMenuBar {
            id: appMenuBar
            Layout.fillWidth: true
            onNewProjectRequested: newProjectWizard.open()
            onOpenProjectRequested: openFileDialog.open()
            onSaveProjectRequested: {
                if (typeof menuController !== "undefined") menuController.saveProject();
            }
            onSaveProjectAsRequested: saveFileDialog.open()
            onCloseProjectRequested: {
                if (typeof menuController !== "undefined") menuController.closeProject(false);
            }
            onExitRequested: {
                if (typeof menuController !== "undefined") menuController.requestExit();
                else Qt.quit();
            }
        }

        // =====================================================================
        // Contextual Ribbon Toolbar (UI-01-002)
        // =====================================================================
        RibbonBar {
            id: appRibbon
            Layout.fillWidth: true
            onCommandPaletteRequested: commandPaletteDialog.open()
            onHistoryDropdownRequested: undoHistoryPopup.open()
        }

        // =====================================================================
        // Top Toolbar
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: window.panelDark

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Label {
                    text: "SHUNYA"
                    font.bold: true
                    font.pixelSize: 16
                    color: window.accentColor
                }

                Rectangle { width: 1; height: 24; color: window.borderDark }

                // Preset selector
                RowLayout {
                    spacing: 4
                    Repeater {
                        model: ["Modeling", "Schematic", "Simulation", "Results", "Scripting"]
                        Button {
                            id: presetBtn
                            text: modelData
                            checked: window.currentPreset === modelData
                            onClicked: {
                                window.currentPreset = modelData;
                                if (typeof dockingController !== "undefined") {
                                    dockingController.applyPreset(modelData);
                                }
                            }
                            contentItem: Text {
                                text: presetBtn.text
                                color: presetBtn.checked ? window.accentColor : window.textDark
                                font.bold: presetBtn.checked
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitWidth: 80
                                implicitHeight: 28
                                radius: 4
                                color: presetBtn.checked ? "#2c3e50" : "transparent"
                                border.color: presetBtn.checked ? window.accentColor : "transparent"
                            }
                        }
                    }
                }

                Rectangle { width: 1; height: 24; color: window.borderDark }

                // Simulation Controls
                Button {
                    text: "\u25B6 Run"
                    contentItem: Text { text: parent.text; color: window.accentGreen; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 64; implicitHeight: 28; radius: 4; color: "#1b3a2b"; border.color: window.accentGreen }
                    onClicked: {
                        if (typeof studio !== "undefined") studio.startSimulation();
                    }
                }

                Button {
                    text: "\u23F8 Pause"
                    contentItem: Text { text: parent.text; color: window.accentYellow; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 64; implicitHeight: 28; radius: 4; color: "#3a341b"; border.color: window.accentYellow }
                    onClicked: {
                        if (typeof studio !== "undefined") studio.pauseSimulation();
                    }
                }

                Button {
                    text: "\u23F9 Stop"
                    contentItem: Text { text: parent.text; color: window.accentRed; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 64; implicitHeight: 28; radius: 4; color: "#3a1b22"; border.color: window.accentRed }
                    onClicked: {
                        if (typeof studio !== "undefined") studio.stopSimulation();
                    }
                }

                Button {
                    text: "\u23ED Step"
                    contentItem: Text { text: parent.text; color: window.accentColor; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 64; implicitHeight: 28; radius: 4; color: "#152e3d"; border.color: window.accentColor }
                    onClicked: {
                        if (typeof studio !== "undefined") studio.stepSimulation();
                    }
                }

                Button {
                    text: "\u21BA Reset"
                    contentItem: Text { text: parent.text; color: window.textDark; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 64; implicitHeight: 28; radius: 4; color: "#2d333b"; border.color: window.borderDark }
                    onClicked: {
                        if (typeof studio !== "undefined") studio.resetSimulation();
                    }
                }

                Item { Layout.fillWidth: true }

                // Telemetry badge
                Rectangle {
                    implicitWidth: 200
                    implicitHeight: 28
                    radius: 4
                    color: "#161b22"
                    border.color: window.borderDark
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Label { text: "Time: " + window.simTime.toFixed(3) + " s"; color: window.textMuted; font.pixelSize: 11; font.family: "Monospace" }
                        Label {
                            text: "[" + window.simStatus + "]"
                            color: window.simStatus === "Running" ? window.accentGreen :
                                   window.simStatus === "Paused" ? window.accentYellow : window.accentRed
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Main Workspace Area (3 Columns)
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 1

            // Left Panel: Project Tree & Components
            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: window.panelDark

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "PROJECT EXPLORER"; font.bold: true; font.pixelSize: 11; color: window.textMuted }
                        Item { Layout.fillWidth: true }
                        Label { text: window.projectName; color: window.accentColor; font.pixelSize: 10; elide: Text.ElideRight }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: window.borderDark }

                    ListView {
                        id: entityList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: (typeof studio !== "undefined" && studio.activeEntities.length > 0) ?
                                studio.activeEntities : [
                                    "Root Assembly",
                                    "Electronics_Subsystem (24V DC Source, R8)",
                                    "Thermal_Dissipation_Stage (Aluminum Core, 18W)",
                                    "Mechanical_Actuator_Stage (RigidBody 1.5kg)",
                                    "ClosedLoop_Sensor_Controller (PID / Rule Sandbox)"
                                ]
                        delegate: Rectangle {
                            width: entityList.width
                            height: 32
                            color: "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                Label { text: "\u25C6"; color: window.accentColor; font.pixelSize: 10 }
                                Label {
                                    text: modelData
                                    color: window.textDark
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: window.borderDark }

                    // Quick Actions
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Button {
                            Layout.fillWidth: true
                            text: "\u2699 Run Scientific Benchmarks"
                            contentItem: Text { text: parent.text; color: window.accentColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitHeight: 28; radius: 4; color: "#162b3a"; border.color: window.accentColor }
                            onClicked: {
                                if (typeof studio !== "undefined") {
                                    studio.runBenchmark();
                                    window.currentPreset = "Results";
                                }
                            }
                        }
                        Button {
                            Layout.fillWidth: true
                            text: "\u29D6 Run Parameter Sweep"
                            contentItem: Text { text: parent.text; color: window.accentYellow; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitHeight: 28; radius: 4; color: "#2d2a17"; border.color: window.accentYellow }
                            onClicked: {
                                if (typeof studio !== "undefined") {
                                    studio.runSweep("LoadResistance", 50.0, 250.0, 5);
                                    window.currentPreset = "Simulation";
                                }
                            }
                        }
                    }
                }
            }

            // Center Panel: Dynamic Viewport Canvas
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.bgDark

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    // Preset specific content
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#121212"
                        radius: 6
                        border.color: window.borderDark

                        // Viewport View
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            // Header banner
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: window.currentPreset.toUpperCase() + " WORKSPACE"
                                    font.bold: true
                                    font.pixelSize: 14
                                    color: window.accentColor
                                }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: "Fidelity Tier: L3 (Validated Analytical/Numerical)"
                                    color: window.textMuted
                                    font.pixelSize: 11
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: window.borderDark }

                            // Workspace dynamic canvas
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                // SIMULATION & RESULTS VIEW
                                ScrollView {
                                    anchors.fill: parent
                                    visible: window.currentPreset === "Results" || window.currentPreset === "Simulation"
                                    clip: true

                                    ColumnLayout {
                                        width: parent.width
                                        spacing: 12

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 110
                                            color: "#1b222c"
                                            radius: 6
                                            border.color: window.accentColor

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 12
                                                Label { text: "Active Multiphysics Telemetry"; font.bold: true; color: window.accentColor }
                                                RowLayout {
                                                    spacing: 24
                                                    ColumnLayout {
                                                        Label { text: "Physical Time (t)"; color: window.textMuted; font.pixelSize: 11 }
                                                        Label { text: window.simTime.toFixed(4) + " s"; color: "#ffffff"; font.bold: true; font.pixelSize: 16; font.family: "Monospace" }
                                                    }
                                                    ColumnLayout {
                                                        Label { text: "Coupled Domains"; color: window.textMuted; font.pixelSize: 11 }
                                                        Label { text: "Circuit + Thermal + RigidBody"; color: window.accentGreen; font.bold: true; font.pixelSize: 13 }
                                                    }
                                                    ColumnLayout {
                                                        Label { text: "Solver Status"; color: window.textMuted; font.pixelSize: 11 }
                                                        Label { text: window.simStatus; color: window.accentYellow; font.bold: true; font.pixelSize: 14 }
                                                    }
                                                }
                                            }
                                        }

                                        // Real-Time GPU Scientific Plotter & Scalar Field Colormap Legend
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 280
                                            spacing: 10

                                            ScientificPlot {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                            }

                                            ColormapLegend {
                                                Layout.preferredWidth: 170
                                                Layout.fillHeight: true
                                                controller: typeof colormapController !== "undefined" ? colormapController : null
                                            }
                                        }

                                        // Mixed-Signal Oscilloscope & Logic Analyzer Viewport
                                        OscilloscopePanel {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 360
                                            controller: typeof oscilloscopeController !== "undefined" ? oscilloscopeController : null
                                        }

                                        Label {
                                            text: "Analytical Benchmark Verification Report:"
                                            font.bold: true
                                            color: window.textDark
                                            visible: typeof studio !== "undefined" && studio.benchmarkOutput.length > 0
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 180
                                            color: "#0d1117"
                                            radius: 4
                                            border.color: "#30363d"
                                            visible: typeof studio !== "undefined" && studio.benchmarkOutput.length > 0

                                            ScrollView {
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                TextArea {
                                                    readOnly: true
                                                    text: typeof studio !== "undefined" ? studio.benchmarkOutput : ""
                                                    color: window.accentGreen
                                                    font.family: "Monospace"
                                                    font.pixelSize: 12
                                                    background: null
                                                }
                                            }
                                        }
                                    }
                                }

                                // MODELING & 3D VIEWPORT WITH GIZMO, VIEWCUBE, AND HUD
                                Item {
                                    anchors.fill: parent
                                    visible: window.currentPreset === "Modeling" || window.currentPreset === "Schematic" || window.currentPreset === "Scripting"

                                    // Viewport background grid canvas
                                    Canvas {
                                        id: vpGridCanvas
                                        anchors.fill: parent
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.clearRect(0, 0, width, height);
                                            ctx.strokeStyle = "#1a1f26";
                                            ctx.lineWidth = 1;
                                            var step = 30;
                                            for (var x = 0; x < width; x += step) {
                                                ctx.beginPath();
                                                ctx.moveTo(x, 0);
                                                ctx.lineTo(x, height);
                                                ctx.stroke();
                                            }
                                            for (var y = 0; y < height; y += step) {
                                                ctx.beginPath();
                                                ctx.moveTo(0, y);
                                                ctx.lineTo(width, y);
                                                ctx.stroke();
                                            }
                                        }
                                    }

                                    // Interactive 3D Transform Gizmo Overlay
                                    GizmoOverlay {
                                        anchors.fill: parent
                                        controller: typeof viewportController !== "undefined" ? viewportController : null
                                    }

                                    // Heads-Up Display (HUD) in top-left
                                    ViewportHud {
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.margins: 10
                                        controller: typeof viewportController !== "undefined" ? viewportController : null
                                    }

                                    // ViewCube Navigation in top-right
                                    ViewCube {
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.margins: 10
                                        controller: typeof viewportController !== "undefined" ? viewportController : null
                                    }
                                }
                            }
                        }
                    }

                    // Collapsible Console / Diagnostics Drawer
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: window.showConsoleDrawer ? 160 : 32
                        color: "#0d1117"
                        radius: 4
                        border.color: window.borderDark

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                color: "#161b22"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    Label { text: "\u25A4 Diagnostics & Core Simulation Log"; font.bold: true; font.pixelSize: 11; color: window.textMuted }
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        text: window.showConsoleDrawer ? "\u25BC Hide" : "\u25B2 Show"
                                        contentItem: Text { text: parent.text; color: window.accentColor; font.pixelSize: 10 }
                                        background: Rectangle { implicitWidth: 50; implicitHeight: 20; color: "transparent" }
                                        onClicked: window.showConsoleDrawer = !window.showConsoleDrawer
                                    }
                                }
                            }

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                visible: window.showConsoleDrawer

                                TextArea {
                                    id: consoleArea
                                    readOnly: true
                                    text: typeof studio !== "undefined" ? studio.logOutput : "[INIT] Engine ready."
                                    color: "#58a6ff"
                                    font.family: "Monospace"
                                    font.pixelSize: 11
                                    background: null
                                    wrapMode: TextEdit.Wrap
                                    onTextChanged: {
                                        cursorPosition = text.length;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Right Panel: Scientific Property Inspector & Multi-Selection Inspector
            Rectangle {
                id: inspectorPanel
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                color: window.panelDark

                property bool showMultiInspector: typeof multiPropertyController !== "undefined" && multiPropertyController.hasMultiSelection

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Tab Selector
                    Rectangle {
                        Layout.fillWidth: true
                        height: 28
                        color: "#181a1f"
                        border.color: window.borderDark

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: !inspectorPanel.showMultiInspector ? "#21252b" : "#181a1f"

                                Text {
                                    anchors.centerIn: parent
                                    text: "Single Entity"
                                    color: !inspectorPanel.showMultiInspector ? "#ffffff" : window.textMuted
                                    font.pixelSize: 11
                                    font.bold: !inspectorPanel.showMultiInspector
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: inspectorPanel.showMultiInspector = false
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: inspectorPanel.showMultiInspector ? "#21252b" : "#181a1f"

                                Text {
                                    anchors.centerIn: parent
                                    text: "Multi-Selection (" + (typeof multiPropertyController !== "undefined" ? multiPropertyController.selectedEntityCount : 0) + ")"
                                    color: inspectorPanel.showMultiInspector ? "#61afef" : window.textMuted
                                    font.pixelSize: 11
                                    font.bold: inspectorPanel.showMultiInspector
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        inspectorPanel.showMultiInspector = true;
                                        if (typeof multiPropertyController !== "undefined" && multiPropertyController.selectedEntityCount === 0) {
                                            multiPropertyController.selectDemoMultiSet("50_resistors_identical");
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        PropertyInspector {
                            anchors.fill: parent
                            model: typeof propertyTreeModel !== "undefined" ? propertyTreeModel : null
                            visible: !inspectorPanel.showMultiInspector
                        }

                        MultiPropertyInspector {
                            anchors.fill: parent
                            controller: typeof multiPropertyController !== "undefined" ? multiPropertyController : null
                            visible: inspectorPanel.showMultiInspector
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Bottom Status Bar
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#161b22"
            border.color: window.borderDark

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 16

                Label { text: "\u25CF System Ready"; color: window.accentGreen; font.pixelSize: 11 }
                Label { text: "Solvers: RK45 Adaptive / Sparse CSR MNA"; color: window.textMuted; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Label { text: "C++20 Core Baseline | Zero Leakage Interface"; color: "#6e7681"; font.pixelSize: 11 }
            }
        }
    }

    NewProjectWizard {
        id: newProjectWizard
    }

    UnsavedChangesGuardDialog {
        id: unsavedChangesGuard
    }

    CommandPalette {
        id: commandPaletteDialog
    }

    UndoHistoryDropdown {
        id: undoHistoryPopup
        x: 40
        y: 65
    }

    FileDialog {
        id: openFileDialog
        title: qsTr("Open Shunya Project")
        nameFilters: ["Shunya Project Files (*.shunya *.json)", "All Files (*)"]
        onAccepted: {
            if (typeof menuController !== "undefined") {
                menuController.openProject(selectedFile.toString().replace("file:///", ""));
            }
        }
    }

    FileDialog {
        id: saveFileDialog
        title: qsTr("Save Shunya Project As")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Shunya Project Files (*.shunya *.json)", "All Files (*)"]
        onAccepted: {
            if (typeof menuController !== "undefined") {
                menuController.saveProjectAs(selectedFile.toString().replace("file:///", ""));
            }
        }
    }

    Connections {
        target: typeof menuController !== "undefined" ? menuController : null
        function onConfirmUnsavedChanges(action) {
            unsavedChangesGuard.pendingAction = action;
            unsavedChangesGuard.open();
        }
    }
}
