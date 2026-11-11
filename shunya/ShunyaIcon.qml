import QtQuick

Item {
    id: root
    property string name: "info"
    property int size: 16
    property color color: Theme.textPrimary
    property bool muted: false
    property bool inverse: false

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    readonly property string effectiveHex: {
        if (inverse) return Theme.iconHexInverse;
        if (muted) return Theme.iconHexMuted;
        // Convert Qt color to 6-char RGB uppercase hex
        var r = Math.round(color.r * 255).toString(16).padStart(2, "0");
        var g = Math.round(color.g * 255).toString(16).padStart(2, "0");
        var b = Math.round(color.b * 255).toString(16).padStart(2, "0");
        return (r + g + b).toUpperCase();
    }

    Image {
        anchors.fill: parent
        asynchronous: false
        source: "image://icons/" + root.name + "/" + root.effectiveHex
        sourceSize.width: root.size * 2
        sourceSize.height: root.size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        opacity: root.enabled ? 1.0 : 0.4
    }
}
