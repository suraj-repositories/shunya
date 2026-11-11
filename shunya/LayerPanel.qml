import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    width: 240
    color: "#161b22"
    border.color: "#30363d"
    border.width: 1
    radius: 4
    clip: true

    property var controller: typeof layerController !== "undefined" ? layerController : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "LAYERS (" + (root.controller ? root.controller.layerCount : 0) + ")"
                color: Theme.textPrimary
                font.pixelSize: 10
                font.bold: true
                Layout.fillWidth: true
            }
            Rectangle {
                width: 44
                height: 20
                radius: 3
                color: "#282c34"
                border.color: "#3e4451"
                Text {
                    anchors.centerIn: parent
                    text: "+ Layer"
                    color: "#98c379"
                    font.pixelSize: 9
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.controller) {
                            var idx = root.controller.layerCount + 1;
                            root.controller.addLayer("Layer_" + idx, "0", "#c678dd", 0.25, "Solid");
                        }
                    }
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 3
            model: root.controller ? root.controller.layerModel : null

            delegate: Rectangle {
                width: ListView.view.width
                height: 28
                radius: 3
                color: root.controller && root.controller.activeLayer === model.layerName ? Theme.bgActive : Theme.bgCanvas
                border.color: root.controller && root.controller.activeLayer === model.layerName ? Theme.borderStrong : Theme.borderSubtle

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 6

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 2
                        color: model.colorHex
                    }

                    Text {
                        text: model.layerName + (model.parentLayer !== "" ? " (" + model.parentLayer + ")" : "")
                        color: model.effectiveVisible ? "#abb2bf" : "#5c6370"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        Layout.fillWidth: true

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (root.controller) {
                                    root.controller.activeLayer = model.layerName;
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: 24
                        height: 18
                        radius: 2
                        color: model.effectiveVisible ? "#2c313c" : "#161b22"
                        Text {
                            anchors.centerIn: parent
                            text: model.effectiveVisible ? "VIS" : "HID"
                            color: model.effectiveVisible ? "#98c379" : "#e06c75"
                            font.pixelSize: 8
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.controller) {
                                    root.controller.toggleLayerVisible(model.layerName);
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: 24
                        height: 18
                        radius: 2
                        color: model.effectiveLocked ? "#3a2426" : "#2c313c"
                        Text {
                            anchors.centerIn: parent
                            text: model.effectiveLocked ? "LCK" : "EDT"
                            color: model.effectiveLocked ? "#e06c75" : "#abb2bf"
                            font.pixelSize: 8
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.controller) {
                                    root.controller.toggleLayerLocked(model.layerName);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
