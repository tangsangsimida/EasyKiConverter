import QtQuick
import QtQuick.Layouts
import "../styles"

ColumnLayout {
    property string label: ""
    property var model: []
    property int currentIndex: 0
    signal indexChanged(int index)
    Layout.fillWidth: true
    spacing: 2
    Text {
        visible: label !== ""
        text: label
        font.pixelSize: AppStyle.fontSizes.xs
        color: AppStyle.colors.textSecondary
    }

    Rectangle {
        Layout.fillWidth: true
        height: 30
        color: AppStyle.colors.border
        opacity: 0.5
        radius: AppStyle.radius.sm
        Row {
            anchors.fill: parent
            anchors.margins: 2
            Repeater {
                model: parent.parent.model
                Rectangle {
                    width: parent.width / parent.parent.model.length
                    height: parent.height
                    radius: AppStyle.radius.xs - 1
                    color: index === parent.parent.parent.currentIndex ? AppStyle.colors.surface : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: index === parent.parent.parent.parent.currentIndex ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: parent.parent.parent.parent.indexChanged(index)
                    }
                }
            }
        }
    }
}
