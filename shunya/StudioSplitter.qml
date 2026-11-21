import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root
    property int orientation: Qt.Vertical // Qt.Vertical (vertical bar, drags horizontally) | Qt.Horizontal (horizontal bar, drags vertically)
    property real defaultSize: 184
    property real minSize: 56
    property real maxSize: 500
    property bool animated: false
    property alias isDragging: dragMa.isDragging

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
        color: dragMa.isDragging
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
            color: dragMa.containsMouse || dragMa.isDragging ? Theme.textPrimary : Theme.textSubtle
            opacity: dragMa.containsMouse || dragMa.isDragging ? 1.0 : 0.4
        }
    }

    MouseArea {
        id: dragMa
        anchors.fill: parent
        anchors.margins: -2 // Slightly wider hit area for easy grabbing
        hoverEnabled: true
        cursorShape: root.orientation === Qt.Vertical ? Qt.SplitHCursor : Qt.SplitVCursor

        property real lastScenePos: 0
        property bool isDragging: false

        onPressed: (mouse) => {
            root.forceActiveFocus();
            var scenePt = root.mapToItem(null, mouse.x, mouse.y);
            lastScenePos = (root.orientation === Qt.Vertical) ? scenePt.x : scenePt.y;
            isDragging = true;
        }

        onPositionChanged: (mouse) => {
            if (pressed && isDragging) {
                var scenePt = root.mapToItem(null, mouse.x, mouse.y);
                var currentScenePos = (root.orientation === Qt.Vertical) ? scenePt.x : scenePt.y;
                var delta = currentScenePos - lastScenePos;
                if (Math.abs(delta) >= 1.0) {
                    lastScenePos = currentScenePos;
                    root.dragDelta(delta);
                }
            }
        }

        onReleased: {
            isDragging = false;
        }

        onCanceled: {
            isDragging = false;
        }

        onDoubleClicked: {
            root.resetRequested();
        }
    }
}
