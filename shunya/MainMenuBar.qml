import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

MenuBar {
    id: root
    implicitHeight: 30
    height: 30
    z: 10000
    palette.window: Theme.bgSurface
    palette.windowText: Theme.textPrimary

    signal newProjectRequested()
    signal openProjectRequested()
    signal saveProjectRequested()
    signal saveProjectAsRequested()
    signal closeProjectRequested()
    signal exitRequested()

    property string pendingImportFmt: "DXF"
    property string pendingExportFmt: "BUNDLE"

    // Alt-Key Mnemonics (Alt+F, Alt+E, Alt+W, Alt+S, Alt+V, Alt+H)
    Shortcut { sequence: "Alt+F"; onActivated: fileMenu.open() }
    Shortcut { sequence: "Alt+E"; onActivated: editMenu.open() }
    Shortcut { sequence: "Alt+W"; onActivated: workspacesMenu.open() }
    Shortcut { sequence: "Alt+S"; onActivated: simulationMenu.open() }
    Shortcut { sequence: "Alt+V"; onActivated: viewMenu.open() }
    Shortcut { sequence: "Alt+H"; onActivated: helpMenu.open() }

    FileDialog {
        id: importFileDlg
        title: "Import " + root.pendingImportFmt + " File"
        fileMode: FileDialog.OpenFile
        nameFilters: root.pendingImportFmt === "DXF"
            ? ["AutoCAD DXF (*.dxf)", "All Files (*)"]
            : (root.pendingImportFmt === "SVG"
                ? ["Scalable Vector Graphics (*.svg)", "All Files (*)"]
                : ["JSON Files (*.json)", "All Files (*)"])
        onAccepted: {
            if (typeof menuController !== "undefined") {
                menuController.importFile(root.pendingImportFmt, selectedFile.toString());
            }
        }
    }

    FileDialog {
        id: exportFileDlg
        title: "Export " + root.pendingExportFmt + " File"
        fileMode: FileDialog.SaveFile
        nameFilters: root.pendingExportFmt === "DXF"
            ? ["AutoCAD DXF (*.dxf)"]
            : (root.pendingExportFmt === "SVG"
                ? ["Scalable Vector Graphics (*.svg)"]
                : ["Shunya v3 Bundle (*.shunya3.json)", "All Files (*)"])
        onAccepted: {
            if (typeof menuController !== "undefined") {
                menuController.exportFile(root.pendingExportFmt, selectedFile.toString());
            }
        }
    }

    // Top-Level Menu Bar Background (VS Code 30px Dark/Light Aesthetic)
    background: Rectangle {
        implicitHeight: 30
        height: 30
        color: Theme.bgSurface
        border.color: Theme.borderSubtle
        border.width: 1
    }

    // Menu Bar Top-Level Item Delegate (30px Height, 8px Horizontal Padding)
    delegate: MenuBarItem {
        id: menuBarItem
        implicitHeight: 30
        height: 30
        leftPadding: 8
        rightPadding: 8

        contentItem: Text {
            text: menuBarItem.text.replace("&", "")
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSm
            font.weight: menuBarItem.highlighted ? Font.DemiBold : Font.Medium
            color: menuBarItem.highlighted ? Theme.textPrimary : Theme.textSecondary
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            implicitWidth: 44
            implicitHeight: 30
            radius: Theme.radiusSm
            color: menuBarItem.highlighted ? Theme.bgHover : "transparent"
        }
    }

    // Reusable Custom VS Code Menu Item Component
    component VSCodeMenuItem: MenuItem {
        id: vsItem
        implicitHeight: 28
        implicitWidth: 230
        leftPadding: 8
        rightPadding: 8
        topPadding: 0
        bottomPadding: 0
        opacity: vsItem.enabled ? 1.0 : 0.40

        background: Rectangle {
            color: vsItem.highlighted ? Theme.bgHover : "transparent"
            radius: Theme.radiusSm
        }

        contentItem: RowLayout {
            spacing: 8
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            // Check tick slot for checkable items
            Item {
                implicitWidth: 16
                implicitHeight: 16
                visible: vsItem.checkable

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    font.bold: true
                    color: vsItem.highlighted ? Theme.textPrimary : Theme.textSecondary
                    visible: vsItem.checked
                }
            }

            // Label text
            Text {
                text: vsItem.text.replace("&", "")
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSm
                color: vsItem.highlighted ? Theme.textPrimary : Theme.textSecondary
                Layout.fillWidth: true
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            // Right-aligned monospace keyboard shortcut badge
            Text {
                text: vsItem.action ? vsItem.action.shortcut : ""
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontXs
                color: Theme.textMuted
                visible: text !== ""
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }

            // Chevron indicator for submenus
            Text {
                text: "›"
                font.family: Theme.fontSans
                font.pixelSize: 14
                font.bold: true
                color: vsItem.highlighted ? Theme.textPrimary : Theme.textMuted
                visible: vsItem.subMenu !== null && typeof vsItem.subMenu !== "undefined"
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // Reusable Custom VS Code Menu Separator Component
    component VSCodeMenuSeparator: MenuSeparator {
        id: vsSep
        implicitHeight: 9
        topPadding: 4
        bottomPadding: 4

        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.borderSubtle
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 8
        }

        background: Rectangle {
            implicitHeight: 9
            color: "transparent"
        }
    }

    // Reusable Custom VS Code Menu Popup Background
    component VSCodeMenuBackground: Rectangle {
        implicitWidth: 230
        color: Theme.bgSurface
        border.color: Theme.borderDefault
        border.width: 1
        radius: Theme.radiusSm
    }

    // 1. FILE MENU (Alt+F)
    Menu {
        id: fileMenu
        title: qsTr("&File")
        y: 30
        z: 10000
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        background: VSCodeMenuBackground {}
        delegate: VSCodeMenuItem {}

        Action { text: qsTr("&New Project..."); shortcut: "Ctrl+N"; onTriggered: root.newProjectRequested() }
        Action { text: qsTr("&Open Project..."); shortcut: "Ctrl+O"; onTriggered: root.openProjectRequested() }

        Menu {
            id: recentMenu
            title: qsTr("&Recent Projects")
            y: 0
            z: 10001
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
            background: VSCodeMenuBackground {}
            delegate: VSCodeMenuItem {}
            enabled: typeof menuController !== "undefined" && menuController.hasRecentProjects

            Instantiator {
                model: typeof menuController !== "undefined" ? menuController.recentProjects : []
                delegate: VSCodeMenuItem {
                    text: modelData.title + (modelData.isPinned ? " [Pinned]" : "")
                    onTriggered: if (typeof menuController !== "undefined") menuController.openProject(modelData.path)
                }
                onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => recentMenu.removeItem(object)
            }

            VSCodeMenuSeparator {}
            VSCodeMenuItem {
                text: qsTr("Clear Recent Projects")
                onTriggered: if (typeof menuController !== "undefined") menuController.clearRecentProjects()
            }
        }

        VSCodeMenuSeparator {}
        Action {
            text: qsTr("&Save")
            shortcut: "Ctrl+S"
            enabled: typeof menuController !== "undefined" ? menuController.isProjectDirty : true
            onTriggered: root.saveProjectRequested()
        }
        Action { text: qsTr("Save &As..."); shortcut: "Ctrl+Shift+S"; onTriggered: root.saveProjectAsRequested() }

        VSCodeMenuSeparator {}
        Menu {
            id: importMenu
            title: qsTr("&Import")
            y: 0
            z: 10001
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
            background: VSCodeMenuBackground {}
            delegate: VSCodeMenuItem {}

            VSCodeMenuItem { text: qsTr("AutoCAD &DXF (*.dxf)..."); onTriggered: { root.pendingImportFmt = "DXF"; importFileDlg.open(); } }
            VSCodeMenuItem { text: qsTr("Scalable Vector Graphics (*.&svg)..."); onTriggered: { root.pendingImportFmt = "SVG"; importFileDlg.open(); } }
            VSCodeMenuItem { text: qsTr("&Material JSON (*.json)..."); onTriggered: { root.pendingImportFmt = "MATERIAL_JSON"; importFileDlg.open(); } }
        }
        Menu {
            id: exportMenu
            title: qsTr("&Export")
            y: 0
            z: 10001
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
            background: VSCodeMenuBackground {}
            delegate: VSCodeMenuItem {}

            VSCodeMenuItem { text: qsTr("Shunya v3 &Bundle (*.shunya3.json)..."); onTriggered: { root.pendingExportFmt = "BUNDLE"; exportFileDlg.open(); } }
            VSCodeMenuItem { text: qsTr("AutoCAD &DXF Sketch (*.dxf)..."); onTriggered: { root.pendingExportFmt = "DXF"; exportFileDlg.open(); } }
            VSCodeMenuItem { text: qsTr("&SVG Vector Sketch (*.svg)..."); onTriggered: { root.pendingExportFmt = "SVG"; exportFileDlg.open(); } }
        }

        VSCodeMenuSeparator {}
        Action { text: qsTr("&Close Project"); shortcut: "Ctrl+W"; onTriggered: root.closeProjectRequested() }
        Action { text: qsTr("E&xit"); shortcut: "Ctrl+Q"; onTriggered: root.exitRequested() }
    }

    // 2. EDIT MENU (Alt+E)
    Menu {
        id: editMenu
        title: qsTr("&Edit")
        y: 30
        z: 10000
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        background: VSCodeMenuBackground {}
        delegate: VSCodeMenuItem {}

        Action { text: qsTr("&Undo Circuit Edit"); shortcut: "Ctrl+Z"; onTriggered: if (typeof pipeline !== "undefined") pipeline.circuitUndo() }
        Action { text: qsTr("&Redo Circuit Edit"); shortcut: "Ctrl+Y"; onTriggered: if (typeof pipeline !== "undefined") pipeline.circuitRedo() }
        VSCodeMenuSeparator {}
        Action { text: qsTr("Prefere&nces & Theme"); shortcut: "Ctrl+,"; onTriggered: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("settings") }
    }

    // 3. WORKSPACES MENU (Alt+W)
    Menu {
        id: workspacesMenu
        title: qsTr("&Workspaces")
        y: 30
        z: 10000
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        background: VSCodeMenuBackground {}
        delegate: VSCodeMenuItem {}

        Action { text: qsTr("1. Chemical &Elements"); shortcut: "Alt+1"; onTriggered: pipeline.setActiveWorkspace("elements") }
        Action { text: qsTr("2. &Materials & Mixing Lab"); shortcut: "Alt+2"; onTriggered: pipeline.setActiveWorkspace("mixing_lab") }
        Action { text: qsTr("3. Physical &Objects"); shortcut: "Alt+3"; onTriggered: pipeline.setActiveWorkspace("objects") }
        Action { text: qsTr("4. &Components & Battery"); shortcut: "Alt+4"; onTriggered: pipeline.setActiveWorkspace("components") }
        Action { text: qsTr("5. &Circuit Designer & Simulator"); shortcut: "Alt+5"; onTriggered: pipeline.setActiveWorkspace("circuits") }
        Action { text: qsTr("6. 2&D CAD Editor"); shortcut: "Alt+6"; onTriggered: pipeline.setActiveWorkspace("editor_2d") }
        Action { text: qsTr("7. 3D CAD &Viewport"); shortcut: "Alt+7"; onTriggered: pipeline.setActiveWorkspace("editor_3d") }
        Action { text: qsTr("8. Physics &Simulation"); shortcut: "Alt+8"; onTriggered: pipeline.setActiveWorkspace("simulation") }
        Action { text: qsTr("9. Project &Settings"); shortcut: "Alt+9"; onTriggered: pipeline.setActiveWorkspace("settings") }
    }

    // 4. SIMULATION MENU (Alt+S)
    Menu {
        id: simulationMenu
        title: qsTr("&Simulation")
        y: 30
        z: 10000
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        background: VSCodeMenuBackground {}
        delegate: VSCodeMenuItem {}

        Action { text: qsTr("&Start Simulation"); shortcut: "F5"; onTriggered: if (typeof studio !== "undefined") studio.startSimulation() }
        Action { text: qsTr("Step &Forward"); shortcut: "F10"; onTriggered: if (typeof studio !== "undefined") studio.stepSimulation() }
        Action { text: qsTr("&Pause"); shortcut: "F6"; onTriggered: if (typeof studio !== "undefined") studio.pauseSimulation() }
        Action { text: qsTr("S&top"); shortcut: "Shift+F5"; onTriggered: if (typeof studio !== "undefined") studio.stopSimulation() }
        VSCodeMenuSeparator {}
        VSCodeMenuItem { text: qsTr("Load Lithium &Battery Workflow Demo"); onTriggered: if (typeof pipeline !== "undefined") pipeline.seedLithiumBatteryDemo() }
        VSCodeMenuItem { text: qsTr("&Reset Clock"); onTriggered: if (typeof studio !== "undefined") studio.resetSimulation() }
    }

    // 5. VIEW MENU (Alt+V)
    Menu {
        id: viewMenu
        title: qsTr("&View")
        y: 30
        z: 10000
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        background: VSCodeMenuBackground {}
        delegate: VSCodeMenuItem {}

        VSCodeMenuItem {
            text: qsTr("Toggle &Left Sidebar")
            shortcut: "Ctrl+B"
            checkable: true
            checked: !window.leftRailCollapsed
            onTriggered: window.leftRailCollapsed = !window.leftRailCollapsed
        }
        VSCodeMenuItem {
            text: qsTr("Toggle &Details Panel")
            shortcut: "Ctrl+Alt+B"
            checkable: true
            checked: !window.rightPanelCollapsed
            onTriggered: window.rightPanelCollapsed = !window.rightPanelCollapsed
        }
        VSCodeMenuItem {
            text: qsTr("Toggle &Diagnostics Console")
            shortcut: "F12"
            checkable: true
            checked: window.showConsoleDrawer
            onTriggered: window.showConsoleDrawer = !window.showConsoleDrawer
        }
        VSCodeMenuSeparator {}
        Menu {
            title: qsTr("&Theme")
            y: 0
            background: VSCodeMenuBackground {}
            delegate: VSCodeMenuItem {}
            Action {
                text: qsTr("Light")
                checkable: true
                checked: !Theme.isDarkMode
                onTriggered: Theme.isDarkMode = false
            }
            Action {
                text: qsTr("Dark")
                checkable: true
                checked: Theme.isDarkMode
                onTriggered: Theme.isDarkMode = true
            }
        }
        VSCodeMenuSeparator {}
        Action {
            text: qsTr("&Reset Workspace Layout")
            onTriggered: {
                window.leftRailWidth = 184;
                window.leftRailCollapsed = false;
                window.consoleDrawerHeight = 150;
                window.showConsoleDrawer = false;
                window.rightPanelCollapsed = false;
                window.rightPanelWidth = 360;
            }
        }
        Action {
            text: qsTr("Toggle &Fullscreen")
            shortcut: "F11"
            onTriggered: {
                if (window.visibility === Window.FullScreen) window.showNormal();
                else window.showFullScreen();
            }
        }
    }

    // 5. HELP MENU (Alt+H)
    Menu {
        id: helpMenu
        title: qsTr("&Help")
        y: 30
        z: 10000
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        background: VSCodeMenuBackground {}
        delegate: VSCodeMenuItem {}

        Action { text: qsTr("Command &Palette..."); shortcut: "Ctrl+K"; onTriggered: commandPaletteDialog.open() }
        VSCodeMenuItem { text: qsTr("&About Shunya Studio..."); onTriggered: aboutDialog.open() }
    }

    Dialog {
        id: aboutDialog
        title: qsTr("About Shunya Engineering Studio")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok

        background: Rectangle {
            color: Theme.bgSurface
            border.color: Theme.borderDefault
            radius: Theme.radiusMd
        }

        ColumnLayout {
            spacing: 8
            Label {
                text: "Shunya Chemistry & Multiphysics Engineering Studio"
                font.family: Theme.fontSans
                font.bold: true
                font.pixelSize: Theme.fontLg
                color: Theme.textPrimary
            }
            Label {
                text: "Phase 4 Unified Desktop Engineering Interface | Version 4.0.0"
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSm
                color: Theme.textMuted
            }
            Label {
                text: "Elements -> Materials & Mixing -> Physical Objects -> Functional Components -> Battery -> Circuits -> CAD -> Simulation."
                font.family: Theme.fontSans
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
            }
        }
    }
}
