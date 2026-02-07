import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    // 接收 ComponentListItemData 对象
    property var itemData
    property string searchText: "" 
    
    signal deleteClicked()

    height: 64 // 增加高度以容纳缩略图和更多信息
    
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
        anchors.margins: AppStyle.spacing.sm
        spacing: AppStyle.spacing.md

        // 缩略图区域
        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            color: "white" // 缩略图背景通常为白色
            radius: AppStyle.radius.sm
            border.color: AppStyle.colors.border
            border.width: 1
            clip: true

            Image {
                id: thumbnail
                anchors.centerIn: parent
                width: 46
                height: 46
                source: (itemData && itemData.thumbnailBase64) ? "data:image/png;base64," + itemData.thumbnailBase64 : ""
                fillMode: Image.PreserveAspectFit
                visible: itemData && itemData.hasThumbnail && !itemData.isFetching
                cache: false
            }

            // 加载状态
            BusyIndicator {
                anchors.centerIn: parent
                width: 24
                height: 24
                running: itemData ? itemData.isFetching : false
                visible: itemData ? itemData.isFetching : false
            }

            // 占位符/错误状态
            Text {
                anchors.centerIn: parent
                text: (itemData && !itemData.isValid) ? "✕" : (itemData && !itemData.isFetching && !itemData.hasThumbnail) ? "?" : ""
                font.pixelSize: AppStyle.fontSizes.xxl
                color: (itemData && !itemData.isValid) ? AppStyle.colors.danger : AppStyle.colors.textSecondary
                visible: !thumbnail.visible && !itemData.isFetching
            }
        }

        // 元器件信息
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2
            Layout.alignment: Qt.AlignVCenter

            // 元件ID
            Text {
                Layout.fillWidth: true
                
                // 使用富文本以支持高亮
                textFormat: Text.RichText
                text: {
                    if (!itemData) return ""
                    var cid = itemData.componentId
                    
                    if (!searchText || searchText.trim() === "") return cid
                    
                    // 转义特殊字符防止正则错误或HTML注入
                    var escapedSearch = searchText.trim().replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
                    var regex = new RegExp("(" + escapedSearch + ")", "gi") // 全局 + 忽略大小写
                    
                    // 高亮颜色使用 Primary 颜色 (#3b82f6)
                    return cid.replace(regex, "<font color='#3b82f6'><b>$1</b></font>")
                }
                
                font.pixelSize: AppStyle.fontSizes.md
                font.family: "Courier New"
                font.bold: true
                color: (itemData && !itemData.isValid) ? AppStyle.colors.danger : AppStyle.colors.textPrimary
                elide: Text.ElideRight
            }

            // 名称/封装/状态信息
            Text {
                Layout.fillWidth: true
                text: {
                    if (!itemData) return ""
                    if (itemData.isFetching) return "正在验证..."
                    if (!itemData.isValid) return itemData.errorMessage || "无效的元器件"
                    
                    var info = []
                    if (itemData.name) info.push(itemData.name)
                    if (itemData.package) info.push(itemData.package)
                    
                    return info.join(" | ")
                }
                font.pixelSize: AppStyle.fontSizes.sm
                color: (itemData && !itemData.isValid) ? AppStyle.colors.danger : AppStyle.colors.textSecondary
                elide: Text.ElideRight
            }
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
        
        // 简单的放大预览逻辑
        onContainsMouseChanged: {
            if (containsMouse && itemData && itemData.hasThumbnail) {
                // 显示放大预览
                previewPopup.open()
            } else {
                previewPopup.close()
            }
        }
    }
    
    // 放大预览弹窗
    Popup {
        id: previewPopup
        x: itemData && itemData.hasThumbnail ? 60 : 0 // 显示在右侧
        y: -50
        width: 200
        height: 200
        padding: 0
        visible: false
        closePolicy: Popup.NoAutoClose
        
        background: Rectangle {
            color: "white"
            border.color: AppStyle.colors.primary
            border.width: 2
            radius: AppStyle.radius.md
            
            // 阴影效果需要 QtQuick.Effects 或自定义 Shader，这里简化
        }
        
        Image {
            anchors.fill: parent
            anchors.margins: 4
            source: (itemData && itemData.thumbnailBase64) ? "data:image/png;base64," + itemData.thumbnailBase64 : ""
            fillMode: Image.PreserveAspectFit
        }
        
        // 元件名称覆盖
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 30
            color: "#CC000000" // 半透明黑
            radius: AppStyle.radius.md
            // 只设置下半部分的圆角有点麻烦，这里简化
            
            Text {
                anchors.centerIn: parent
                text: itemData ? itemData.componentId : ""
                color: "white"
                font.bold: true
            }
        }
    }
}
