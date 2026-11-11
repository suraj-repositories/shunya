import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: 88
    color: "#252526"
    border.color: "#3f3f46"
    border.width: 1
    radius: 4

    property string title: "Group"
    property var actions: []

    signal actionTriggered(string actionId)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 2

        // Actions Container
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            Repeater {
                model: root.actions
                Button {
                    id: btn
                    Layout.fillHeight: true
                    implicitWidth: modelData.isLarge ? 56 : 36
                    hoverEnabled: true

                    ToolTip.visible: hovered && modelData.tooltip !== ""
                    ToolTip.text: modelData.tooltip + (modelData.shortcut !== "" ? " (" + modelData.shortcut + ")" : "")

                    contentItem: ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2

                        ShunyaIcon {
                            name: {
                                if (modelData.id.indexOf("run") !== -1) return "play";
                                if (modelData.id.indexOf("pause") !== -1) return "pause";
                                if (modelData.id.indexOf("step") !== -1) return "step";
                                if (modelData.id.indexOf("undo") !== -1) return "undo";
                                if (modelData.id.indexOf("redo") !== -1) return "redo";
                                if (modelData.id.indexOf("save") !== -1) return "save";
                                if (modelData.id.indexOf("open") !== -1) return "open";
                                if (modelData.id.indexOf("new") !== -1) return "new";
                                if (modelData.id.indexOf("line") !== -1) return "editor2d";
                                if (modelData.id.indexOf("circle") !== -1) return "editor2d";
                                if (modelData.id.indexOf("rect") !== -1) return "editor2d";
                                if (modelData.id.indexOf("extrude") !== -1) return "editor3d";
                                if (modelData.id.indexOf("revolve") !== -1) return "rotate";
                                if (modelData.id.indexOf("fillet") !== -1) return "editor2d";
                                if (modelData.id.indexOf("wire") !== -1) return "wire";
                                if (modelData.id.indexOf("resistor") !== -1) return "components";
                                if (modelData.id.indexOf("plot") !== -1) return "simulation";
                                return "settings";
                            }
                            size: modelData.isLarge ? 18 : 14
                            muted: !btn.hovered
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Text {
                            visible: modelData.isLarge
                            text: modelData.label
                            font.pixelSize: 10
                            color: "#aaaaaa"
                            Layout.alignment: Qt.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }

                    background: Rectangle {
                        color: btn.down ? Theme.bgActive : (btn.hovered ? Theme.bgHover : "transparent")
                        border.color: btn.hovered ? Theme.borderStrong : "transparent"
                        radius: 3
                    }

                    onClicked: {
                        root.actionTriggered(modelData.id);
                        if (typeof ribbonController !== "undefined") {
                            ribbonController.executeCommand(modelData.id);
                        }
                    }
                }
            }
        }

        // Group Title Label
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 14
            color: "#1e1e1e"
            radius: 2

            Text {
                text: root.title
                anchors.centerIn: parent
                font.pixelSize: 9
                font.weight: Font.DemiBold
                color: "#777777"
            }
        }
    }
}
