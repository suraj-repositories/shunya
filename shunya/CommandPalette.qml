import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: root
    title: ""
    modal: true
    anchors.centerIn: parent
    width: 580
    height: 420
    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    property string queryText: ""
    property var searchResults: typeof ribbonController !== "undefined" ? ribbonController.fuzzySearchCommands(queryText) : []
    property int selectedIndex: 0

    background: Rectangle {
        color: "#1e1e1e"
        border.color: "#007acc"
        border.width: 1
        radius: 6
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
            color: "#252526"
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "🔍"
                    font.pixelSize: 14
                    color: "#888888"
                }

                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    placeholderText: "Type a command or search action (e.g. 'run', 'extrude', 'drc')..."
                    placeholderTextColor: "#666666"
                    color: "white"
                    font.pixelSize: 13
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

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#3f3f46" }

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
                height: 40
                color: index === root.selectedIndex ? "#094771" : (rowMouse.hovered ? "#2a2d32" : "transparent")

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.selectedIndex = index;
                        root.triggerSelected();
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10

                    // Category Badge
                    Rectangle {
                        implicitWidth: 80
                        implicitHeight: 20
                        radius: 3
                        color: "#2d2d30"
                        border.color: "#3f3f46"
                        Text {
                            text: modelData.category
                            color: "#00b4d8"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            anchors.centerIn: parent
                        }
                    }

                    // Command Title
                    Text {
                        text: modelData.title
                        color: "white"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    // Shortcut Badge
                    Text {
                        text: modelData.shortcut
                        visible: modelData.shortcut !== ""
                        color: "#888888"
                        font.pixelSize: 11
                    }
                }
            }
        }

        // Footer readout
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: "#181818"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Text {
                    text: root.searchResults.length + " commands available (↑↓ Navigate, Enter Execute, Esc Dismiss)"
                    color: "#666666"
                    font.pixelSize: 10
                }
            }
        }
    }

    function triggerSelected() {
        if (root.selectedIndex >= 0 && root.selectedIndex < root.searchResults.length) {
            var item = root.searchResults[root.selectedIndex];
            if (typeof ribbonController !== "undefined") {
                ribbonController.executeCommand(item.actionId);
            }
            root.close();
        }
    }
}
