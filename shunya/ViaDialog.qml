import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Via Properties"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel

    property string netName: "SIG"
    property double drillMm: 0.30
    property double padDiameterMm: 0.60
    property string viaType: "ThroughHole"

    GridLayout {
        columns: 2
        rowSpacing: 8
        columnSpacing: 12

        Label { text: "Net:" }
        TextField { text: root.netName; onTextChanged: root.netName = text }

        Label { text: "Via Type:" }
        ComboBox {
            model: ["ThroughHole", "Blind", "Buried"]
            onActivated: root.viaType = currentText
        }

        Label { text: "Drill (mm):" }
        TextField { text: root.drillMm.toFixed(2) }

        Label { text: "Pad Diameter (mm):" }
        TextField { text: root.padDiameterMm.toFixed(2) }
    }
}
