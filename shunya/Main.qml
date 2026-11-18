import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtCore

ApplicationWindow {
    id: window
    width: 1360
    height: 840
    minimumWidth: 960
    minimumHeight: 680
    visible: true
    color: Theme.bgPrimary
    title: (typeof pipeline !== "undefined" ? pipeline.projectName : "Shunya Studio") +
           (typeof pipeline !== "undefined" && pipeline.isDirty ? " * " : " ") +
           "- Shunya Engineering Studio"

    property double simTime: typeof studio !== "undefined" ? studio.simTime : 0.0
    property string simStatus: typeof studio !== "undefined" ? studio.simStatus : "Stopped"
    property string projectName: typeof pipeline !== "undefined" ? pipeline.projectName : "LiFePO4_Battery_Demo"
    property bool showConsoleDrawer: false

    // Layout Sizing & Session Persistence Properties
    property real leftRailWidth: 184
    property bool leftRailCollapsed: false
    property real consoleDrawerHeight: 150
    property bool rightPanelCollapsed: false
    property real rightPanelWidth: 360

    Settings {
        id: layoutSettings
        category: "WorkspaceLayout"
        property alias leftRailWidth: window.leftRailWidth
        property alias leftRailCollapsed: window.leftRailCollapsed
        property alias consoleDrawerHeight: window.consoleDrawerHeight
        property alias showConsoleDrawer: window.showConsoleDrawer
        property alias rightPanelCollapsed: window.rightPanelCollapsed
        property alias rightPanelWidth: window.rightPanelWidth
    }

    // Compatibility shims for dialogs/menus
    property bool lightMode: !Theme.isDarkMode
    property color bgDark: Theme.bgPrimary
    property color panelDark: Theme.bgSurface
    property color borderDark: Theme.borderDefault
    property color textDark: Theme.textPrimary
    property color textMuted: Theme.textMuted
    property color accentColor: Theme.textPrimary

    readonly property var navItems: [
        { id: "elements",   label: "Elements",      icon: "elements" },
        { id: "mixing_lab", label: "Mixing Lab",    icon: "mixing" },
        { id: "objects",    label: "Objects",       icon: "objects" },
        { id: "components", label: "Components",    icon: "components" },
        { id: "circuits",   label: "Circuits",      icon: "circuits" },
        { id: "editor_2d",  label: "2D Editor",     icon: "editor2d" },
        { id: "editor_3d",  label: "3D Editor",     icon: "editor3d" },
        { id: "simulation", label: "Simulation",    icon: "simulation" },
        { id: "settings",   label: "Settings",      icon: "settings" }
    ]

    function currentWorkspaceIndex() {
        var ws = (typeof pipeline !== "undefined") ? pipeline.activeWorkspace : "elements";
        for (var i = 0; i < navItems.length; ++i) {
            if (navItems[i].id === ws) return i;
        }
        return 0;
    }

    Shortcut { sequence: "F12"; onActivated: window.showConsoleDrawer = !window.showConsoleDrawer }
    Shortcut { sequence: "Ctrl+`"; onActivated: window.showConsoleDrawer = !window.showConsoleDrawer }
    Shortcut { sequence: "Ctrl+B"; onActivated: window.leftRailCollapsed = !window.leftRailCollapsed }
    Shortcut { sequence: "Ctrl+Alt+B"; onActivated: window.rightPanelCollapsed = !window.rightPanelCollapsed }
    Shortcut { sequence: "F11"; onActivated: { if (window.visibility === Window.FullScreen) window.showNormal(); else window.showFullScreen(); } }
    Shortcut { sequence: "Ctrl+K"; onActivated: commandPaletteDialog.open() }
    Shortcut { sequence: "Ctrl+P"; onActivated: commandPaletteDialog.open() }
    Shortcut { sequence: "Ctrl+Z"; onActivated: if (typeof pipeline !== "undefined") pipeline.circuitUndo() }
    Shortcut { sequence: "Ctrl+Y"; onActivated: if (typeof pipeline !== "undefined") pipeline.circuitRedo() }
    Shortcut { sequence: "Alt+1"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("elements") }
    Shortcut { sequence: "Alt+2"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("mixing_lab") }
    Shortcut { sequence: "Alt+3"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("objects") }
    Shortcut { sequence: "Alt+4"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("components") }
    Shortcut { sequence: "Alt+5"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("circuits") }
    Shortcut { sequence: "Alt+6"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("editor_2d") }
    Shortcut { sequence: "Alt+7"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("editor_3d") }
    Shortcut { sequence: "Alt+8"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("simulation") }
    Shortcut { sequence: "Alt+9"; onActivated: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("settings") }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Monochrome Menu Bar
        MainMenuBar {
            id: appMenuBar
            Layout.fillWidth: true
            onNewProjectRequested: newProjectWizard.open()
            onOpenProjectRequested: openFileDialog.open()
            onSaveProjectRequested: if (typeof menuController !== "undefined") menuController.saveProject()
            onSaveProjectAsRequested: saveFileDialog.open()
            onCloseProjectRequested: if (typeof menuController !== "undefined") menuController.closeProject(false)
            onExitRequested: if (typeof menuController !== "undefined") menuController.requestExit(); else Qt.quit()
        }

        // 2. Single-Project Top Workflow Header Bar (No File-Manager Sidebar!)
        Rectangle {
            Layout.fillWidth: true
            height: 44
            color: Theme.bgSurface
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12

                Row {
                    spacing: 8
                    Layout.alignment: Qt.AlignVCenter
                    ShunyaIcon { name: "sunken-eye"; size: 18; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: window.projectName
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    MonoBadge {
                        visible: typeof pipeline !== "undefined" && pipeline.isDirty
                        text: "Unsaved"
                        tone: "warning"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Item { Layout.fillWidth: true }

                // Pipeline Status & Quick Telemetry Counters
                Text {
                    text: typeof pipeline !== "undefined"
                        ? (pipeline.elementCount + " Elements  /  " +
                           pipeline.materialCount + " Materials  /  " +
                           pipeline.objectCount + " Objects  /  " +
                           pipeline.componentCount + " Components")
                        : ""
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }

                Rectangle { width: 1; height: 20; color: Theme.borderDefault }

                MonoBadge {
                    text: window.simStatus + "  t=" + window.simTime.toFixed(2) + "s"
                    tone: window.simStatus === "Running" ? "success" : "neutral"
                }
            }
        }

        // 3. Main Body: Slim Workflow Navigation Rail (Left) + Active Workspace Stack
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Workflow Rail (Single-Project Pipeline Only — No File Explorer!)
            Rectangle {
                id: leftRail
                Layout.preferredWidth: window.leftRailCollapsed ? 56 : window.leftRailWidth
                Layout.fillHeight: true
                clip: true
                color: Theme.bgSurface
                border.color: Theme.borderSubtle
                border.width: 1

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: window.leftRailCollapsed ? 4 : 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: window.leftRailCollapsed ? 0 : 6
                        Layout.bottomMargin: 2

                        Text {
                            visible: !window.leftRailCollapsed
                            text: "WORKFLOW"
                            font.family: Theme.fontSans
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: Theme.textSubtle
                            Layout.fillWidth: true
                        }

                        MonoButton {
                            text: window.leftRailCollapsed ? ">" : "<"
                            compact: true
                            variant: "ghost"
                            Layout.preferredWidth: 24
                            Layout.alignment: window.leftRailCollapsed ? Qt.AlignHCenter : Qt.AlignRight
                            onClicked: window.leftRailCollapsed = !window.leftRailCollapsed
                        }
                    }

                    Repeater {
                        model: window.navItems
                        delegate: Rectangle {
                            id: navItemRect
                            Layout.fillWidth: true
                            height: 36
                            radius: Theme.radiusSm
                            readonly property bool isCurrent: (typeof pipeline !== "undefined") && pipeline.activeWorkspace === modelData.id
                            color: isCurrent ? Theme.bgActive : (navMa.containsMouse ? Theme.bgHover : "transparent")
                            border.width: isCurrent ? 1 : 0
                            border.color: isCurrent ? Theme.borderDefault : "transparent"

                            // Active indicator vertical accent strip
                            Rectangle {
                                width: 3
                                height: 18
                                radius: 1.5
                                color: Theme.textPrimary
                                visible: isCurrent
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: window.leftRailCollapsed ? 0 : 10
                                anchors.rightMargin: window.leftRailCollapsed ? 0 : 8
                                spacing: window.leftRailCollapsed ? 0 : 10

                                ShunyaIcon {
                                    name: modelData.icon
                                    size: 16
                                    color: isCurrent ? Theme.textPrimary : Theme.textSecondary
                                    Layout.alignment: window.leftRailCollapsed ? Qt.AlignHCenter : Qt.AlignVCenter
                                    Layout.preferredWidth: window.leftRailCollapsed ? navItemRect.width : 16
                                }
                                Text {
                                    visible: !window.leftRailCollapsed
                                    text: modelData.label
                                    Layout.fillWidth: true
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontSm
                                    font.weight: isCurrent ? Font.DemiBold : Font.Medium
                                    color: isCurrent ? Theme.textPrimary : Theme.textSecondary
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    visible: !window.leftRailCollapsed && (isCurrent || navMa.containsMouse)
                                    text: "Alt+" + (index + 1)
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    color: Theme.textSubtle
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ToolTip.visible: window.leftRailCollapsed && navMa.containsMouse
                            ToolTip.text: modelData.label + " (Alt+" + (index + 1) + ")"
                            ToolTip.delay: 200

                            MouseArea {
                                id: navMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof pipeline !== "undefined") {
                                        pipeline.setActiveWorkspace(modelData.id);
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    MonoButton {
                        Layout.fillWidth: true
                        text: window.leftRailCollapsed ? "" : "Search (Ctrl+K)"
                        iconName: "search"
                        compact: true
                        onClicked: commandPaletteDialog.open()
                    }
                }
            }

            // Draggable Vertical Splitter for Left Rail
            StudioSplitter {
                id: leftRailSplitter
                orientation: Qt.Vertical
                defaultSize: 184
                minSize: 56
                maxSize: 360
                visible: !window.leftRailCollapsed
                onDragDelta: (delta) => {
                    var nw = Math.max(minSize, Math.min(maxSize, window.leftRailWidth + delta));
                    window.leftRailWidth = nw;
                }
                onResetRequested: {
                    window.leftRailWidth = 184;
                }
            }

            // Central Workspace Container + Optional Collapsible Bottom Log Drawer
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    clip: true

                    StackLayout {
                        anchors.fill: parent
                        currentIndex: window.currentWorkspaceIndex()

                        ElementsWorkspace {}
                        MixingLabWorkspace {}
                        ObjectsWorkspace {}
                        ComponentsBatteryWorkspace {}
                        CircuitWorkspace {}
                        Editor2DWorkspace {}
                        Editor3DWorkspace {}
                        SimulationWorkspace {}
                        ProjectSettingsWorkspace {}
                    }
                }

                // Draggable Horizontal Splitter for Bottom Console Drawer
                StudioSplitter {
                    id: consoleSplitter
                    orientation: Qt.Horizontal
                    defaultSize: 150
                    minSize: 80
                    maxSize: Math.max(120, window.height * 0.7)
                    visible: window.showConsoleDrawer
                    onDragDelta: (delta) => {
                        var nh = Math.max(minSize, Math.min(maxSize, window.consoleDrawerHeight - delta));
                        window.consoleDrawerHeight = nh;
                    }
                    onResetRequested: {
                        window.consoleDrawerHeight = 150;
                    }
                }

                // Bottom Status & Diagnostics Drawer (Toggled by F12, Ctrl+`, or Console button)
                Rectangle {
                    visible: window.showConsoleDrawer
                    Layout.fillWidth: true
                    Layout.preferredHeight: window.consoleDrawerHeight
                    clip: true
                    color: Theme.bgSurface
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: "Diagnostics & Solver Telemetry Log"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }
                            MonoCheckBox {
                                id: autoScrollCheck
                                text: "Auto-scroll"
                                checked: true
                            }
                            MonoButton {
                                text: "Copy"
                                compact: true
                                onClicked: {
                                    logTextArea.selectAll();
                                    logTextArea.copy();
                                    logTextArea.deselect();
                                }
                            }
                            MonoButton {
                                text: "Clear"
                                compact: true
                                onClicked: {
                                    if (typeof studio !== "undefined") {
                                        studio.clearLog();
                                    }
                                }
                            }
                            MonoButton {
                                iconName: "close"
                                compact: true
                                variant: "ghost"
                                onClicked: window.showConsoleDrawer = false
                            }
                        }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            TextArea {
                                id: logTextArea
                                readOnly: true
                                text: typeof studio !== "undefined" ? studio.logOutput : ""
                                color: Theme.textSecondary
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontXs
                                selectByMouse: true
                                wrapMode: Text.WrapAnywhere
                                background: Rectangle { color: Theme.bgPrimary; radius: Theme.radiusSm }
                                onTextChanged: {
                                    if (autoScrollCheck.checked) {
                                        cursorPosition = length;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 4. Bottom Status Strip (VS Code-Style Desktop Engineering Status Bar)
        Rectangle {
            id: bottomStatusBar
            Layout.fillWidth: true
            height: 26
            color: Theme.bgElevated
            border.color: Theme.borderSubtle
            border.width: 1

            property bool gridSnap: true
            property double snapGridMm: 1.0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                // 1. Pipeline Status
                Row {
                    spacing: 6
                    Rectangle {
                        width: 7
                        height: 7
                        radius: 3.5
                        color: typeof pipeline !== "undefined" && pipeline.isDirty ? Theme.statusWarning : Theme.statusSuccess
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: typeof pipeline !== "undefined" ? pipeline.statusMessage : "Ready"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle { width: 1; height: 14; color: Theme.borderDefault }

                // 2. Active Tool
                Text {
                    text: "Tool: " + (typeof ribbonController !== "undefined" ? ribbonController.activeCategory : "Select")
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }

                Rectangle { width: 1; height: 14; color: Theme.borderDefault }

                // 3. Cursor World Coordinates
                Text {
                    text: "X: 0.00 mm  Y: 0.00 mm  Z: 0.00 mm"
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }

                Rectangle { width: 1; height: 14; color: Theme.borderDefault }

                // 4. Grid Snap Status (Interactive Clickable)
                Rectangle {
                    height: 18
                    width: snapTxt.contentWidth + 12
                    radius: Theme.radiusSm
                    color: snapMa.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        id: snapTxt
                        anchors.centerIn: parent
                        text: bottomStatusBar.gridSnap ? "SNAP: ON (1.0 mm)" : "SNAP: OFF"
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontXs
                        color: bottomStatusBar.gridSnap ? Theme.textPrimary : Theme.textSubtle
                    }

                    MouseArea {
                        id: snapMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bottomStatusBar.gridSnap = !bottomStatusBar.gridSnap
                    }
                }

                Rectangle { width: 1; height: 14; color: Theme.borderDefault }

                // 5. Active Unit System
                Text {
                    text: "Units: SI / Metric (mm)"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }

                Item { Layout.fillWidth: true }

                // 6. Simulation Clock
                Row {
                    spacing: 6
                    Rectangle {
                        width: 7
                        height: 7
                        radius: 3.5
                        color: window.simStatus === "Running" ? Theme.statusSuccess : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "Clock: " + window.simTime.toFixed(2) + " s [" + window.simStatus + "]"
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontXs
                        color: Theme.textPrimary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    // Dialogs & File Pickers
    NewProjectWizard { id: newProjectWizard }
    UnsavedChangesGuardDialog {
        id: unsavedGuard
        onProceedPending: if (typeof menuController !== "undefined") menuController.forceProceedPendingAction()
        onSaveAndProceed: if (typeof menuController !== "undefined") menuController.saveThenProceedPendingAction()
    }
    CommandPalette { id: commandPaletteDialog }

    FileDialog {
        id: openFileDialog
        title: "Open Shunya Engineering Project"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Shunya Projects (*.shunya3.json *.shunya)", "All Files (*)"]
        onAccepted: if (typeof menuController !== "undefined") menuController.openProject(selectedFile.toString())
    }

    FileDialog {
        id: saveFileDialog
        title: "Save Shunya Engineering Project As"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Shunya v3 Bundle (*.shunya3.json)", "Legacy Manifest (*.shunya)"]
        onAccepted: if (typeof menuController !== "undefined") menuController.saveProjectAs(selectedFile.toString())
    }

    Connections {
        target: typeof menuController !== "undefined" ? menuController : null
        function onConfirmUnsavedChanges(action) {
            if (action === "save_as") {
                saveFileDialog.open();
            } else {
                unsavedGuard.pendingAction = action;
                unsavedGuard.open();
            }
        }
    }
}
