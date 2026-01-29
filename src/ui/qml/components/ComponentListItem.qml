import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0
Rectangle {
    id: item
    property string componentId
    property string searchText: "" // 新增：搜索关键词
    signal deleteClicked()
    height: 48
    // 悬停效果
    color: itemMouseArea.containsMouse ? AppStyle.colors.background : AppStyle.colors.surface
    radius: AppStyle.radius.md
    border.color: AppStyle.colors.border
    border.width: 1
    Behavior on color {
        ColorAnimation {
            duration: AppStyle.durations.fast
            easing.type: AppStyle.easings.easeOut
        }
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: AppStyle.spacing.md
        anchors.rightMargin: AppStyle.spacing.md
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppStyle.spacing.sm
        // 元件ID
        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 使用富文本以支持高亮
            textFormat: Text.RichText
            text: {
                if (!searchText || searchText.trim() === "") return componentId
                
                // 转义特殊字符防止正则错误或HTML注入
                var escapedSearch = searchText.trim().replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
                var regex = new RegExp("(" + escapedSearch + ")", "gi") // 全局 + 忽略大小写
                
                // 高亮颜色使用 Primary 颜色 (#3b82f6)
                return componentId.replace(regex, "<font color='#3b82f6'><b>$1</b></font>")
            }
            
            font.pixelSize: AppStyle.fontSizes.sm
            font.family: "Courier New"
            color: AppStyle.colors.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        // 删除按钮
        Button {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            background: Rectangle {
                color: parent.pressed ? AppStyle.colors.dangerLight :
                       parent.hovered ? AppStyle.colors.dangerLight : "transparent"
                radius: AppStyle.radius.sm
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            contentItem: Text {
                text: "×"
                font.pixelSize: AppStyle.fontSizes.xxl
                font.bold: true
                color: parent.pressed ? AppStyle.colors.dangerDark :
                       parent.hovered ? AppStyle.colors.danger : AppStyle.colors.danger
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            onClicked: {
                item.deleteClicked()
            }
        }
    }
    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton
    }
}
