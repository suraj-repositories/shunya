import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property int atomicNumber: 1
    property string symbol: "H"
    property string elementName: "Hydrogen"
    property double atomicMass: 1.008
    property string cellColor: "#6d28d9"
    property string valueLabel: "1.008"
    property bool isSelected: false
    property bool isHighlighted: true

    signal cellClicked(int atomicNumber)

    radius: Theme.radiusSm
    color: cellColor
    opacity: isHighlighted ? 1.0 : 0.25
    border.width: isSelected ? 2 : (ma.containsMouse ? 2 : 1)
    border.color: isSelected ? Theme.borderFocus : (ma.containsMouse ? Theme.borderStrong : Theme.borderSubtle)

    Column {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 1

        Text {
            text: root.atomicNumber.toString()
            color: Theme.textSecondary
            font.family: Theme.fontMono
            font.pixelSize: 9
        }
        Text {
            text: root.symbol
            color: Theme.textPrimary
            font.family: Theme.fontSans
            font.pixelSize: 15
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: root.elementName
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: 8
            elide: Text.ElideRight
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.cellClicked(root.atomicNumber)
    }

    ToolTip.visible: ma.containsMouse
    ToolTip.text: root.symbol + " (" + root.elementName + ", Z=" + root.atomicNumber + ") • " + root.valueLabel + (root.isHighlighted ? "" : " [Dimmed: Does not match active filter/phase]")
}
