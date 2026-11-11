import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import shunya

Popup {
    id: root
    width: 280
    height: 320
    modal: true
    focus: true
    padding: 0

    property var history: typeof ribbonController !== "undefined" ? ribbonController.historyEntries : []

    background: Rectangle {
        color: Theme.bgSurface
        border.color: Theme.borderDefault
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
            color: Theme.bgCanvas

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Text {
                    text: "Command Undo/Redo Stack"
                    color: Theme.textPrimary
                    font.family: Theme.fontSans
                    font.bold: true
                    font.pixelSize: 11
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.borderSubtle }

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
                color: modelData.isCurrent ? Theme.bgActive : (entryMouse.hovered ? Theme.bgHover : "transparent")

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

                    ShunyaIcon {
                        name: modelData.isCurrent ? "play" : (modelData.isUndone ? "reset" : "check")
                        size: 12
                        color: modelData.isCurrent ? Theme.textPrimary : (modelData.isUndone ? Theme.textSubtle : Theme.statusSuccess)
                    }

                    Text {
                        text: (modelData.index + 1) + ". " + modelData.name
                        color: modelData.isUndone ? Theme.textSubtle : Theme.textPrimary
                        font.family: Theme.fontSans
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
