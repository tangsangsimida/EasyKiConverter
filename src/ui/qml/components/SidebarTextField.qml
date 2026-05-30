import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

ColumnLayout {
    property string label: ""
    property string text: ""
    property string placeholder: ""
    property string browseIcon: ""
    signal browseClicked
    signal textEdited(string text)
    Layout.fillWidth: true
    spacing: 2
    Text {
        text: label
        font.pixelSize: AppStyle.fontSizes.xs
        color: AppStyle.colors.textSecondary
    }

    RowLayout {
        spacing: AppStyle.spacing.xs
        TextField {
            id: tf
            Layout.fillWidth: true
            text: parent.parent.text
            placeholderText: parent.parent.placeholder
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textPrimary
            onTextChanged: parent.parent.textEdited(text)
            background: Rectangle {
                color: AppStyle.colors.surface
                border.color: tf.focus ? AppStyle.colors.primary : AppStyle.colors.border
                radius: AppStyle.radius.sm
            }
        }
        ModernButton {
            visible: browseIcon !== ""
            iconName: browseIcon
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            backgroundColor: AppStyle.colors.border
            onClicked: parent.parent.browseClicked()
        }
    }
}
