import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    property bool checked: false
    property string text: ""
    property string variant: "checkbox" // "checkbox" | "switch"
    signal toggled(bool checked)

    implicitHeight: 32
    implicitWidth: row.implicitWidth + 8

    focus: true
    Keys.onSpacePressed: (event) => {
        root.checked = !root.checked;
        root.toggled(root.checked);
        event.accepted = true;
    }

    RowLayout {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        // Checkbox or Switch Indicator
        Rectangle {
            id: indicatorBox
            Layout.preferredWidth: root.variant === "switch" ? 34 : 18
            Layout.preferredHeight: root.variant === "switch" ? 20 : 18
            radius: root.variant === "switch" ? 10 : Theme.radiusSm
            color: root.variant === "switch"
                ? (root.checked ? Theme.btnPrimaryBg : Theme.bgInput)
                : (root.checked ? Theme.bgActive : Theme.bgInput)
            border.color: root.activeFocus
                ? Theme.borderFocus
                : (root.checked ? Theme.borderStrong : (checkMa.containsMouse ? Theme.borderHover : Theme.borderDefault))
            border.width: root.activeFocus ? 1.5 : 1

            // Checkmark for checkbox variant
            Text {
                visible: root.variant === "checkbox" && root.checked
                anchors.centerIn: parent
                text: "✓"
                font.family: Theme.fontSans
                font.pixelSize: 12
                font.bold: true
                color: Theme.textPrimary
            }

            // Sliding pill thumb for switch variant
            Rectangle {
                visible: root.variant === "switch"
                width: 14; height: 14; radius: 7
                color: root.checked ? Theme.textInverse : Theme.textMuted
                anchors.verticalCenter: parent.verticalCenter
                x: root.checked ? (indicatorBox.width - width - 3) : 3
                Behavior on x {
                    NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
                }
            }
        }

        Text {
            visible: root.text.length > 0
            text: root.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSm
            color: root.checked ? Theme.textPrimary : Theme.textSecondary
            verticalAlignment: Text.AlignVCenter
        }
    }

    MouseArea {
        id: checkMa
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus();
            root.checked = !root.checked;
            root.toggled(root.checked);
        }
    }
}
