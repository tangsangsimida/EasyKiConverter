import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

Item {
    id: root
    property string label: ""
    property string value: ""
    property color valueColor: AppStyle.colors.textPrimary
    implicitWidth: columnLayout.implicitWidth
    implicitHeight: columnLayout.implicitHeight
    ColumnLayout {
        id: columnLayout
        anchors.fill: parent
        spacing: 4
        Text {
            text: label
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            Layout.fillWidth: true
        }

        Text {
            text: value
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            color: root.valueColor
            Layout.fillWidth: true
        }
    }
}
