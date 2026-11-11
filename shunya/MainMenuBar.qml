import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

MenuBar {
    id: root
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

    background: Rectangle {
        color: Theme.bgSurface
        border.color: Theme.borderSubtle
        border.width: 1
    }

    delegate: MenuBarItem {
        id: menuBarItem
        contentItem: Text {
            text: menuBarItem.text
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
            color: menuBarItem.highlighted ? Theme.bgHover : "transparent"
        }
    }

    Menu {
        title: qsTr("&File")
        overlap: 1

        Action { text: qsTr("&New Project..."); shortcut: "Ctrl+N"; onTriggered: root.newProjectRequested() }
        Action { text: qsTr("&Open Project..."); shortcut: "Ctrl+O"; onTriggered: root.openProjectRequested() }

        Menu {
            id: recentMenu
            title: qsTr("&Recent Projects")
            enabled: typeof menuController !== "undefined" && menuController.hasRecentProjects

            Instantiator {
                model: typeof menuController !== "undefined" ? menuController.recentProjects : []
                delegate: MenuItem {
                    text: modelData.title + (modelData.isPinned ? " [Pinned]" : "")
                    onTriggered: if (typeof menuController !== "undefined") menuController.openProject(modelData.path)
                }
                onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => recentMenu.removeItem(object)
            }

            MenuSeparator {}
            MenuItem {
                text: qsTr("Clear Recent Projects")
                onTriggered: if (typeof menuController !== "undefined") menuController.clearRecentProjects()
            }
        }

        MenuSeparator {}
        Action {
            text: qsTr("&Save")
            shortcut: "Ctrl+S"
            enabled: typeof menuController !== "undefined" ? menuController.isProjectDirty : true
            onTriggered: root.saveProjectRequested()
        }
        Action { text: qsTr("Save &As..."); shortcut: "Ctrl+Shift+S"; onTriggered: root.saveProjectAsRequested() }

        MenuSeparator {}
        Menu {
            title: qsTr("&Import")
            MenuItem { text: qsTr("AutoCAD &DXF (*.dxf)..."); onTriggered: { root.pendingImportFmt = "DXF"; importFileDlg.open(); } }
            MenuItem { text: qsTr("Scalable Vector Graphics (*.&svg)..."); onTriggered: { root.pendingImportFmt = "SVG"; importFileDlg.open(); } }
            MenuItem { text: qsTr("&Material JSON (*.json)..."); onTriggered: { root.pendingImportFmt = "MATERIAL_JSON"; importFileDlg.open(); } }
        }
        Menu {
            title: qsTr("&Export")
            MenuItem { text: qsTr("Shunya v3 &Bundle (*.shunya3.json)..."); onTriggered: { root.pendingExportFmt = "BUNDLE"; exportFileDlg.open(); } }
            MenuItem { text: qsTr("AutoCAD &DXF Sketch (*.dxf)..."); onTriggered: { root.pendingExportFmt = "DXF"; exportFileDlg.open(); } }
            MenuItem { text: qsTr("&SVG Vector Sketch (*.svg)..."); onTriggered: { root.pendingExportFmt = "SVG"; exportFileDlg.open(); } }
        }

        MenuSeparator {}
        Action { text: qsTr("&Close Project"); shortcut: "Ctrl+W"; onTriggered: root.closeProjectRequested() }
        Action { text: qsTr("E&xit"); shortcut: "Ctrl+Q"; onTriggered: root.exitRequested() }
    }

    Menu {
        title: qsTr("&Edit")
        overlap: 1
        Action { text: qsTr("&Undo Circuit Edit"); shortcut: "Ctrl+Z"; onTriggered: if (typeof pipeline !== "undefined") pipeline.circuitUndo() }
        Action { text: qsTr("&Redo Circuit Edit"); shortcut: "Ctrl+Y"; onTriggered: if (typeof pipeline !== "undefined") pipeline.circuitRedo() }
        MenuSeparator {}
        Action { text: qsTr("Prefere&nces & Theme"); shortcut: "Ctrl+,"; onTriggered: if (typeof pipeline !== "undefined") pipeline.setActiveWorkspace("settings") }
    }

    Menu {
        title: qsTr("&Workspaces")
        overlap: 1
        MenuItem { text: qsTr("1. Chemical &Elements"); onTriggered: pipeline.setActiveWorkspace("elements") }
        MenuItem { text: qsTr("2. &Materials & Mixing Lab"); onTriggered: pipeline.setActiveWorkspace("mixing_lab") }
        MenuItem { text: qsTr("3. Physical &Objects"); onTriggered: pipeline.setActiveWorkspace("objects") }
        MenuItem { text: qsTr("4. &Components & Battery"); onTriggered: pipeline.setActiveWorkspace("components") }
        MenuItem { text: qsTr("5. &Circuit Designer & Simulator"); onTriggered: pipeline.setActiveWorkspace("circuits") }
        MenuItem { text: qsTr("6. 2&D CAD Editor"); onTriggered: pipeline.setActiveWorkspace("editor_2d") }
        MenuItem { text: qsTr("7. 3D CAD &Viewport"); onTriggered: pipeline.setActiveWorkspace("editor_3d") }
        MenuItem { text: qsTr("8. Physics &Simulation"); onTriggered: pipeline.setActiveWorkspace("simulation") }
        MenuItem { text: qsTr("9. Project &Settings"); onTriggered: pipeline.setActiveWorkspace("settings") }
        MenuSeparator {}
        MenuItem { text: qsTr("Switch Light / Dark Mode"); onTriggered: Theme.isDarkMode = !Theme.isDarkMode }
        Action { text: qsTr("Toggle &Diagnostics Console"); shortcut: "F12"; onTriggered: window.showConsoleDrawer = !window.showConsoleDrawer }
    }

    Menu {
        title: qsTr("&Simulation")
        overlap: 1
        Action { text: qsTr("&Start Simulation"); shortcut: "F5"; onTriggered: if (typeof studio !== "undefined") studio.startSimulation() }
        Action { text: qsTr("Step &Forward"); shortcut: "F10"; onTriggered: if (typeof studio !== "undefined") studio.stepSimulation() }
        Action { text: qsTr("&Pause"); shortcut: "F6"; onTriggered: if (typeof studio !== "undefined") studio.pauseSimulation() }
        Action { text: qsTr("S&top"); shortcut: "Shift+F5"; onTriggered: if (typeof studio !== "undefined") studio.stopSimulation() }
        MenuSeparator {}
        MenuItem { text: qsTr("Load Lithium &Battery Workflow Demo"); onTriggered: if (typeof pipeline !== "undefined") pipeline.seedLithiumBatteryDemo() }
        MenuItem { text: qsTr("&Reset Clock"); onTriggered: if (typeof studio !== "undefined") studio.resetSimulation() }
    }

    Menu {
        title: qsTr("&Help")
        overlap: 1
        Action { text: qsTr("Command &Palette..."); shortcut: "Ctrl+K"; onTriggered: commandPaletteDialog.open() }
        MenuItem { text: qsTr("&About Shunya Studio..."); onTriggered: aboutDialog.open() }
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
                text: "Phase 3 Unified Workflow Platform | Version 3.0.0"
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSm
                color: Theme.textMuted
            }
            Label {
                text: "Elements -> Materials & Mixing -> Physical Objects -> Functional Components -> Battery -> Circuits -> Physics Simulation."
                font.family: Theme.fontSans
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
            }
        }
    }
}
