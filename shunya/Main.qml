import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

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
    Shortcut { sequence: "Ctrl+K"; onActivated: commandPaletteDialog.open() }
    Shortcut { sequence: "Ctrl+P"; onActivated: commandPaletteDialog.open() }
    Shortcut { sequence: "Ctrl+Z"; onActivated: if (typeof pipeline !== "undefined") pipeline.circuitUndo() }
    Shortcut { sequence: "Ctrl+Y"; onActivated: if (typeof pipeline !== "undefined") pipeline.circuitRedo() }

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
                    ShunyaIcon { name: "battery"; size: 18; anchors.verticalCenter: parent.verticalCenter }
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

                MonoButton {
                    text: "Console"
                    iconName: "info"
                    compact: true
                    active: window.showConsoleDrawer
                    onClicked: window.showConsoleDrawer = !window.showConsoleDrawer
                }

                MonoButton {
                    id: themeToggleBtn
                    text: Theme.isDarkMode ? "Light" : "Dark"
                    iconName: Theme.isDarkMode ? "sun" : "moon"
                    compact: true
                    onClicked: Theme.isDarkMode = !Theme.isDarkMode
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
                Layout.preferredWidth: 168
                Layout.fillHeight: true
                color: Theme.bgSurface
                border.color: Theme.borderSubtle
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "WORKFLOW"
                        font.family: Theme.fontSans
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: Theme.textSubtle
                        Layout.leftMargin: 6
                        Layout.bottomMargin: 2
                    }

                    Repeater {
                        model: window.navItems
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 36
                            radius: Theme.radiusSm
                            readonly property bool isCurrent: (typeof pipeline !== "undefined") && pipeline.activeWorkspace === modelData.id
                            color: isCurrent ? Theme.btnPrimaryBg : (navMa.containsMouse ? Theme.bgHover : "transparent")
                            border.width: isCurrent ? 1 : 0
                            border.color: Theme.btnPrimaryBg

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 8
                                spacing: 9

                                ShunyaIcon {
                                    name: modelData.icon
                                    size: 16
                                    inverse: isCurrent
                                    color: Theme.textSecondary
                                }
                                Text {
                                    text: modelData.label
                                    Layout.fillWidth: true
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontSm
                                    font.weight: isCurrent ? Font.DemiBold : Font.Medium
                                    color: isCurrent ? Theme.btnPrimaryFg : Theme.textSecondary
                                    elide: Text.ElideRight
                                }
                            }

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
                        text: "Command (Ctrl+K)"
                        iconName: "search"
                        compact: true
                        onClicked: commandPaletteDialog.open()
                    }
                }
            }

            // Central Workspace Container + Optional Collapsible Bottom Log Drawer
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
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

                // Bottom Status & Diagnostics Drawer (Toggled by F12 or Console button)
                Rectangle {
                    visible: window.showConsoleDrawer
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    color: Theme.bgSurface
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Diagnostics & Solver Telemetry Log"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.fillWidth: true }
                            MonoButton { iconName: "close"; compact: true; variant: "ghost"; onClicked: window.showConsoleDrawer = false }
                        }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            TextArea {
                                readOnly: true
                                text: typeof studio !== "undefined" ? studio.logOutput : ""
                                color: Theme.textSecondary
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontXs
                                background: Rectangle { color: Theme.bgPrimary; radius: Theme.radiusSm }
                            }
                        }
                    }
                }
            }
        }

        // 4. Bottom Status Strip
        Rectangle {
            Layout.fillWidth: true
            height: 24
            color: Theme.bgSurface
            border.color: Theme.borderSubtle
            border.width: 1
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Text {
                    text: typeof pipeline !== "undefined" ? pipeline.statusMessage : "Ready"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: "Shunya Phase 3  |  Elements -> Materials -> Objects -> Components -> Circuits -> Simulation"
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    color: Theme.textSubtle
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
