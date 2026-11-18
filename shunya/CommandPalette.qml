import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: root
    title: ""
    modal: true
    anchors.centerIn: parent
    width: 620
    height: 440
    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0
    z: 10005

    property string queryText: ""
    property int selectedIndex: 0

    readonly property var defaultActions: [
        { actionId: "ws.elements", title: "Switch to Chemical Elements", category: "Workspace", shortcut: "Alt+1", icon: "elements" },
        { actionId: "ws.mixing_lab", title: "Switch to Materials & Mixing Lab", category: "Workspace", shortcut: "Alt+2", icon: "mixing" },
        { actionId: "ws.objects", title: "Switch to Physical Objects", category: "Workspace", shortcut: "Alt+3", icon: "objects" },
        { actionId: "ws.components", title: "Switch to Functional Components & Battery", category: "Workspace", shortcut: "Alt+4", icon: "components" },
        { actionId: "ws.circuits", title: "Switch to Circuit Designer & Simulator", category: "Workspace", shortcut: "Alt+5", icon: "circuits" },
        { actionId: "ws.editor_2d", title: "Switch to 2D CAD Editor", category: "Workspace", shortcut: "Alt+6", icon: "editor2d" },
        { actionId: "ws.editor_3d", title: "Switch to 3D CAD Viewport", category: "Workspace", shortcut: "Alt+7", icon: "editor3d" },
        { actionId: "ws.simulation", title: "Switch to Physics Simulation", category: "Workspace", shortcut: "Alt+8", icon: "simulation" },
        { actionId: "ws.settings", title: "Switch to Project Settings", category: "Workspace", shortcut: "Alt+9", icon: "settings" },
        { actionId: "file.new", title: "File: New Project...", category: "Menu", shortcut: "Ctrl+N", icon: "new" },
        { actionId: "file.open", title: "File: Open Project...", category: "Menu", shortcut: "Ctrl+O", icon: "open" },
        { actionId: "file.save", title: "File: Save Project", category: "Menu", shortcut: "Ctrl+S", icon: "save" },
        { actionId: "file.save_as", title: "File: Save Project As...", category: "Menu", shortcut: "Ctrl+Shift+S", icon: "save" },
        { actionId: "edit.undo", title: "Edit: Undo Circuit Edit", category: "Menu", shortcut: "Ctrl+Z", icon: "undo" },
        { actionId: "edit.redo", title: "Edit: Redo Circuit Edit", category: "Menu", shortcut: "Ctrl+Y", icon: "redo" },
        { actionId: "view.dark_mode", title: "View: Toggle Dark / Light Theme", category: "View", shortcut: "", icon: "sun" },
        { actionId: "view.console", title: "View: Toggle Diagnostics Console", category: "View", shortcut: "F12", icon: "info" }
    ]

    function computeSearchResults(query) {
        var baseCmds = (typeof ribbonController !== "undefined") ? ribbonController.fuzzySearchCommands(query) : [];
        var all = [];
        var q = query.trim().toLowerCase();

        for (var i = 0; i < defaultActions.length; ++i) {
            var a = defaultActions[i];
            if (q === "" || a.title.toLowerCase().indexOf(q) !== -1 || a.category.toLowerCase().indexOf(q) !== -1 || a.actionId.toLowerCase().indexOf(q) !== -1) {
                all.push(a);
            }
        }
        for (var j = 0; j < baseCmds.length; ++j) {
            var c = baseCmds[j];
            all.push({
                actionId: c.actionId,
                title: c.title,
                category: c.category,
                shortcut: c.shortcut,
                icon: "settings"
            });
        }
        return all;
    }

    property var searchResults: computeSearchResults(queryText)

    background: Rectangle {
        color: Theme.bgSurface
        border.color: Theme.borderFocus
        border.width: 1
        radius: Theme.radiusMd
    }

    onOpened: {
        searchInput.text = "";
        root.queryText = "";
        root.selectedIndex = 0;
        searchInput.forceActiveFocus();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Search Input Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: Theme.bgElevated
            radius: Theme.radiusMd

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                ShunyaIcon {
                    name: "search"
                    size: 16
                    color: Theme.textMuted
                }

                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("Type a command, workspace or tool action (e.g. 'elements', 'save', 'wire')...")
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textPrimary
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontMd
                    background: Rectangle { color: "transparent" }

                    onTextChanged: {
                        root.queryText = text;
                        root.selectedIndex = 0;
                    }

                    Keys.onDownPressed: {
                        if (root.selectedIndex + 1 < resultsList.count) {
                            root.selectedIndex++;
                            resultsList.positionViewAtIndex(root.selectedIndex, ListView.Contain);
                        }
                    }
                    Keys.onUpPressed: {
                        if (root.selectedIndex > 0) {
                            root.selectedIndex--;
                            resultsList.positionViewAtIndex(root.selectedIndex, ListView.Contain);
                        }
                    }
                    Keys.onReturnPressed: root.triggerSelected()
                    Keys.onEnterPressed: root.triggerSelected()
                    Keys.onEscapePressed: root.close()
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.borderSubtle }

        // Results List
        ListView {
            id: resultsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.searchResults

            delegate: Rectangle {
                id: rowDelegate
                width: resultsList.width
                height: 38
                color: index === root.selectedIndex ? Theme.bgActive : (rowMouse.containsMouse ? Theme.bgHover : "transparent")

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.selectedIndex = index;
                        root.triggerSelected();
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    // Category Badge
                    Rectangle {
                        implicitWidth: 80
                        implicitHeight: 20
                        radius: Theme.radiusSm
                        color: Theme.bgElevated
                        border.color: Theme.borderSubtle
                        border.width: 1

                        Text {
                            text: modelData.category
                            color: Theme.textSecondary
                            font.family: Theme.fontSans
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            anchors.centerIn: parent
                        }
                    }

                    // Command Title
                    Text {
                        text: modelData.title
                        color: index === root.selectedIndex ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        font.weight: index === root.selectedIndex ? Font.DemiBold : Font.Normal
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    // Shortcut Badge
                    Text {
                        text: modelData.shortcut
                        visible: modelData.shortcut !== ""
                        color: Theme.textMuted
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontXs
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // Footer readout
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: Theme.bgElevated
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Text {
                    text: root.searchResults.length + " commands available (Up/Down Navigate, Enter Execute, Esc Dismiss)"
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: 10
                    Layout.fillWidth: true
                }

                Text {
                    text: "Shunya Quick Switcher"
                    color: Theme.textSubtle
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                }
            }
        }
    }

    function triggerSelected() {
        if (root.selectedIndex >= 0 && root.selectedIndex < root.searchResults.length) {
            var item = root.searchResults[root.selectedIndex];
            root.close();

            if (item.actionId.indexOf("ws.") === 0) {
                var wsId = item.actionId.substring(3);
                if (typeof pipeline !== "undefined") {
                    pipeline.setActiveWorkspace(wsId);
                }
            } else if (item.actionId === "view.dark_mode") {
                Theme.isDarkMode = !Theme.isDarkMode;
            } else if (item.actionId === "view.console") {
                window.showConsoleDrawer = !window.showConsoleDrawer;
            } else if (item.actionId === "file.new") {
                newProjectWizard.open();
            } else if (item.actionId === "file.open") {
                openFileDialog.open();
            } else if (item.actionId === "file.save") {
                if (typeof menuController !== "undefined") menuController.saveProject();
            } else if (item.actionId === "file.save_as") {
                saveFileDialog.open();
            } else if (item.actionId === "edit.undo") {
                if (typeof pipeline !== "undefined") pipeline.circuitUndo();
            } else if (item.actionId === "edit.redo") {
                if (typeof pipeline !== "undefined") pipeline.circuitRedo();
            } else if (typeof ribbonController !== "undefined") {
                ribbonController.executeCommand(item.actionId);
            }
        }
    }
}
