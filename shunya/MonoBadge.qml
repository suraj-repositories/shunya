import QtQuick

Rectangle {
    id: root
    property string text: ""
    property string tone: "neutral" // "neutral", "success", "warning", "error"

    implicitWidth: label.implicitWidth + 14
    implicitHeight: 20
    radius: 10
    color: {
        if (tone === "success") return Qt.rgba(Theme.statusSuccess.r, Theme.statusSuccess.g, Theme.statusSuccess.b, 0.14);
        if (tone === "warning") return Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.14);
        if (tone === "error")   return Qt.rgba(Theme.statusError.r, Theme.statusError.g, Theme.statusError.b, 0.14);
        return Theme.bgElevated;
    }
    border.width: 1
    border.color: {
        if (tone === "success") return Theme.statusSuccess;
        if (tone === "warning") return Theme.statusWarning;
        if (tone === "error")   return Theme.statusError;
        return Theme.borderDefault;
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.family: Theme.fontSans
        font.pixelSize: Theme.fontXs
        font.weight: Font.Medium
        color: {
            if (root.tone === "success") return Theme.statusSuccess;
            if (root.tone === "warning") return Theme.statusWarning;
            if (root.tone === "error")   return Theme.statusError;
            return Theme.textSecondary;
        }
    }
}
