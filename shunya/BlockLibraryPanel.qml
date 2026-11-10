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
                text: "BLOCKS (" + (root.controller ? root.controller.blockCount : 0) + " / " +
                      (root.controller ? root.controller.instanceCount : 0) + " inst)"
                color: "#e5c07b"
                font.pixelSize: 10
                font.bold: true
                Layout.fillWidth: true
            }
            Rectangle {
                width: 46
                height: 20
                radius: 3
                color: "#282c34"
                border.color: "#3e4451"
                Text {
                    anchors.centerIn: parent
                    text: "+ Block"
                    color: "#98c379"
                    font.pixelSize: 9
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.controller) {
                            var idx = root.controller.blockCount + 1;
                            root.controller.createRectBlock("Block_" + idx, 30.0, 20.0);
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
            model: root.controller ? root.controller.blockModel : null

            delegate: Rectangle {
                width: ListView.view.width
                height: 30
                radius: 3
                color: "#1e2227"
                border.color: "#2c313c"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 6

                    Text {
                        text: model.blockName + " [" + model.itemCount + " items, " + model.instanceCount + "x]"
                        color: "#abb2bf"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        width: 40
                        height: 18
                        radius: 2
                        color: "#2c3e50"
                        border.color: "#61afef"
                        Text {
                            anchors.centerIn: parent
                            text: "Place"
                            color: "#ffffff"
                            font.pixelSize: 8
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.controller) {
                                    root.controller.placeBlockInstance(model.blockName, 0.0, 0.0, 0.0, 1.0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
