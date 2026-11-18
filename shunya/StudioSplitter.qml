import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root
    property int orientation: Qt.Vertical // Qt.Vertical (vertical bar, drags horizontally) | Qt.Horizontal (horizontal bar, drags vertically)
    property real defaultSize: 184
    property real minSize: 56
    property real maxSize: 500
    property bool animated: false

    signal dragDelta(real delta)
    signal resetRequested()

    implicitWidth: orientation === Qt.Vertical ? 5 : parent ? parent.width : 100
    implicitHeight: orientation === Qt.Horizontal ? 5 : parent ? parent.height : 100

    focus: true
    Keys.onPressed: (event) => {
        if (root.orientation === Qt.Vertical) {
            if (event.key === Qt.Key_Left) {
                root.dragDelta(-5);
                event.accepted = true;
            } else if (event.key === Qt.Key_Right) {
                root.dragDelta(5);
                event.accepted = true;
            }
        } else {
            if (event.key === Qt.Key_Up) {
                root.dragDelta(-5);
                event.accepted = true;
            } else if (event.key === Qt.Key_Down) {
                root.dragDelta(5);
                event.accepted = true;
            }
        }
    }

    Rectangle {
        id: visualBar
        anchors.fill: parent
        color: dragMa.drag.active
            ? Theme.borderFocus
            : (dragMa.containsMouse ? Theme.borderStrong : Theme.borderSubtle)

        Behavior on color {
            ColorAnimation { duration: 100 }
        }

        // Center grip accent line or dots
        Rectangle {
            id: gripAccent
            anchors.centerIn: parent
            width: root.orientation === Qt.Vertical ? 2 : 24
            height: root.orientation === Qt.Vertical ? 24 : 2
            radius: 1
            color: dragMa.containsMouse || dragMa.drag.active ? Theme.textPrimary : Theme.textSubtle
            opacity: dragMa.containsMouse || dragMa.drag.active ? 1.0 : 0.4
        }
    }

    MouseArea {
        id: dragMa
        anchors.fill: parent
        anchors.margins: -2 // Slightly wider hit area for easy grabbing
        hoverEnabled: true
        cursorShape: root.orientation === Qt.Vertical ? Qt.SplitHCursor : Qt.SplitVCursor

        property real startPos: 0

        onPressed: (mouse) => {
            root.forceActiveFocus();
            startPos = (root.orientation === Qt.Vertical) ? mouse.x : mouse.y;
        }

        onPositionChanged: (mouse) => {
            if (pressed) {
                var current = (root.orientation === Qt.Vertical) ? mouse.x : mouse.y;
                var delta = current - startPos;
                if (Math.abs(delta) >= 0.5) {
                    root.dragDelta(delta);
                }
            }
        }

        onDoubleClicked: {
            root.resetRequested();
        }
    }
}
