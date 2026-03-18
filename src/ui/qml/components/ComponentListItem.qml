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
    signal deleteClicked
    signal copyClicked
    signal retryClicked
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
        acceptedButtons: Qt.RightButton | Qt.LeftButton // 同时响应右键和左键
        onClicked: mouse => {
            // 右键点击复制 ID
            if (mouse.button === Qt.RightButton) {
                if (itemData && itemData.componentId) {
                    copyHelper.text = itemData.componentId;
                    copyHelper.selectAll();
                    copyHelper.copy();
                    item.copyClicked();
                    copyFeedback.visible = true;
                }
            } else
            // Ctrl + 左键点击打开浏览器
            if (mouse.button === Qt.LeftButton && (mouse.modifiers & Qt.ControlModifier)) {
                if (itemData && itemData.componentId) {
                    var url = "https://so.szlcsc.com/global.html?k=" + itemData.componentId;
                    Qt.openUrlExternally(url);
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.sm
        spacing: AppStyle.spacing.md
        // 预览图区域 - 支持悬停放大
        Item {
            id: previewArea
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            // 默认显示的单张缩略图
            Rectangle {
                id: defaultThumbnail
                width: 48
                height: 48
                color: "white"
                radius: AppStyle.radius.sm
                border.color: AppStyle.colors.border
                border.width: 1
                clip: true
                Image {
                    anchors.centerIn: parent
                    width: 46
                    height: 46
                    source: (itemData && itemData.thumbnailBase64) ? "data:image/png;base64," + itemData.thumbnailBase64 : ""
                    fillMode: Image.PreserveAspectFit
                    cache: true
                    asynchronous: true
                    visible: itemData && itemData.hasThumbnail && !itemData.isFetching
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
                    visible: !(itemData && itemData.hasThumbnail && !itemData.isFetching) && !(itemData && itemData.isFetching)
                }
            }

            // 悬停时显示的放大预览图 - 显示三张图片在一排
            Popup {
                id: previewPopup
                parent: defaultThumbnail
                width: 490 // 三张图片 150x3 + 间距
                height: 170
                padding: 0
                visible: previewMouseArea.containsMouse && itemData && itemData.hasThumbnail
                closePolicy: Popup.NoAutoClose
                modal: false
                focus: false
                dim: false
                // 智能位置计算：检测是否超出窗口边界
                onVisibleChanged: {
                    if (visible) {
                        // 获取缩略图在屏幕上的位置
                        var thumbGlobalPos = defaultThumbnail.mapToGlobal(Qt.point(0, 0));
                        // 获取窗口在屏幕上的位置
                        var window = item.Window.window;
                        var windowGlobalX = window ? window.x : 0;
                        var windowWidth = window ? window.width : item.width;
                        // 计算缩略图相对于窗口的位置
                        var thumbRelativeX = thumbGlobalPos.x - windowGlobalX;
                        // 预览图宽度（490）+ 间距（60）= 550
                        var popupWidth = width + 60;
                        // 如果右侧空间不足，显示在左侧
                        if (thumbRelativeX + popupWidth > windowWidth) {
                            x = -540; // 显示在缩略图左侧
                        } else {
                            x = 60; // 显示在缩略图右侧
                        }

                        // 垂直居中显示
                        y = (defaultThumbnail.height - height) / 2;
                    }
                }

                enter: Transition {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 150
                    }
                }

                exit: Transition {
                    NumberAnimation {
                        property: "opacity"
                        from: 1
                        to: 0
                        duration: 150
                    }
                }

                background: Rectangle {
                    color: AppStyle.colors.surface
                    border.color: AppStyle.colors.primary
                    border.width: 2
                    radius: AppStyle.radius.md
                    // 阴影效果
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowBlur: 1.0
                        shadowColor: AppStyle.isDarkMode ? "#00000000" : "#33000000"
                        shadowVerticalOffset: 2
                        shadowHorizontalOffset: 2
                    }
                }

                contentItem: Row {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10
                    Repeater {
                        model: itemData ? itemData.previewImageCount : 0
                        Rectangle {
                            width: 150
                            height: 150
                            color: AppStyle.colors.background
                            radius: AppStyle.radius.sm
                            border.color: AppStyle.colors.border
                            border.width: 1
                            clip: true
                            property bool imageLoaded: false
                            property bool loadTriggered: false
                            // 延迟加载触发器
                            Timer {
                                id: loadDelayTimer
                                interval: index * 100 // 每张图片延迟100ms加载
                                onTriggered: {
                                    parent.loadTriggered = true;
                                }
                            }

                            // 当 Popup 可见时开始延迟加载
                            Connections {
                                target: previewPopup
                                function onVisibleChanged() {
                                    if (previewPopup.visible && !loadTriggered) {
                                        loadDelayTimer.start();
                                    }
                                }
                            }

                            Image {
                                anchors.centerIn: parent
                                width: 148
                                height: 148
                                source: {
                                    if (!parent.loadTriggered)
                                        return "";
                                    if (!itemData || !itemData.previewImages || itemData.previewImages.length === 0)
                                        return "";
                                    return "data:image/png;base64," + itemData.previewImages[index];
                                }
                                fillMode: Image.PreserveAspectFit
                                cache: true
                                asynchronous: true
                                visible: parent.loadTriggered && parent.imageLoaded
                                onStatusChanged: {
                                    if (status === Image.Ready) {
                                        parent.imageLoaded = true;
                                    }
                                }
                            }

                            // 加载状态指示器
                            BusyIndicator {
                                anchors.centerIn: parent
                                width: 28
                                height: 28
                                running: parent.loadTriggered && !parent.imageLoaded
                                visible: parent.loadTriggered && !parent.imageLoaded
                            }

                            // 初始占位符
                            Rectangle {
                                anchors.centerIn: parent
                                width: 35
                                height: 35
                                color: AppStyle.colors.background
                                radius: AppStyle.radius.md
                                visible: !parent.loadTriggered
                                Text {
                                    anchors.centerIn: parent
                                    text: index + 1
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: AppStyle.colors.textSecondary
                                }
                            }

                            // 图片序号标记
                            Rectangle {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                width: 20
                                height: 20
                                color: AppStyle.colors.primary
                                radius: 10
                                Text {
                                    anchors.centerIn: parent
                                    text: index + 1
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: "white"
                                }
                            }

                            // 底部文字遮罩
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                height: 25
                                color: "#CC000000"
                                Text {
                                    anchors.centerIn: parent
                                    text: itemData ? itemData.componentId : ""
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 9
                                }
                            }
                        }
                    }
                }
            }

            // 预览区域交互
            MouseArea {
                id: previewMouseArea
                width: defaultThumbnail.width
                height: defaultThumbnail.height
                hoverEnabled: true
                cursorShape: (itemData && itemData.hasThumbnail) ? Qt.PointingHandCursor : Qt.ArrowCursor
                acceptedButtons: Qt.LeftButton
                onClicked: function (mouse) {
                    // Ctrl + 左键点击打开浏览器
                    if (mouse.modifiers & Qt.ControlModifier) {
                        if (itemData && itemData.componentId) {
                            var url = "https://so.szlcsc.com/global.html?k=" + itemData.componentId;
                            Qt.openUrlExternally(url);
                        }
                    }
                }
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
                id: componentIdText
                Layout.fillWidth: true
                // 使用富文本以支持高亮
                textFormat: Text.RichText
                text: {
                    if (!itemData)
                        return "";
                    var cid = itemData.componentId;
                    if (!searchText || searchText.trim() === "")
                        return cid;
                    // 转义特殊字符防止正则错误或HTML注入
                    var escapedSearch = searchText.trim().replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
                    var regex = new RegExp("(" + escapedSearch + ")", "gi"); // 全局 + 忽略大小写
                    // 高亮颜色使用 Primary 颜色 (#3b82f6)
                    return cid.replace(regex, "<font color='#3b82f6'><b>$1</b></font>");
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
                    if (!itemData)
                        return "";
                    if (itemData.isFetching)
                        return "正在验证...";
                    if (!itemData.isValid)
                        return itemData.errorMessage || "无效的元器件";
                    var info = [];
                    if (itemData.name)
                        info.push(itemData.name);
                    if (itemData.package)
                        info.push(itemData.package);
                    return info.join(" | ");
                }
                font.pixelSize: AppStyle.fontSizes.sm
                color: (itemData && !itemData.isValid) ? AppStyle.colors.danger : AppStyle.colors.textSecondary
                elide: Text.ElideRight
            }
        }
        // 重试按钮（仅对验证失败的元器件显示）
        Button {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            visible: itemData && !itemData.isValid && !itemData.isFetching
            background: Rectangle {
                color: parent.pressed ? AppStyle.colors.primaryHover : parent.hovered ? "#dbeafe" : "transparent"
                radius: AppStyle.radius.sm
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            contentItem: Text {
                text: "↻"
                font.pixelSize: AppStyle.fontSizes.xxl
                font.bold: true
                color: parent.pressed ? AppStyle.colors.primaryPressed : parent.hovered ? AppStyle.colors.primary : AppStyle.colors.primary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            onClicked: {
                item.retryClicked();
            }
        }
        // 删除按钮
        Button {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            background: Rectangle {
                color: parent.pressed ? AppStyle.colors.dangerLight : parent.hovered ? AppStyle.colors.dangerLight : "transparent"
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
                color: parent.pressed ? AppStyle.colors.dangerDark : parent.hovered ? AppStyle.colors.danger : AppStyle.colors.danger
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            onClicked: {
                item.deleteClicked();
            }
        }
    }
}
