import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

RowLayout {
    property string label: ""
    property bool checked: false
    signal toggled(bool checked)
    Layout.fillWidth: true
    height: 32
    Text {
        text: label
        Layout.fillWidth: true
        font.pixelSize: AppStyle.fontSizes.sm
        color: AppStyle.colors.textPrimary
    }

    Switch {
        id: sw
        checked: parent.checked
        onToggled: parent.toggled(checked)
        indicator: Rectangle {
            implicitWidth: 36
            implicitHeight: 20
            x: sw.leftPadding
            y: parent.height / 2 - height / 2
            radius: 10
            color: sw.checked ? AppStyle.colors.primary : AppStyle.colors.border
            Rectangle {
                x: sw.checked ? parent.width - width - 2 : 2
                width: 16
                height: 16
                radius: 8
                color: "white"
                anchors.verticalCenter: parent.verticalCenter
                Behavior on x {
                    NumberAnimation {
                        duration: 150
                    }
                }
            }
        }
    }
}
