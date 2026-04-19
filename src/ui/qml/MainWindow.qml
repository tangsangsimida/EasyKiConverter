import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import QtQuick.Effects
import QtQml.Models
import "styles"
import "components"
import EasyKiconverter_Cpp_Version 1.0

Item {
    id: window
    focus: true
    // 连接到 ViewModel
    property var componentListController: componentListViewModel
    property var exportSettingsController: exportSettingsViewModel
    property var exportProgressController: exportProgressViewModel
    property var themeController: themeSettingsViewModel
    property var updateChecker: updateCheckerService
    // 窗口状态属性
    readonly property bool isMaximized: Window.window ? (Window.window.visibility === Window.Maximized || Window.window.visibility === Window.FullScreen) : false
    readonly property int windowRadius: isMaximized ? 0 : AppStyle.radius.lg
    readonly property int calculatedMinimumWindowWidth: calculateMinimumWidth()
    function mainFlickable() {
        return scrollView.contentItem;
    }

    function clampScroll(targetY) {
        var flickable = mainFlickable();
        if (!flickable) {
            return 0;
        }
        var maxY = Math.max(0, flickable.contentHeight - flickable.height);
        return Math.max(0, Math.min(maxY, targetY));
    }

    function scrollToTop() {
        var flickable = mainFlickable();
        if (flickable) {
            flickable.contentY = 0;
        }
    }

    function scrollToBottom() {
        var flickable = mainFlickable();
        if (flickable) {
            flickable.contentY = clampScroll(flickable.contentHeight);
        }
    }

    function scrollPage(deltaPages) {
        var flickable = mainFlickable();
        if (flickable) {
            flickable.contentY = clampScroll(flickable.contentY + flickable.height * deltaPages);
        }
    }
    // 用于测量文本宽度的 FontMetrics（考虑 DPI 缩放）
    FontMetrics {
        id: textMetrics
        font.pixelSize: ResponsiveHelper.fontSizes.md * ResponsiveHelper.scaleFactor  // 应用 DPI 缩放因子
        font.family: "Arial"
    }

    // 当前语言标识（从 LanguageManager 获取初始值）
    property string currentLanguage: LanguageManager ? (LanguageManager.instance ? LanguageManager.instance().currentLanguage : "zh_CN") : "zh_CN"
    // 监听 LanguageManager 的语言切换信号
    Connections {
        target: LanguageManager && LanguageManager.instance ? LanguageManager.instance() : null
        function onLanguageChanged(language) {
            // 语言切换时重新计算最小窗口宽度
            currentLanguage = language;
            console.log("语言切换到:", currentLanguage, "重新计算最小窗口宽度");
            // 强制触发重新计算
            Qt.callLater(function () {
                if (Window.window) {
                    Window.window.dynamicMinimumWidth = calculateMinimumWidth();
                    console.log("更新最小窗口宽度为:", Window.window.dynamicMinimumWidth);
                }
            });
        }
    }
    Shortcut {
        sequence: "Home"
        context: Qt.ApplicationShortcut
        onActivated: scrollToTop()
    }

    Shortcut {
        sequence: "End"
        context: Qt.ApplicationShortcut
        onActivated: scrollToBottom()
    }

    Shortcut {
        sequence: "PageUp"
        context: Qt.ApplicationShortcut
        onActivated: scrollPage(-1)
    }

    Shortcut {
        sequence: "PgUp"
        context: Qt.ApplicationShortcut
        onActivated: scrollPage(-1)
    }

    Shortcut {
        sequence: "PageDown"
        context: Qt.ApplicationShortcut
        onActivated: scrollPage(1)
    }

    Shortcut {
        sequence: "PgDown"
        context: Qt.ApplicationShortcut
        onActivated: scrollPage(1)
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_PageUp) {
            scrollPage(-1);
            event.accepted = true;
        } else if (event.key === Qt.Key_PageDown) {
            scrollPage(1);
            event.accepted = true;
        }
    }

    // 计算最小窗口宽度（依赖 currentLanguage，语言切换时会自动重新计算）
    function calculateMinimumWidth() {
        // 始终测量所有可能显示的文本（不区分语言，确保安全）
        // 选项文本
        var optionTexts = [qsTranslate("MainWindow", "符号库"), qsTranslate("MainWindow", "封装库"), qsTranslate("MainWindow", "3D模型"), qsTranslate("MainWindow", "预览图"), qsTranslate("MainWindow", "手册"), qsTranslate("MainWindow", "追加"), qsTranslate("MainWindow", "更新")];
        // 说明文字（最长的文本）
        var descriptionTexts = [qsTranslate("MainWindow", "保留已存在的元器件"), qsTranslate("MainWindow", "覆盖已存在的元器件")];
        // 合并所有文本用于测量
        var texts = optionTexts.concat(descriptionTexts);
        // 测量文本宽度
        var maxOptionTextWidth = 0;
        var maxDescriptionTextWidth = 0;
        for (var i = 0; i < texts.length; i++) {
            var textWidth = textMetrics.advanceWidth(texts[i]);
            if (i < optionTexts.length && textWidth > maxOptionTextWidth) {
                maxOptionTextWidth = textWidth;
            }
            if (i >= optionTexts.length && textWidth > maxDescriptionTextWidth) {
                maxDescriptionTextWidth = textWidth;
            }
        }

        // 计算普通选项所需宽度
        // 5个普通选项（每个选项包含复选框 + 文本）
        var checkboxWidth = 22;  // 复选框宽度
        var checkboxSpacing = 8;  // 复选框与文本之间的间距
        var normalOptionWidth = checkboxWidth + checkboxSpacing + maxOptionTextWidth;
        // 计算导出模式选项所需宽度
        // 导出模式选项包含：单选按钮 + 文本 + 换行 + 说明文字
        var radioButtonWidth = 20;  // 单选按钮宽度
        var radioButtonSpacing = 8;  // 单选按钮与文本之间的间距
        // 导出模式选项的宽度需要能容纳说明文字
        // 第一行：单选按钮 + 选项文本
        var firstLineWidth = radioButtonWidth + radioButtonSpacing + maxOptionTextWidth;
        // 第二行：说明文字
        var secondLineWidth = maxDescriptionTextWidth + 10;  // 减少缩进空间
        // 取两行中较大的，并根据语言调整缓冲区
        var modeOptionWidth = Math.max(firstLineWidth, secondLineWidth);
        // 根据语言调整缓冲区：中文0px，英文5px（最小化缓冲区）
        var bufferSize = (currentLanguage === "zh_CN") ? 0 : 5;
        modeOptionWidth = modeOptionWidth + bufferSize;
        // 6个选项的总宽度（5个普通选项 + 1个导出模式选项）
        var optionsTotalWidth = normalOptionWidth * 5 + modeOptionWidth;
        // 5个间距（选项之间只保留1px的间隔）
        var minSpacing = 1;
        var spacingWidth = minSpacing * 5;
        // 卡片的左右内边距（12px * 2）
        var cardPadding = 12 * 2;
        // 卡片的边框（1px * 2）
        var cardBorder = 2;
        // 窗口的额外边距（10px * 2）
        var windowMargin = 10 * 2;
        // 滚动条宽度（0px，最小窗口时隐藏滚动条）
        var scrollBarWidth = 0;
        // 安全边距（5px）
        var safetyMargin = 5;
        // 总计
        var minWidth = optionsTotalWidth + spacingWidth + cardPadding + cardBorder + windowMargin + scrollBarWidth + safetyMargin;
        console.log("计算的最小窗口宽度:", minWidth);
        console.log("  最大文本宽度:", maxOptionTextWidth);
        console.log("  普通选项宽度:", normalOptionWidth);
        console.log("  导出模式选项宽度:", modeOptionWidth);
        console.log("  选项总宽度:", optionsTotalWidth);
        return minWidth;
    }
    // 绑定 AppStyle.isDarkMode 到 themeSettingsViewModel.isDarkMode
    Binding {
        target: AppStyle
        property: "isDarkMode"
        value: themeSettingsViewModel ? themeSettingsViewModel.isDarkMode : false
    }
    // BOM 文件选择对话框
    FileDialog {
        id: bomFileDialog
        title: qsTr("选择 BOM 文件")
        nameFilters: ["Supported files (*.txt *.csv *.xlsx *.xls)", "Text files (*.txt)", "CSV files (*.csv)", "Excel files (*.xlsx *.xls)", "All files (*.*)"]
        onAccepted: {
            componentListController.selectBomFile(selectedFile);
        }
    }
    // 输出路径选择对话框
    FolderDialog {
        id: outputFolderDialog
        title: qsTr("选择输出目录")
        onAccepted: {
            // 从 URL 中提取本地路径（跨平台处理）
            var path = selectedFolder.toString();
            if (path.startsWith("file://")) {
                path = path.substring(7); // 移除 "file://" 前缀
                // Windows路径修复：如果路径以 /开头且包含: (如 /C:/)，移除开头的 /
                if (path.startsWith("/") && path.indexOf(":") > 0) {
                    path = path.substring(1);
                }
            }
            exportSettingsController.setOutputPath(path);
        }
    }

    // 主容器
    Rectangle {
        id: mainContainer
        anchors.fill: parent
        color: "transparent"
        radius: AppStyle.radius.lg
        // 源图片（用于 Canvas 绘制）
        Image {
            id: bgSource
            visible: false
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/imgs/background.jpg"
            asynchronous: true
            cache: true
            onStatusChanged: if (status === Image.Ready)
                backgroundCanvas.requestPaint()
        }

        // 画布背景（实现圆角裁切）
        Canvas {
            id: backgroundCanvas
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                // 绘制圆角路径
                var r = windowRadius;
                ctx.beginPath();
                ctx.roundedRect(0, 0, width, height, r, r);
                ctx.closePath();
                // 裁切
                ctx.clip();
                // 绘制背景色（作为底色）
                ctx.fillStyle = AppStyle.colors.background;
                ctx.fill();
                // 绘制图片
                if (bgSource.status === Image.Ready) {
                    // 模拟 PreserveAspectCrop
                    var sw = bgSource.sourceSize.width;
                    var sh = bgSource.sourceSize.height;
                    if (sw > 0 && sh > 0) {
                        var scale = Math.max(width / sw, height / sh);
                        var dw = sw * scale;
                        var dh = sh * scale;
                        var dx = (width - dw) / 2;
                        var dy = (height - dh) / 2;
                        ctx.drawImage(bgSource, dx, dy, dw, dh);
                    }
                }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onVisibleChanged: requestPaint()
            // 监听圆角变化
            property int radiusTrigger: windowRadius
            onRadiusTriggerChanged: requestPaint()
        }
        // 半透明遮罩层（确保内容可读性）
        Rectangle {
            anchors.fill: parent
            color: AppStyle.isDarkMode ? "#000000" : "#ffffff"
            opacity: AppStyle.isDarkMode ? 0.3 : 0.5
            radius: windowRadius
            enabled: false  // 不拦截鼠标事件
            Behavior on color {
                ColorAnimation {
                    duration: AppStyle.durations.themeSwitch
                    easing.type: AppStyle.easings.easeOut
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: AppStyle.durations.themeSwitch
                    easing.type: AppStyle.easings.easeOut
                }
            }
        }

        // 自定义标题栏
        TitleBar {
            id: titleBar
            windowRadius: window.windowRadius
            windowController: Window.window ? Window.window.windowController : null
        }

        // 主滚动区域
        ScrollView {
            id: scrollView
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            enabled: true  // 确保能传递鼠标事件
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            // 内容容器（添加左右边距）
            Item {
                width: scrollView.width
                implicitHeight: contentLayout.implicitHeight
                enabled: true  // 确保能传递鼠标事件
                // 内容区域
                ColumnLayout {
                    id: contentLayout
                    width: parent.width - AppStyle.spacing.huge * 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 30
                    // 头部区域（标题 + 语言选择器 + GitHub + 主题切换 + 分隔线）
                    HeaderSection {
                        Layout.fillWidth: true
                        themeController: window.themeController
                        componentListController: window.componentListController
                    }

                    UpdateBanner {
                        Layout.fillWidth: true
                        updateChecker: window.updateChecker
                    }

                    // 元件输入卡片
                    ComponentInputCard {
                        Layout.fillWidth: true
                        componentListController: window.componentListController
                    }

                    // BOM导入卡片
                    BomImportCard {
                        Layout.fillWidth: true
                        componentListController: window.componentListController
                        exportSettingsController: window.exportSettingsController
                        onOpenBomFileDialog: bomFileDialog.open()
                    }

                    // 元件列表卡片
                    ComponentListCard {
                        Layout.fillWidth: true
                        componentListController: window.componentListController
                        exportProgressController: window.exportProgressController
                    }

                    // 导出设置卡片
                    ExportSettingsCard {
                        Layout.fillWidth: true
                        exportSettingsController: window.exportSettingsController
                        onOpenOutputFolderDialog: outputFolderDialog.open()
                    }

                    // 进度显示卡片
                    ExportProgressCard {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                    }

                    // 转换结果卡片（延迟加载）
                    ExportResultsCard {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                    }

                    // 导出统计卡片（仅在导出完成后显示）
                    ExportStatisticsCard {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                        exportSettingsController: window.exportSettingsController
                    }

                    // 导出按钮组
                    ExportButtonsSection {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                        exportSettingsController: window.exportSettingsController
                        componentListController: window.componentListController
                    }

                    // 底部边距
                    Item {
                        Layout.preferredHeight: 40
                    }
                }
            }
        }
    }

    // 窗口边缘调整大小手柄
    WindowResizeHandles {
        isMaximized: window.isMaximized
    }
}
