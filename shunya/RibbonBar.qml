import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    Layout.fillWidth: true
    implicitHeight: isCollapsed ? 32 : 124
    color: "#1e1e1e"
    border.color: "#3f3f46"
    border.width: 1

    property bool isCollapsed: typeof ribbonController !== "undefined" ? ribbonController.isCollapsed : false
    property var tabsData: typeof ribbonController !== "undefined" ? ribbonController.getRibbonTabs() : []
    property string activeTabId: typeof ribbonController !== "undefined" ? ribbonController.activeTabId : "tab_home"

    signal commandPaletteRequested()
    signal historyDropdownRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // Top Ribbon Header (Tabs + Quick Access)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#252526"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                RowLayout {
                    spacing: 4
                    ToolButton {
                        implicitWidth: 24; implicitHeight: 24
                        contentItem: ShunyaIcon { name: "save"; size: 14 }
                        ToolTip.visible: hovered; ToolTip.text: "Save (Ctrl+S)"
                        onClicked: if (typeof ribbonController !== "undefined") ribbonController.executeCommand("file.save");
                    }
                    ToolButton {
                        implicitWidth: 24; implicitHeight: 24
                        contentItem: ShunyaIcon { name: "undo"; size: 14; muted: !enabled }
                        enabled: typeof ribbonController !== "undefined" ? ribbonController.canUndo : true
                        ToolTip.visible: hovered; ToolTip.text: "Undo (Ctrl+Z)"
                        onClicked: if (typeof ribbonController !== "undefined") ribbonController.undo();
                    }
                    ToolButton {
                        implicitWidth: 24; implicitHeight: 24
                        contentItem: ShunyaIcon { name: "redo"; size: 14; muted: !enabled }
                        enabled: typeof ribbonController !== "undefined" ? ribbonController.canRedo : true
                        ToolTip.visible: hovered; ToolTip.text: "Redo (Ctrl+Y)"
                        onClicked: if (typeof ribbonController !== "undefined") ribbonController.redo();
                    }
                    ToolButton {
                        implicitWidth: 20; implicitHeight: 24
                        contentItem: ShunyaIcon { name: "chevron-down"; size: 12; muted: true }
                        ToolTip.visible: hovered; ToolTip.text: "Undo History Stack"
                        onClicked: root.historyDropdownRequested()
                    }
                }

                Rectangle { width: 1; height: 18; color: "#3f3f46" }

                // Ribbon Tab Headers
                Repeater {
                    model: root.tabsData
                    Button {
                        id: tabHeader
                        implicitHeight: 26
                        implicitWidth: tabText.implicitWidth + 20
                        checked: root.activeTabId === modelData.id

                        contentItem: Text {
                            id: tabText
                            text: modelData.title
                            color: tabHeader.checked ? Theme.textPrimary : Theme.textSecondary
                            font.bold: tabHeader.checked
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: tabHeader.checked ? Theme.bgPrimary : (tabHeader.hovered ? Theme.bgHover : "transparent")
                            border.color: tabHeader.checked ? Theme.borderStrong : "transparent"
                            border.width: 1
                            radius: 2
                        }

                        onClicked: {
                            if (typeof ribbonController !== "undefined") {
                                ribbonController.setActiveTab(modelData.id);
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true } // spacer

                // Fuzzy Command Palette Button (Ctrl+K)
                Button {
                    implicitHeight: 24
                    implicitWidth: 150
                    hoverEnabled: true
                    contentItem: RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        ShunyaIcon { name: "search"; size: 12; muted: true }
                        Text { text: "Search (Ctrl+K)..."; color: Theme.textMuted; font.pixelSize: 11 }
                    }
                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : Theme.bgElevated
                        border.color: parent.hovered ? Theme.borderStrong : Theme.borderDefault
                        radius: 4
                    }
                    onClicked: root.commandPaletteRequested()
                }

                // Ribbon Collapse / Expand Toggle
                ToolButton {
                    implicitWidth: 24; implicitHeight: 24
                    contentItem: ShunyaIcon {
                        name: root.isCollapsed ? "chevron-down" : "chevron-up"
                        size: 12
                        muted: true
                    }
                    ToolTip.visible: hovered; ToolTip.text: root.isCollapsed ? "Expand Ribbon" : "Collapse Ribbon"
                    onClicked: {
                        if (typeof ribbonController !== "undefined") {
                            ribbonController.toggleCollapsed();
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Ribbon Groups Container
        // =====================================================================
        Rectangle {
            visible: !root.isCollapsed
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e1e"

            Repeater {
                model: root.tabsData
                RibbonTab {
                    anchors.fill: parent
                    visible: root.activeTabId === modelData.id
                    groups: modelData.groups
                }
            }
        }
    }
}
