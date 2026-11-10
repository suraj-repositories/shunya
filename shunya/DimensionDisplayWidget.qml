import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    height: 26
    width: dofRow.width + 16
    radius: 4
    color: "#1e2227ee"
    border.color: statusColor

    property int remainingDof: 0
    property string statusMode: "FullyConstrained" // "UnderConstrained", "FullyConstrained", "OverConstrainedConflict"
    property double solveTimeMs: 0.12

    readonly property color statusColor: {
        if (statusMode === "OverConstrainedConflict") return "#e06c75";
        if (statusMode === "FullyConstrained") return "#98c379";
        return "#e5c07b";
    }

    RowLayout {
        id: dofRow
        anchors.centerIn: parent
        spacing: 8

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: root.statusColor
        }

        Text {
            text: {
                if (root.statusMode === "OverConstrainedConflict") return "CONFLICT (Over-Constrained)";
                if (root.statusMode === "FullyConstrained") return "DOF: 0 (Fully Constrained)";
                return "DOF: " + root.remainingDof + " (Under-Constrained)";
            }
            color: root.statusColor
            font.pixelSize: 10
            font.bold: true
            font.family: "Monospace"
        }

        Text {
            text: "| " + root.solveTimeMs.toFixed(2) + " ms"
            color: "#8b949e"
            font.pixelSize: 9
            font.family: "Monospace"
        }
    }
}
