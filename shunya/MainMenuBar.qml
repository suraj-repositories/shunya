import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

MenuBar {
    id: root
    palette.window: window.panelDark
    palette.windowText: window.textDark

    signal newProjectRequested()
    signal openProjectRequested()
    signal saveProjectRequested()
    signal saveProjectAsRequested()
    signal closeProjectRequested()
    signal exitRequested()

    background: Rectangle {
        color: "#1e1e1e"
        border.color: window.borderDark
        border.width: 1
    }

    delegate: MenuBarItem {
        id: menuBarItem
        contentItem: Text {
            text: menuBarItem.text
            font.pixelSize: 13
            font.weight: Font.Medium
            color: menuBarItem.highlighted ? window.accentColor : window.textDark
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            implicitWidth: 40
            implicitHeight: 28
            color: menuBarItem.highlighted ? "#2a2d32" : "transparent"
        }
    }

    // =========================================================================
    // FILE MENU
    // =========================================================================
    Menu {
        title: qsTr("&File")
        overlap: 1

        Action {
            text: qsTr("&New Project...")
            shortcut: "Ctrl+N"
            onTriggered: root.newProjectRequested()
        }

        Action {
            text: qsTr("&Open Project...")
            shortcut: "Ctrl+O"
            onTriggered: root.openProjectRequested()
        }

        Menu {
            id: recentMenu
            title: qsTr("&Recent Projects")
            enabled: typeof menuController !== "undefined" && menuController.hasRecentProjects

            Instantiator {
                model: typeof menuController !== "undefined" ? menuController.recentProjects : []
                delegate: MenuItem {
                    text: modelData.title + (modelData.isPinned ? " [Pinned]" : "")
                    onTriggered: {
                        if (typeof menuController !== "undefined") {
                            menuController.openProject(modelData.path);
                        }
                    }
                }
                onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => recentMenu.removeItem(object)
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Clear Recent Projects")
                onTriggered: {
                    if (typeof menuController !== "undefined") {
                        menuController.clearRecentProjects();
                    }
                }
            }
        }

        MenuSeparator {}

        Action {
            text: qsTr("&Save")
            shortcut: "Ctrl+S"
            enabled: typeof menuController !== "undefined" ? menuController.isProjectDirty : true
            onTriggered: root.saveProjectRequested()
        }

        Action {
            text: qsTr("Save &As...")
            shortcut: "Ctrl+Shift+S"
            onTriggered: root.saveProjectAsRequested()
        }

        MenuSeparator {}

        Menu {
            title: qsTr("&Import")

            MenuItem {
                text: qsTr("AutoCAD &DXF (*.dxf)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.importFile("DXF", "sample.dxf");
                }
            }
            MenuItem {
                text: qsTr("Scalable Vector Graphics (*.&svg)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.importFile("SVG", "schematic.svg");
                }
            }
            MenuItem {
                text: qsTr("&STEP 3D CAD (*.step, *.stp)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.importFile("STEP", "assembly.step");
                }
            }
            MenuItem {
                text: qsTr("SPICE &Netlist (*.cir, *.net)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.importFile("SPICE", "circuit.net");
                }
            }
        }

        Menu {
            title: qsTr("&Export")

            MenuItem {
                text: qsTr("&STEP Solid Model (*.step)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.exportFile("STEP", "export_model.step");
                }
            }
            MenuItem {
                text: qsTr("&Gerber RS-274X Package (*.zip)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.exportFile("Gerber", "pcb_gerbers.zip");
                }
            }
            MenuItem {
                text: qsTr("Engineering &Validation Report (*.pdf)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.exportFile("PDF_Report", "validation_report.pdf");
                }
            }
            MenuItem {
                text: qsTr("Simulation &Telemetry (*.csv)...")
                onTriggered: {
                    if (typeof menuController !== "undefined") menuController.exportFile("CSV", "telemetry.csv");
                }
            }
        }

        MenuSeparator {}

        Action {
            text: qsTr("&Close Project")
            shortcut: "Ctrl+W"
            onTriggered: root.closeProjectRequested()
        }

        Action {
            text: qsTr("E&xit")
            shortcut: "Ctrl+Q"
            onTriggered: root.exitRequested()
        }
    }

    // =========================================================================
    // EDIT MENU
    // =========================================================================
    Menu {
        title: qsTr("&Edit")
        overlap: 1

        Action {
            text: qsTr("&Undo")
            shortcut: "Ctrl+Z"
            onTriggered: { if (typeof studio !== "undefined") studio.appendLog("[Edit] Undo invoked"); }
        }
        Action {
            text: qsTr("&Redo")
            shortcut: "Ctrl+Y"
            onTriggered: { if (typeof studio !== "undefined") studio.appendLog("[Edit] Redo invoked"); }
        }
        MenuSeparator {}
        Action {
            text: qsTr("Cu&t")
            shortcut: "Ctrl+X"
        }
        Action {
            text: qsTr("&Copy")
            shortcut: "Ctrl+C"
        }
        Action {
            text: qsTr("&Paste")
            shortcut: "Ctrl+V"
        }
        MenuSeparator {}
        Action {
            text: qsTr("Prefere&nces...")
            shortcut: "Ctrl+,"
            onTriggered: { if (typeof studio !== "undefined") studio.appendLog("[Settings] Preferences opened"); }
        }
    }

    // =========================================================================
    // VIEW MENU
    // =========================================================================
    Menu {
        title: qsTr("&View")
        overlap: 1

        MenuItem {
            text: qsTr("&Modeling Workspace")
            onTriggered: window.currentPreset = "Modeling"
        }
        MenuItem {
            text: qsTr("&Schematic Workspace")
            onTriggered: window.currentPreset = "Schematic"
        }
        MenuItem {
            text: qsTr("&Simulation Workspace")
            onTriggered: window.currentPreset = "Simulation"
        }
        MenuItem {
            text: qsTr("&Results Workspace")
            onTriggered: window.currentPreset = "Results"
        }
        MenuItem {
            text: qsTr("Scri&pting Workspace")
            onTriggered: window.currentPreset = "Scripting"
        }
        MenuSeparator {}
        Action {
            text: qsTr("Toggle &Diagnostics Console")
            shortcut: "F12"
            onTriggered: window.showConsoleDrawer = !window.showConsoleDrawer
        }
    }

    // =========================================================================
    // SIMULATION MENU
    // =========================================================================
    Menu {
        title: qsTr("&Simulation")
        overlap: 1

        Action {
            text: qsTr("&Start Simulation")
            shortcut: "F5"
            onTriggered: { if (typeof studio !== "undefined") studio.startSimulation(); }
        }
        Action {
            text: qsTr("Step &Forward")
            shortcut: "F10"
            onTriggered: { if (typeof studio !== "undefined") studio.stepSimulation(); }
        }
        Action {
            text: qsTr("&Pause")
            shortcut: "F6"
            onTriggered: { if (typeof studio !== "undefined") studio.pauseSimulation(); }
        }
        Action {
            text: qsTr("S&top")
            shortcut: "Shift+F5"
            onTriggered: { if (typeof studio !== "undefined") studio.stopSimulation(); }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("&Reset Clock")
            onTriggered: { if (typeof studio !== "undefined") studio.resetSimulation(); }
        }
    }

    // =========================================================================
    // HELP MENU
    // =========================================================================
    Menu {
        title: qsTr("&Help")
        overlap: 1

        Action {
            text: qsTr("&Documentation")
            shortcut: "F1"
            onTriggered: { if (typeof studio !== "undefined") studio.appendLog("[Help] Opening documentation portal"); }
        }
        MenuItem {
            text: qsTr("&About Shunya Studio...")
            onTriggered: aboutDialog.open()
        }
    }

    Dialog {
        id: aboutDialog
        title: qsTr("About Shunya Studio")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok

        ColumnLayout {
            spacing: 8
            Label {
                text: "Shunya Virtual Engineering & Multiphysics Studio"
                font.bold: true
                font.pixelSize: 15
                color: window.accentColor
            }
            Label {
                text: "Version 2.5.0-dev | Phase 2 Active Development"
                font.pixelSize: 12
                color: window.textMuted
            }
            Label {
                text: "Cross-domain scientific simulation: Circuits, Thermal, Rigid Dynamics, Fluids & Chemistry."
                wrapMode: Text.WordWrap
                color: window.textDark
            }
        }
    }
}
