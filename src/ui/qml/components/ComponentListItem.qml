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
    // 导出状态（由父级传入，来自 ExportProgressViewModel）
    property var exportStatus: null
    readonly property bool isExporting: exportStatus && (exportStatus.status === "in_progress" || exportStatus.status === "pending")
    readonly property bool exportSuccess: exportStatus && exportStatus.status === "success"
    readonly property bool exportFailed: exportStatus && exportStatus.status === "failed"
    signal deleteClicked
    signal copyClicked
    signal retryClicked
    signal descriptionEditRequested(string componentId, string description)
    height: 64 // 增加高度以容纳缩略图和更多信息
    // 悬停效果
    color: itemMouseArea.containsMouse ? AppStyle.colors.background : AppStyle.colors.surface
    radius: AppStyle.radius.md
    border.color: {
        if (exportSuccess)
            return AppStyle.colors.success;
        if (exportFailed)
            return AppStyle.colors.danger;
        if (isExporting)
            return AppStyle.colors.warning;
        return AppStyle.colors.border;
    }
    border.width: exportStatus ? 2 : AppStyle.borderWidths.thin
    Behavior on border.color {
        ColorAnimation {
            duration: AppStyle.durations.fast
        }
    }

    // 底部导出状态指示条
    Rectangle {
        id: exportStatusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 3
        radius: 1
        visible: exportStatus !== null
        color: {
            if (exportSuccess)
                return AppStyle.colors.success;
            if (exportFailed)
                return AppStyle.colors.danger;
            if (isExporting)
                return AppStyle.colors.warning;
            return AppStyle.colors.border;
        }
        clip: true
        // 导出中：扫光（Shimmer）效果
        Rectangle {
            id: shimmerEffect
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * 0.4
            visible: isExporting
            opacity: 0.6
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: "transparent"
                }
                GradientStop {
                    position: 0.5
                    color: Qt.rgba(1, 1, 1, 0.8)
                }
                GradientStop {
                    position: 1.0
                    color: "transparent"
                }
            }
            SequentialAnimation on x {
                running: isExporting
                loops: Animation.Infinite
                NumberAnimation {
                    from: -shimmerEffect.width
                    to: exportStatusBar.width
                    duration: 1200
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
    // 缓存搜索正则以优化性能
    property string cachedSearchText: ""
    property var cachedRegex: null
    function previewImageSource(imageData) {
        if (!imageData || imageData === "")
            return "";
        if (imageData.indexOf("data:image/") === 0)
            return imageData;
        if (imageData.indexOf("/9j/") === 0)
            return "data:image/jpeg;base64," + imageData;
        if (imageData.indexOf("iVBOR") === 0)
            return "data:image/png;base64," + imageData;
        if (imageData.indexOf("R0lG") === 0)
            return "data:image/gif;base64," + imageData;
        if (imageData.indexOf("UklGR") === 0)
            return "data:image/webp;base64," + imageData;
        return "data:image/png;base64," + imageData;
    }
    function previewImageSources() {
        var result = [];
        if (!itemData || !itemData.previewImages)
            return result;
        for (var i = 0; i < itemData.previewImages.length && result.length < 3; ++i) {
            var source = previewImageSource(itemData.previewImages[i]);
            if (source !== "")
                result.push(source);
        }
        return result;
    }
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
        // 预览图区域 - 使用全局单例弹窗
        Item {
            id: previewArea
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            // 悬停意图延迟，避免鼠标扫过时触发重型预览浮层。
            Timer {
                id: hoverDelayTimer
                interval: AppStyle.interactions.previewIntentDelay
                repeat: false
                onTriggered: {
                    if (previewMouseArea.containsMouse && itemData && itemData.previewImageCount > 0) {
                        var card = previewArea.findCard();
                        if (card) {
                            hideDelayTimer.stop();
                            var sources = rootItem.previewImageSources();
                            var thumbPos = previewBackground.mapToItem(null, 0, 0);
                            var cursorPos = previewMouseArea.mapToItem(null, previewMouseArea.mouseX, previewMouseArea.mouseY);
                            card.showPreviewPopup(sources, itemData, thumbPos.x, thumbPos.y, previewBackground.width, previewBackground.height, cursorPos.x, cursorPos.y);
                        }
                    }
                }
            }
            Timer {
                id: hideDelayTimer
                interval: AppStyle.interactions.popupHideDelay
                repeat: false
                onTriggered: {
                    if (previewMouseArea.containsMouse)
                        return;
                    var card = previewArea.findCard();
                    if (card)
                        card.hidePreviewPopup();
                }
            }
            // 沿父级链查找 ComponentListCard
            function findCard() {
                var p = parent;
                while (p) {
                    if (p.showPreviewPopup !== undefined)
                        return p;
                    p = p.parent;
                }
                return null;
            }
            Component.onDestruction: {
                var card = previewArea.findCard();
                if (card)
                    card.hidePreviewPopup();
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
                    sourceSize: Qt.size(46, 46)
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
                        var sources = rootItem.previewImageSources();
                        return sources.length > 0 ? sources[0] : "";
                    }
                    fillMode: Image.PreserveAspectFit
                    cache: true
                    asynchronous: true
                    visible: itemData && (itemData.validationPhase === "completed" || itemData.validationPhase === "fetching_preview") && itemData.previewImageCount > 0
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    running: (itemData && (itemData.validationPhase === "validating" || itemData.validationPhase === "fetching_preview")) ? true : false
                    visible: (itemData && (itemData.validationPhase === "validating" || itemData.validationPhase === "fetching_preview")) ? true : false
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    radius: width / 2
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
                            PathMove {
                                x: checkShape.width / 2 - 6
                                y: checkShape.height / 2
                            }
                            PathLine {
                                x: checkShape.width / 2 - 2
                                y: checkShape.height / 2 + 3
                            }
                            PathLine {
                                x: checkShape.width / 2 + 6
                                y: checkShape.height / 2 - 3
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    radius: width / 2
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
                            PathMove {
                                x: crossShape.width / 2 - 6
                                y: crossShape.height / 2 - 6
                            }
                            PathLine {
                                x: crossShape.width / 2 + 6
                                y: crossShape.height / 2 + 6
                            }
                        }
                        ShapePath {
                            strokeColor: AppStyle.colors.danger
                            strokeWidth: 2
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            PathMove {
                                x: crossShape.width / 2 + 6
                                y: crossShape.height / 2 - 6
                            }
                            PathLine {
                                x: crossShape.width / 2 - 6
                                y: crossShape.height / 2 + 6
                            }
                        }
                    }
                }
            }

            MouseArea {
                id: previewMouseArea
                width: previewBackground.width
                height: previewBackground.height
                hoverEnabled: true
                cursorShape: (itemData && itemData.previewImageCount > 0) ? Qt.PointingHandCursor : Qt.ArrowCursor
                acceptedButtons: Qt.LeftButton
                onClicked: function (mouse) {
                    if (mouse.modifiers & Qt.ControlModifier) {
                        if (itemData && itemData.componentId) {
                            var url = "https://so.szlcsc.com/global.html?k=" + itemData.componentId;
                            Qt.openUrlExternally(url);
                        }
                    }
                }
                onContainsMouseChanged: {
                    if (containsMouse) {
                        hideDelayTimer.stop();
                        hoverDelayTimer.restart();
                    } else {
                        hoverDelayTimer.stop();
                        hideDelayTimer.restart();
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
                    // 导出状态优先显示
                    if (exportStatus) {
                        var s = exportStatus.status || "";
                        if (s === "in_progress")
                            return "正在导出...";
                        if (s === "success")
                            return "导出完成";
                        if (s === "failed")
                            return exportStatus.error || "导出失败";
                    }
                    // 使用 validationPhase 显示精确状态
                    var phase = itemData.validationPhase || "idle";
                    if (phase === "validating")
                        return "正在验证 CAD 数据...";
                    if (phase === "fetching_preview")
                        return "正在获取预览图...";
                    if (phase === "failed")
                        return itemData.errorMessage || "验证失败";
                    if (phase === "completed" || itemData.isValid) {
                        return itemData.description || itemData.name || "";
                    }
                    return "";
                }
                font.pixelSize: AppStyle.fontSizes.sm
                color: {
                    if (exportFailed)
                        return AppStyle.colors.danger;
                    if (exportSuccess)
                        return AppStyle.colors.success;
                    if (itemData && itemData.validationPhase === "failed")
                        return AppStyle.colors.danger;
                    return AppStyle.colors.textSecondary;
                }
                elide: Text.ElideRight
            }
        }
        // 重试按钮（仅对验证失败的元器件显示）
        Button {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            visible: itemData && itemData.isValid
            background: Rectangle {
                color: parent.pressed ? AppStyle.colors.primaryPressed : parent.hovered ? "#dbeafe" : "transparent"
                radius: AppStyle.radius.sm
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            contentItem: Text {
                text: "✎"
                font.pixelSize: AppStyle.fontSizes.lg
                font.bold: true
                color: parent.hovered ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("编辑元器件描述")
            onClicked: {
                if (itemData) {
                    rootItem.descriptionEditRequested(itemData.componentId, itemData.description || "");
                }
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
