import QtQuick

Rectangle {
    id: root
    property string text: ""
    property string iconName: ""
    property int iconSize: 14
    property string variant: "secondary" // "primary", "secondary", "ghost", "danger"
    property bool primary: false
    property bool active: false
    property bool compact: false
    property string tooltipText: ""
    property string shortcutText: ""
    signal clicked()

    implicitWidth: row.implicitWidth + (compact ? 14 : 20)
    implicitHeight: compact ? 28 : 32
    radius: Theme.radiusSm
    clip: true
    opacity: enabled ? 1.0 : 0.45

    color: {
        if (variant === "primary" || primary || active) {
            return ma.pressed ? Theme.btnPrimaryHover : (ma.containsMouse ? Theme.btnPrimaryHover : Theme.btnPrimaryBg);
        }
        if (variant === "ghost") {
            return ma.containsMouse ? Theme.bgHover : "transparent";
        }
        if (variant === "danger") {
            return ma.containsMouse ? Theme.bgHover : Theme.bgSurface;
        }
        return ma.pressed ? Theme.bgActive : (ma.containsMouse ? Theme.btnSecHover : Theme.bgElevated);
    }

    border.width: variant === "ghost" ? 0 : 1
    border.color: {
        if (variant === "primary" || primary || active) return Theme.btnPrimaryBg;
        if (variant === "danger") return Theme.statusError;
        return ma.containsMouse ? Theme.borderStrong : Theme.btnSecBorder;
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        ShunyaIcon {
            visible: root.iconName !== ""
            name: root.iconName
            size: root.iconSize
            inverse: (root.variant === "primary" || root.primary || root.active)
            color: root.variant === "danger" ? Theme.statusError : Theme.textPrimary
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            visible: root.text !== ""
            text: root.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSm
            font.weight: (root.variant === "primary" || root.primary || root.active) ? Font.DemiBold : Font.Medium
            color: (root.variant === "primary" || root.primary || root.active)
                ? Theme.btnPrimaryFg
                : (root.variant === "danger" ? Theme.statusError : Theme.textPrimary)
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.enabled) root.clicked()
    }

    // Informative Hover Tooltip with Shortcut Badge Reminder
    Rectangle {
        id: tooltipBubble
        visible: ma.containsMouse && (root.tooltipText !== "" || root.shortcutText !== "")
        z: 99999
        anchors.bottom: parent.top
        anchors.bottomMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: tooltipRow.implicitWidth + 14
        implicitHeight: 22
        radius: Theme.radiusSm
        color: Theme.bgElevated
        border.color: Theme.borderDefault
        border.width: 1

        Row {
            id: tooltipRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: root.tooltipText
                font.family: Theme.fontSans
                font.pixelSize: 11
                color: Theme.textPrimary
                visible: root.tooltipText !== ""
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                visible: root.shortcutText !== ""
                height: 16
                width: scTxt.contentWidth + 6
                radius: 2
                color: Theme.bgSurface
                border.color: Theme.borderSubtle
                border.width: 1
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    id: scTxt
                    anchors.centerIn: parent
                    text: root.shortcutText
                    font.family: Theme.fontMono
                    font.pixelSize: 9
                    color: Theme.textMuted
                }
            }
        }
    }
}
