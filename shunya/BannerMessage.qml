import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: 36
    radius: Theme.radiusSm
    visible: text !== ""

    property string text: ""
    property string tone: "info" // "info", "warning", "error", "success"
    signal dismissed()

    color: {
        if (tone === "error") return Theme.isDarkMode ? "#2D1214" : "#FEE2E2";
        if (tone === "warning") return Theme.isDarkMode ? "#2D200E" : "#FEF3C7";
        if (tone === "success") return Theme.isDarkMode ? "#0E2D18" : "#DCFCE7";
        return Theme.bgElevated;
    }

    border.width: 1
    border.color: {
        if (tone === "error") return Theme.statusError;
        if (tone === "warning") return Theme.statusWarning;
        if (tone === "success") return Theme.statusSuccess;
        return Theme.borderDefault;
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        spacing: 8

        ShunyaIcon {
            name: {
                if (root.tone === "error") return "error";
                if (root.tone === "warning") return "warning";
                if (root.tone === "success") return "check";
                return "info";
            }
            size: 14
            color: {
                if (root.tone === "error") return Theme.statusError;
                if (root.tone === "warning") return Theme.statusWarning;
                if (root.tone === "success") return Theme.statusSuccess;
                return Theme.textSecondary;
            }
        }

        Text {
            text: root.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSm
            color: Theme.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        MonoButton {
            iconName: "close"
            variant: "ghost"
            compact: true
            iconSize: 12
            onClicked: {
                root.text = "";
                root.dismissed();
            }
        }
    }
}
