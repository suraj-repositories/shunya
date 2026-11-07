import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: root
    width: 280
    height: 320
    modal: true
    focus: true
    padding: 0

    property var history: typeof ribbonController !== "undefined" ? ribbonController.historyEntries : []

    background: Rectangle {
        color: "#252526"
        border.color: "#3f3f46"
        border.width: 1
        radius: 4
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#1e1e1e"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Text {
                    text: "Command Undo/Redo Stack"
                    color: "#cccccc"
                    font.bold: true
                    font.pixelSize: 11
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#3f3f46" }

        // Stack ListView
        ListView {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.history

            delegate: Rectangle {
                id: entryDelegate
                width: stackView.width
                height: 32
                color: modelData.isCurrent ? "#094771" : (entryMouse.hovered ? "#2a2d32" : "transparent")

                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (typeof ribbonController !== "undefined") {
                            ribbonController.rollbackToIndex(modelData.index + 1);
                        }
                        root.close();
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: modelData.isCurrent ? "➔" : (modelData.isUndone ? "○" : "●")
                        color: modelData.isCurrent ? "#00b4d8" : (modelData.isUndone ? "#666666" : "#06d6a0")
                        font.pixelSize: 12
                    }

                    Text {
                        text: (modelData.index + 1) + ". " + modelData.name
                        color: modelData.isUndone ? "#777777" : "white"
                        font.pixelSize: 11
                        font.strikeout: modelData.isUndone
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // Footer Rollback to Start button
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#1e1e1e"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8

                Button {
                    Layout.fillWidth: true
                    implicitHeight: 24
                    text: "Rollback All to Initial State"
                    contentItem: Text {
                        text: parent.text
                        color: "#ef476f"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.down ? "#3a1b24" : (parent.hovered ? "#2a151b" : "transparent")
                        border.color: "#ef476f"
                        radius: 3
                    }
                    onClicked: {
                        if (typeof ribbonController !== "undefined") {
                            ribbonController.rollbackToIndex(0);
                        }
                        root.close();
                    }
                }
            }
        }
    }
}
