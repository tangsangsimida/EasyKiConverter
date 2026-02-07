import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    // 接收 ComponentListItemData 对象
    property var itemData
    property string searchText: "" 
    
    signal deleteClicked()
    signal copyClicked() // 新增复制信号

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

    // 复制辅助组件
    TextEdit {
        id: copyHelper
        visible: false
        text: ""
    }

    // 复制成功提示
    ToolTip {
        id: copyFeedback
        parent: item
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        text: qsTr("已复制 ID")
        delay: 0
        timeout: 1500
    }

    // 1. 将全局鼠标区域移到最底层（作为背景交互层）
    // 这样它就不会遮挡上层的缩略图交互
    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor 
        acceptedButtons: Qt.RightButton // 只响应右键，左键穿透（如果有需要）或保留默认
        
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                if (itemData && itemData.componentId) {
                    copyHelper.text = itemData.componentId
                    copyHelper.selectAll()
                    copyHelper.copy()
                    
                    item.copyClicked()
                    copyFeedback.visible = true
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.sm
        spacing: AppStyle.spacing.md
        z: 10 // 确保内容在背景 MouseArea 之上

        // 缩略图区域
        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            color: "white" 
            radius: AppStyle.radius.sm
            border.color: AppStyle.colors.border
            border.width: 1
            clip: true // 裁剪内部图片圆角

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
                running: (itemData && itemData.isFetching) ? true : false
                visible: (itemData && itemData.isFetching) ? true : false
            }

            // 占位符/错误状态
            Text {
                anchors.centerIn: parent
                text: (itemData && !itemData.isValid) ? "✕" : (itemData && !itemData.isFetching && !itemData.hasThumbnail) ? "?" : ""
                font.pixelSize: AppStyle.fontSizes.xxl
                color: (itemData && !itemData.isValid) ? AppStyle.colors.danger : AppStyle.colors.textSecondary
                visible: !thumbnail.visible && !(itemData && itemData.isFetching)
            }

            // 2. 缩略图交互区域
            MouseArea {
                id: thumbMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: (itemData && itemData.hasThumbnail) ? Qt.PointingHandCursor : Qt.ArrowCursor
                // 这里的 hover 事件现在可以正常触发了，因为 itemMouseArea 在底层
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

    // 3. 悬浮预览层 (改回 Popup 以确保显示在最顶层且不被裁剪)
    Popup {
        id: previewOverlay
        // 绑定可见性：鼠标悬停 && 有数据 && 有缩略图
        visible: thumbMouseArea.containsMouse && itemData && itemData.hasThumbnail
        
        // 相对坐标：显示在缩略图右侧
        x: 65 
        y: (parent.height - height) / 2
        
        width: 200
        height: 200
        padding: 0
        
        // 关键配置：确保它是被动的，完全由 visible 控制
        closePolicy: Popup.NoAutoClose
        modal: false
        focus: false
        dim: false // 不变暗背景

        background: Rectangle {
            color: "white"
            border.color: AppStyle.colors.primary
            border.width: 2
            radius: AppStyle.radius.md
            
            // 阴影效果
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 1.0
                shadowColor: "#33000000"
                shadowVerticalOffset: 2
                shadowHorizontalOffset: 2
            }
        }
        
        contentItem: Item {
            Image {
                anchors.fill: parent
                anchors.margins: 4
                source: (itemData && itemData.thumbnailBase64) ? "data:image/png;base64," + itemData.thumbnailBase64 : ""
                fillMode: Image.PreserveAspectFit
            }
            
            // 底部文字遮罩
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: 30
                color: "#CC000000"
                radius: AppStyle.radius.md
                
                Text {
                    anchors.centerIn: parent
                    text: itemData ? itemData.componentId : ""
                    color: "white"
                    font.bold: true
                }
            }
        }
    }
}