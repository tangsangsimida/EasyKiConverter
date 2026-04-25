import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Shapes
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: rootItem
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
    border.width: AppStyle.borderWidths.thin
    // 缓存搜索正则以优化性能
    property string cachedSearchText: ""
    property var cachedRegex: null
    Timer {
        id: regexUpdateTimer
        interval: 100
        onTriggered: updateCachedRegex()
    }
    function updateCachedRegex() {
        if (searchText !== cachedSearchText) {
            cachedSearchText = searchText;
            if (!searchText || searchText.trim() === "") {
                cachedRegex = null;
            } else {
                var escaped = searchText.trim().replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
                cachedRegex = new RegExp("(" + escaped + ")", "gi");
            }
        }
    }
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
        parent: rootItem
        x: parent.width - width - 8
        y: 4
        delay: 0
        timeout: 1500
        visible: false
        text: ""
        background: Rectangle {
            color: AppStyle.isDarkMode ? "#1e293b" : "#ffffff"
            radius: AppStyle.radius.md
            border.width: AppStyle.borderWidths.thin
            border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 8
                shadowColor: AppStyle.isDarkMode ? "#00000088" : "#33000000"
                shadowVerticalOffset: 2
            }
        }
        contentItem: Row {
            spacing: 4
            Text {
                text: "✓"
                color: AppStyle.colors.success
                font.bold: true
            }
            Text {
                text: qsTr("已复制")
                color: AppStyle.isDarkMode ? "#ffffff" : "#1e293b"
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
    }

    Timer {
        id: copyFeedbackTimer
        interval: 1500
        onTriggered: copyFeedback.visible = false
    }

    // 底层鼠标区域
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
                    rootItem.copyClicked();
                    copyFeedback.visible = true;
                    copyFeedbackTimer.start();
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
            // 悬停延迟
            Timer {
                id: hoverDelayTimer
                interval: 250
                repeat: false
                onTriggered: {
                    if (previewMouseArea.containsMouse) {
                        previewArea.showPreviewPopup();
                    }
                }
            }
            // 默认显示的单张缩略图
            Rectangle {
                id: previewBackground
                width: 48
                height: 48
                color: AppStyle.colors.textOnPrimary
                radius: AppStyle.radius.sm
                border.color: AppStyle.colors.border
                border.width: AppStyle.borderWidths.thin
                clip: true
                Image {
                    anchors.centerIn: parent
                    width: 46
                    height: 46
                    sourceSize: Qt.size(46, 46)  // 提示按实际显示大小解码
                    source: {
                        if (!itemData)
                            return "";
                        var phase = itemData.validationPhase || "idle";
                        if (phase === "idle" || phase === "validating" || phase === "failed")
                            return "";
                        if (!itemData.previewImageCount || itemData.previewImageCount === 0)
                            return "";
                        if (!itemData.previewImages || itemData.previewImages.length === 0)
                            return "";
                        return "data:image/jpeg;base64," + itemData.previewImages[0];
                    }
                    fillMode: Image.PreserveAspectFit
                    cache: true
                    asynchronous: true
                    visible: itemData && (itemData.validationPhase === "completed" || itemData.validationPhase === "fetching_preview") && itemData.previewImageCount > 0
                }

                // 加载状态（验证中或获取预览图中）
                BusyIndicator {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    running: (itemData && (itemData.validationPhase === "validating" || itemData.validationPhase === "fetching_preview")) ? true : false
                    visible: (itemData && (itemData.validationPhase === "validating" || itemData.validationPhase === "fetching_preview")) ? true : false
                }

                // 验证成功但无预览图 - 显示绿色圆圈+勾号
                Rectangle {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    radius: width / 2  // 声明式圆角
                    color: "transparent"
                    border.color: AppStyle.colors.success
                    border.width: AppStyle.borderWidths.thick
                    visible: (itemData && (itemData.validationPhase === "completed" || itemData.validationPhase === "fetching_preview") && (!itemData.previewImageCount || itemData.previewImageCount === 0))
                    Shape {
                        id: checkShape
                        anchors.fill: parent
                        ShapePath {
                            strokeColor: AppStyle.colors.success
                            strokeWidth: 2
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            joinStyle: ShapePath.RoundJoin
                            PathMove { x: checkShape.width / 2 - 6; y: checkShape.height / 2 }
                            PathLine { x: checkShape.width / 2 - 2; y: checkShape.height / 2 + 3 }
                            PathLine { x: checkShape.width / 2 + 6; y: checkShape.height / 2 - 3 }
                        }
                    }
                }

                // 验证失败 - 显示红色圆圈+X
                Rectangle {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    radius: width / 2  // 声明式圆角
                    color: "transparent"
                    border.color: AppStyle.colors.danger
                    border.width: AppStyle.borderWidths.thick
                    visible: (itemData && itemData.validationPhase === "failed")
                    Shape {
                        id: crossShape
                        anchors.fill: parent
                        ShapePath {
                            strokeColor: AppStyle.colors.danger
                            strokeWidth: 2
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            PathMove { x: crossShape.width / 2 - 6; y: crossShape.height / 2 - 6 }
                            PathLine { x: crossShape.width / 2 + 6; y: crossShape.height / 2 + 6 }
                        }
                        ShapePath {
                            strokeColor: AppStyle.colors.danger
                            strokeWidth: 2
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            PathMove { x: crossShape.width / 2 + 6; y: crossShape.height / 2 - 6 }
                            PathLine { x: crossShape.width / 2 - 6; y: crossShape.height / 2 + 6 }
                        }
                    }
                }
            }

            // 放大预览图 - 延迟加载的 Popup
            Loader {
                id: previewPopupLoader
                active: false
                asynchronous: true
                // 跟踪是否应该在加载完成后显示
                property bool pendingShow: false
                onLoaded: {
                    if (pendingShow && item) {
                        item.visible = true;
                    }
                }
                sourceComponent: Popup {
                    id: previewPopup
                    parent: previewBackground
                    width: 490 // 三张图片 150x3 + 间距
                    height: 170
                    padding: 0
                    // visible 由 Timer 控制，悬停1秒后显示
                    visible: false
                    closePolicy: Popup.NoAutoClose
                    modal: false
                    focus: false
                    dim: false
                    // 位置计算
                    onVisibleChanged: {
                        if (visible) {
                            // 获取缩略图在屏幕上的位置
                            var thumbGlobalPos = previewBackground.mapToGlobal(Qt.point(0, 0));
                            // 获取窗口在屏幕上的位置
                            var window = rootItem.Window ? rootItem.Window.window : null;
                            var windowGlobalX = window ? window.x : 0;
                            var windowWidth = window ? window.width : rootItem.width;
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
                            y = (previewBackground.height - height) / 2;
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
                        border.width: AppStyle.borderWidths.thick
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

                    contentItem: Item {
                        anchors.fill: parent
                        anchors.margins: 10
                        visible: itemData && itemData.isValid
                        // 有预览图时显示所有图片（最多3张）
                        Row {
                            anchors.fill: parent
                            spacing: 10
                            visible: itemData && itemData.previewImageCount > 0
                            Repeater {
                                model: itemData ? Math.min(itemData.previewImageCount, 3) : 0
                                Rectangle {
                                    width: 150
                                    height: 150
                                    color: AppStyle.colors.background
                                    radius: AppStyle.radius.sm
                                    border.color: AppStyle.colors.border
                                    border.width: AppStyle.borderWidths.thin
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        sourceSize: Qt.size(150, 150)  // 提示按实际显示大小解码
                                        source: {
                                            if (!itemData || !itemData.previewImages) {
                                                return "";
                                            }
                                            var imageData = itemData.previewImages[index];
                                            if (!imageData) {
                                                return "";
                                            }
                                            return "data:image/jpeg;base64," + imageData;
                                        }
                                        fillMode: Image.PreserveAspectFit
                                        cache: true
                                        asynchronous: true
                                    }

                                    // 图片序号标记
                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        width: 20
                                        height: 20
                                        color: AppStyle.colors.primary
                                        radius: width / 2  // 声明式圆角
                                        Text {
                                            anchors.centerIn: parent
                                            text: index + 1
                                            font.pixelSize: 11  // 小尺寸序号，暂不归入标准字体阶梯
                                            font.bold: true
                                            color: AppStyle.colors.textOnPrimary
                                        }
                                    }

                                    // 底部元器件编号遮罩
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: parent.width
                                        height: 25
                                        color: AppStyle.colors.overlay
                                        Text {
                                            anchors.centerIn: parent
                                            text: itemData ? itemData.componentId : ""
                                            color: AppStyle.colors.textOnPrimary
                                            font.bold: true
                                            font.pixelSize: AppStyle.fontSizes.xxs
                                        }
                                    }
                                }
                            }
                        }

                        // 无预览图时显示提示
                        Text {
                            anchors.centerIn: parent
                            text: "无预览图"
                            font.pixelSize: AppStyle.fontSizes.lg
                            font.bold: true
                            color: AppStyle.colors.textSecondary
                            visible: !itemData || !itemData.previewImageCount || itemData.previewImageCount === 0
                        }
                    }
                }
            }

            // 预览图弹窗显示函数
            function showPreviewPopup() {
                if (!previewPopupLoader.active) {
                    previewPopupLoader.pendingShow = true;
                    previewPopupLoader.active = true;
                } else if (previewPopupLoader.item) {
                    previewPopupLoader.item.visible = true;
                }
            }

            // 预览图弹窗隐藏函数
            function hidePreviewPopup() {
                previewPopupLoader.pendingShow = false;
                if (previewPopupLoader.item) {
                    previewPopupLoader.item.visible = false;
                }
            }

            // 预览区域交互
            MouseArea {
                id: previewMouseArea
                width: previewBackground.width
                height: previewBackground.height
                hoverEnabled: true
                cursorShape: (itemData && itemData.previewImageCount > 0) ? Qt.PointingHandCursor : Qt.ArrowCursor
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
                onContainsMouseChanged: {
                    if (containsMouse) {
                        hoverDelayTimer.start();
                    } else {
                        hoverDelayTimer.stop();
                        previewArea.hidePreviewPopup();
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
                    // 使用缓存的正则，避免重复创建
                    regexUpdateTimer.restart();
                    if (!cachedRegex) {
                        return cid;
                    }
                    // 重置正则状态（全局匹配需要重置 lastIndex）
                    cachedRegex.lastIndex = 0;
                    // 高亮颜色使用 Primary 颜色 (#3b82f6)
                    return cid.replace(cachedRegex, "<font color='#3b82f6'><b>$1</b></font>");
                }

                font.pixelSize: AppStyle.fontSizes.md
                font.family: "Courier New"
                font.bold: true
                color: (itemData && itemData.validationPhase === "failed") ? AppStyle.colors.danger : AppStyle.colors.textPrimary
                elide: Text.ElideRight
            }

            // 名称/封装/状态信息
            Text {
                Layout.fillWidth: true
                text: {
                    if (!itemData)
                        return "";
                    // 使用 validationPhase 显示精确状态
                    var phase = itemData.validationPhase || "idle";
                    if (phase === "validating")
                        return "正在验证 CAD 数据...";
                    if (phase === "fetching_preview")
                        return "正在获取预览图...";
                    if (phase === "failed")
                        return itemData.errorMessage || "验证失败";
                    if (phase === "completed" || itemData.isValid) {
                        var info = [];
                        if (itemData.name)
                            info.push(itemData.name);
                        if (itemData.package)
                            info.push(itemData.package);
                        return info.join(" | ");
                    }
                    return "";
                }
                font.pixelSize: AppStyle.fontSizes.sm
                color: (itemData && itemData.validationPhase === "failed") ? AppStyle.colors.danger : AppStyle.colors.textSecondary
                elide: Text.ElideRight
            }
        }
        // 重试按钮（仅对验证失败的元器件显示）
        Button {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            visible: itemData && itemData.validationPhase === "failed" && itemData.retryable
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
                rootItem.retryClicked();
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
                rootItem.deleteClicked();
            }
        }
    }
}
