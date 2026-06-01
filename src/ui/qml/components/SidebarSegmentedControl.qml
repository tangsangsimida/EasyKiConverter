import QtQuick
import QtQuick.Layouts
import "../styles"

ColumnLayout {
    id: segRoot
    property string label: ""
    property var model: []
    property int currentIndex: 0
    signal indexChanged(int index)
    Layout.fillWidth: true
    spacing: 2
    Text {
        visible: segRoot.label !== ""
        text: segRoot.label
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
            id: segRow
            anchors.fill: parent
            anchors.margins: 2
            Repeater {
                id: segRepeater
                model: segRoot.model
                Rectangle {
                    width: segRow.width / segRepeater.model.length
                    height: segRow.height
                    radius: AppStyle.radius.xs - 1
                    color: index === segRoot.currentIndex ? AppStyle.colors.surface : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: index === segRoot.currentIndex ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: segRoot.indexChanged(index)
                    }
                }
            }
        }
    }
}
