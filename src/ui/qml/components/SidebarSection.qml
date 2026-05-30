import QtQuick
import QtQuick.Layouts
import "../styles"

ColumnLayout {
    property string title: ""
    Layout.fillWidth: true
    spacing: AppStyle.spacing.sm
    Text {
        text: title
        font.pixelSize: AppStyle.fontSizes.xs
        font.bold: true
        color: AppStyle.colors.primary
        opacity: 0.8
        Layout.leftMargin: AppStyle.spacing.xs
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: AppStyle.colors.border
        opacity: 0.3
    }
}
