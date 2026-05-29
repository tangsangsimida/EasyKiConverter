import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
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
    readonly property int calculatedMinimumWindowWidth: ResponsiveHelper.minimumWindowWidth
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
    // 当前语言标识（从 LanguageManager 获取初始值）
    property string currentLanguage: LanguageManager ? (LanguageManager.instance ? LanguageManager.instance().currentLanguage : "zh_CN") : "zh_CN"
    // 监听 LanguageManager 的语言切换信号
    Connections {
        target: LanguageManager && LanguageManager.instance ? LanguageManager.instance() : null
        function onLanguageChanged(language) {
            currentLanguage = language;
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

    // 绑定 AppStyle.isDarkMode 到 themeController.isDarkMode
    Binding {
        target: AppStyle
        property: "isDarkMode"
        value: themeController ? themeController.isDarkMode : false
    }
    // 将窗口实际宽度写入 ResponsiveHelper，驱动断点系统
    Binding {
        target: ResponsiveHelper
        property: "windowWidth"
        value: window.width
    }
    // 从 FolderDialog URL 中提取本地路径（跨平台处理）
    function urlToLocalPath(url) {
        return QUrl(url).toLocalFile();
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
            exportSettingsController.setOutputPath(urlToLocalPath(selectedFolder));
        }
    }

    FolderDialog {
        id: cacheFolderDialog
        title: qsTr("选择缓存目录")
        onAccepted: {
            exportSettingsController.setCacheDir(urlToLocalPath(selectedFolder));
        }
    }

    // 主容器
    Rectangle {
        id: mainContainer
        anchors.fill: parent
        color: "transparent"
        radius: AppStyle.radius.lg
        // 使用 Canvas 裁剪顶层窗口背景，避免 Windows + Vulkan 透明窗口下的 mask 区域变黑。
        Image {
            id: bgSource
            visible: false
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/imgs/background.jpg"
            asynchronous: true
            cache: true
            onStatusChanged: if (status === Image.Ready) {
                backgroundCanvas.requestPaint();
            }
        }

        Canvas {
            id: backgroundCanvas
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const r = windowRadius;
                ctx.beginPath();
                ctx.roundedRect(0, 0, width, height, r, r);
                ctx.closePath();
                ctx.clip();
                ctx.fillStyle = AppStyle.colors.background;
                ctx.fill();
                if (bgSource.status === Image.Ready) {
                    const sw = bgSource.sourceSize.width;
                    const sh = bgSource.sourceSize.height;
                    if (sw > 0 && sh > 0) {
                        const scale = Math.max(width / sw, height / sh);
                        const dw = sw * scale;
                        const dh = sh * scale;
                        const dx = (width - dw) / 2;
                        const dy = (height - dh) / 2;
                        ctx.drawImage(bgSource, dx, dy, dw, dh);
                    }
                }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onVisibleChanged: requestPaint()
            property int radiusTrigger: windowRadius
            onRadiusTriggerChanged: requestPaint()
            property color backgroundTrigger: AppStyle.colors.background
            onBackgroundTriggerChanged: requestPaint()
        }
        // 半透明遮罩层（确保内容可读性）
        Rectangle {
            anchors.fill: parent
            color: AppStyle.isDarkMode ? AppStyle.overlayMask.dark : AppStyle.overlayMask.light
            opacity: AppStyle.isDarkMode ? AppStyle.overlayMask.opacityDark : AppStyle.overlayMask.opacityLight
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
                // 内容区域（自适应边距 + 最大宽度约束）
                ColumnLayout {
                    id: contentLayout
                    width: Math.max(0, Math.min(parent.width - ResponsiveHelper.contentMargin * 2, ResponsiveHelper.contentMaxWidth))
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: ResponsiveHelper.cardSpacing
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
                        Layout.fillHeight: true
                        exportSettingsController: window.exportSettingsController
                        onOpenOutputFolderDialog: outputFolderDialog.open()
                        onOpenCacheFolderDialog: cacheFolderDialog.open()
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
                        Layout.preferredHeight: ResponsiveHelper.contentMargin
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
