import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#252526"
    border.color: "#3f3f46"
    border.width: 1
    clip: true

    property string nodeId: ""
    property string panelId: ""
    property string title: "Dock Panel"
    property bool isClosable: true
    property bool isFloating: false
    default property alias content: contentHost.children

    signal closeRequested(string id)
    signal floatRequested(string id, bool floating)
    signal dragStarted(string id)
    signal dragEnded(string id)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Panel Title Bar Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#2d2d30"
            border.color: "#383838"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 6
                spacing: 6

                // Drag indicator icon / Title
                Text {
                    text: root.title
                    color: "#cccccc"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                // Float / Tear-off window button
                ToolButton {
                    id: floatBtn
                    implicitWidth: 20
                    implicitHeight: 20
                    contentItem: Text {
                        text: root.isFloating ? "🗗" : "🗖"
                        color: "#999999"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: floatBtn.hovered ? "#3e3e42" : "transparent"
                        radius: 2
                    }
                    onClicked: root.floatRequested(root.panelId, !root.isFloating)
                }

                // Close button
                ToolButton {
                    id: closeBtn
                    visible: root.isClosable
                    implicitWidth: 20
                    implicitHeight: 20
                    contentItem: Text {
                        text: "✕"
                        color: closeBtn.hovered ? "#ff6b6b" : "#999999"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: closeBtn.hovered ? "#3e3e42" : "transparent"
                        radius: 2
                    }
                    onClicked: root.closeRequested(root.panelId)
                }
            }

            // Drag handle
            MouseArea {
                anchors.fill: parent
                anchors.rightMargin: 50 // allow buttons to click
                drag.target: root.isFloating ? root : null
                onPressed: root.dragStarted(root.nodeId)
                onReleased: root.dragEnded(root.nodeId)
            }
        }

        // Panel Content Area
        Item {
            id: contentHost
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
