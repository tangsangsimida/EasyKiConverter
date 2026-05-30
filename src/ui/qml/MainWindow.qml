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
    // 对话框引用（供 SidebarPanel 等子组件访问）
    property alias outputFolderDialog: outputFolderDialog
    property alias cacheFolderDialog: cacheFolderDialog
    property alias bomFileDialog: bomFileDialog
    // 窗口状态属性
    readonly property bool isMaximized: Window.window ? (Window.window.visibility === Window.Maximized || Window.window.visibility === Window.FullScreen) : false
    readonly property int windowRadius: isMaximized ? 0 : AppStyle.radius.lg
    readonly property int calculatedMinimumWindowWidth: ResponsiveHelper.minimumWindowWidth
    // 响应式布局模式
    readonly property bool isSidebarMode: ResponsiveHelper.isAtLeastMedium
    // 交互状态
    property int inputMode: 0  // 0=单个, 1=BOM
    // 滚动跟踪（用于 compact 模式下的 sticky bar）
    property bool compactScrolledPastThreshold: false
    property real stickyBarOpacity: 0
    function mainFlickable() {
        return workspaceFlickable;
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
        // GPU 渲染背景（替代 Canvas）
        Image {
            id: bgSource
            visible: false
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/imgs/background.jpg"
            asynchronous: true
            cache: true
        }

        Image {
            id: backgroundImage
            anchors.fill: parent
            source: bgSource.source
            fillMode: Image.PreserveAspectCrop
            visible: bgSource.status === Image.Ready
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: bgMask
            }
        }

        ShaderEffectSource {
            id: bgMask
            sourceItem: bgMaskRect
            visible: false
            live: window.isMaximized !== undefined
        }

        Rectangle {
            id: bgMaskRect
            width: mainContainer.width
            height: mainContainer.height
            radius: windowRadius
            visible: false
        }

        // 背景填充（图片未加载时的纯色背景）
        Rectangle {
            anchors.fill: parent
            color: AppStyle.colors.background
            radius: windowRadius
            z: -1
            Behavior on color {
                ColorAnimation {
                    duration: AppStyle.durations.themeSwitch
                    easing.type: AppStyle.easings.easeOut
                }
            }
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

        // 主布局：侧边栏 + 工作区
        RowLayout {
            id: mainLayout
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 0
            // ==================== 侧边栏控制面板 ====================
            Loader {
                id: sidebarLoader
                active: window.isSidebarMode
                Layout.preferredWidth: active ? 360 : 0
                Layout.fillHeight: true
                sourceComponent: Component {
                    SidebarPanel {
                        onRequestOutputFolderDialog: outputFolderDialog.open()
                        onRequestCacheFolderDialog: cacheFolderDialog.open()
                    }
                }
            }

            // ==================== 工作区（始终可见） ====================
            Flickable {
                id: workspaceFlickable
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: workspaceContentItem.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOff
                }

                onContentYChanged: {
                    window.updateCompactScrollState();
                }

                Item {
                    id: workspaceContentItem
                    width: workspaceFlickable.width
                    implicitHeight: workspaceColumn.implicitHeight
                    ColumnLayout {
                        id: workspaceColumn
                        width: Math.max(0, Math.min(parent.width - ResponsiveHelper.contentMargin * 2, ResponsiveHelper.contentMaxWidth))
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: ResponsiveHelper.cardSpacing
                        // 头部区域
                        HeaderSection {
                            Layout.fillWidth: true
                            themeController: window.themeController
                            componentListController: window.componentListController
                        }

                        UpdateBanner {
                            Layout.fillWidth: true
                            updateChecker: window.updateChecker
                        }

                        // ==================== 数据源输入卡片（合并） ====================
                        Card {
                            Layout.fillWidth: true
                            title: qsTranslate("MainWindow", "数据源")
                            ColumnLayout {
                                width: parent.width
                                spacing: AppStyle.spacing.lg
                                // 模式切换标签
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: AppStyle.spacing.sm
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        radius: AppStyle.radius.sm
                                        color: inputMode === 0 ? AppStyle.colors.primary : "transparent"
                                        border.color: inputMode === 0 ? AppStyle.colors.primary : AppStyle.colors.border
                                        border.width: AppStyle.borderWidths.thin
                                        Text {
                                            anchors.centerIn: parent
                                            text: qsTranslate("MainWindow", "单个元器件")
                                            font.pixelSize: AppStyle.fontSizes.sm
                                            color: inputMode === 0 ? AppStyle.colors.textOnPrimary : AppStyle.colors.textSecondary
                                            font.bold: inputMode === 0
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: inputMode = 0
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        radius: AppStyle.radius.sm
                                        color: inputMode === 1 ? AppStyle.colors.success : "transparent"
                                        border.color: inputMode === 1 ? AppStyle.colors.success : AppStyle.colors.border
                                        border.width: AppStyle.borderWidths.thin
                                        Text {
                                            anchors.centerIn: parent
                                            text: qsTranslate("MainWindow", "批量导入 BOM")
                                            font.pixelSize: AppStyle.fontSizes.sm
                                            color: inputMode === 1 ? AppStyle.colors.textOnPrimary : AppStyle.colors.textSecondary
                                            font.bold: inputMode === 1
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: inputMode = 1
                                        }
                                    }
                                }

                                // 单个元器件输入 / BOM 导入（带横向滑动切换）
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(singleInputColumn.implicitHeight, bomImportColumn.implicitHeight)
                                    clip: true
                                    // 单个元器件输入
                                    ColumnLayout {
                                        id: singleInputColumn
                                        width: parent.width
                                        spacing: AppStyle.spacing.sm
                                        opacity: inputMode === 0 ? 1 : 0
                                        x: inputMode === 0 ? 0 : -20
                                        visible: opacity > 0 || inputMode === 0
                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: AppStyle.durations.fast
                                            }
                                        }
                                        Behavior on x {
                                            NumberAnimation {
                                                duration: AppStyle.durations.fast
                                                easing.type: Easing.OutCubic
                                            }
                                        }

                                        RowLayout {
                                            width: parent.width
                                            spacing: 12
                                            TextField {
                                                id: componentInput
                                                objectName: "componentInputField"
                                                Layout.fillWidth: true
                                                placeholderText: qsTranslate("MainWindow", "输入LCSC元件编号 (例如: C2040)")
                                                font.pixelSize: AppStyle.fontSizes.md
                                                color: AppStyle.colors.textPrimary
                                                placeholderTextColor: AppStyle.colors.textSecondary
                                                Component.onCompleted: {
                                                    Qt.callLater(function () {
                                                        forceActiveFocus();
                                                    });
                                                }
                                                background: Rectangle {
                                                    color: componentInput.enabled ? AppStyle.colors.surface : AppStyle.colors.background
                                                    border.color: componentInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                                                    border.width: componentInput.focus ? 2 : 1
                                                    radius: AppStyle.radius.md
                                                    Behavior on border.color {
                                                        ColorAnimation {
                                                            duration: AppStyle.durations.fast
                                                        }
                                                    }
                                                    Behavior on border.width {
                                                        NumberAnimation {
                                                            duration: AppStyle.durations.fast
                                                        }
                                                    }
                                                }
                                                onAccepted: {
                                                    if (componentInput.text.length > 0) {
                                                        window.componentListController.addComponent(componentInput.text);
                                                        componentInput.text = "";
                                                    }
                                                }
                                            }

                                            ModernButton {
                                                text: qsTranslate("MainWindow", "添加")
                                                iconName: "add"
                                                enabled: componentInput.text.length > 0
                                                onClicked: {
                                                    if (componentInput.text.length > 0) {
                                                        window.componentListController.addComponent(componentInput.text);
                                                        componentInput.text = "";
                                                    }
                                                }
                                            }

                                            ModernButton {
                                                text: qsTranslate("MainWindow", "粘贴")
                                                iconName: "folder"
                                                backgroundColor: AppStyle.colors.textSecondary
                                                hoverColor: AppStyle.colors.textPrimary
                                                pressedColor: AppStyle.colors.textPrimary
                                                onClicked: {
                                                    window.componentListController.pasteFromClipboard();
                                                }
                                            }
                                        }
                                    }

                                    // BOM 导入
                                    ColumnLayout {
                                        id: bomImportColumn
                                        width: parent.width
                                        spacing: AppStyle.spacing.sm
                                        opacity: inputMode === 1 ? 1 : 0
                                        x: inputMode === 1 ? 0 : 20
                                        visible: opacity > 0 || inputMode === 1
                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: AppStyle.durations.fast
                                            }
                                        }
                                        Behavior on x {
                                            NumberAnimation {
                                                duration: AppStyle.durations.fast
                                                easing.type: Easing.OutCubic
                                            }
                                        }

                                        RowLayout {
                                            width: parent.width
                                            spacing: 12
                                            ModernButton {
                                                text: qsTranslate("MainWindow", "选择BOM文件")
                                                iconName: "upload"
                                                backgroundColor: AppStyle.colors.success
                                                hoverColor: AppStyle.colors.successDark
                                                pressedColor: AppStyle.colors.successDark
                                                onClicked: {
                                                    bomFileDialog.open();
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: window.componentListController && window.componentListController.bomFilePath && window.componentListController.bomFilePath.length > 0 ? window.componentListController.bomFilePath.split("/").pop() : qsTranslate("MainWindow", "未选择文件")
                                                font.pixelSize: AppStyle.fontSizes.sm
                                                color: AppStyle.colors.textSecondary
                                                horizontalAlignment: Text.AlignHCenter
                                                elide: Text.ElideMiddle
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.topMargin: AppStyle.spacing.xs
                                            text: window.componentListController ? window.componentListController.bomResult : ""
                                            font.pixelSize: AppStyle.fontSizes.sm
                                            color: AppStyle.colors.success
                                            horizontalAlignment: Text.AlignHCenter
                                            visible: window.componentListController && window.componentListController.bomResult && window.componentListController.bomResult.length > 0
                                        }
                                    }
                                }

                                // 弱网络选项（始终可见）
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    CheckBox {
                                        id: weakNetworkCheckbox
                                        Layout.fillWidth: false
                                        text: qsTranslate("MainWindow", "客户端弱网络适配")
                                        checked: window.exportSettingsController ? window.exportSettingsController.weakNetworkSupport : false
                                        onCheckedChanged: {
                                            if (window.exportSettingsController) {
                                                window.exportSettingsController.setWeakNetworkSupport(checked);
                                            }
                                        }
                                        font.pixelSize: AppStyle.fontSizes.sm
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTranslate("MainWindow", "仅影响本机网络请求策略，不代表服务器限流；开启后会使用更保守的并发、超时和重试配置")
                                        ToolTip.delay: 500
                                        indicator: Rectangle {
                                            implicitWidth: 22
                                            implicitHeight: 22
                                            x: weakNetworkCheckbox.leftPadding
                                            y: parent.height / 2 - height / 2
                                            radius: AppStyle.radius.xs
                                            color: weakNetworkCheckbox.checked ? AppStyle.colors.primary : "transparent"
                                            border.color: weakNetworkCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                            border.width: AppStyle.borderWidths.normal
                                            Behavior on color {
                                                ColorAnimation {
                                                    duration: 150
                                                }
                                            }
                                            Behavior on border.color {
                                                ColorAnimation {
                                                    duration: 150
                                                }
                                            }

                                            Text {
                                                anchors.centerIn: parent
                                                text: "✓"
                                                font.pixelSize: AppStyle.fontSizes.sm
                                                color: AppStyle.colors.textOnPrimary
                                                visible: weakNetworkCheckbox.checked
                                            }
                                        }

                                        contentItem: Text {
                                            text: parent.text
                                            font: parent.font
                                            color: AppStyle.colors.textPrimary
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: parent.indicator.width + parent.spacing
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTranslate("MainWindow", "导入 BOM 前可先开启，验证和预览图加载也会使用该策略")
                                        font.pixelSize: AppStyle.fontSizes.xs
                                        color: AppStyle.colors.textSecondary
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }

                        // 元件列表卡片
                        ComponentListCard {
                            Layout.fillWidth: true
                            componentListController: window.componentListController
                            exportProgressController: window.exportProgressController
                        }

                        // ==================== Compact 模式独有内容 ====================
                        // 转换进度
                        ExportProgressCard {
                            Layout.fillWidth: true
                            exportProgressController: window.exportProgressController
                            visible: !window.isSidebarMode && ((window.exportProgressController && window.exportProgressController.isExporting) || (window.exportProgressController && window.exportProgressController.progress > 0))
                        }

                        // 转换结果
                        ExportResultsCard {
                            Layout.fillWidth: true
                            exportProgressController: window.exportProgressController
                            visible: !window.isSidebarMode
                        }

                        // 导出统计
                        ExportStatisticsCard {
                            Layout.fillWidth: true
                            exportProgressController: window.exportProgressController
                            exportSettingsController: window.exportSettingsController
                            visible: !window.isSidebarMode
                        }

                        // 导出设置
                        Loader {
                            id: compactSettingsLoader
                            Layout.fillWidth: true
                            active: !window.isSidebarMode
                            sourceComponent: ExportSettingsCard {
                                exportSettingsController: window.exportSettingsController
                            }
                        }

                        Connections {
                            target: compactSettingsLoader.item
                            function onOpenOutputFolderDialog() {
                                outputFolderDialog.open();
                            }
                            function onOpenCacheFolderDialog() {
                                cacheFolderDialog.open();
                            }
                        }

                        // 导出按钮组
                        Loader {
                            id: compactButtonsLoader
                            Layout.fillWidth: true
                            active: !window.isSidebarMode
                            sourceComponent: ExportButtonsSection {
                                exportProgressController: window.exportProgressController
                                exportSettingsController: window.exportSettingsController
                                componentListController: window.componentListController
                            }
                        }

                        // 底部间距（为 sticky bar 预留空间）
                        Item {
                            Layout.preferredHeight: window.compactScrolledPastThreshold ? 80 : ResponsiveHelper.contentMargin
                        }
                    }
                }
            }
        }

        // ==================== Compact 模式浮动操作栏 ====================
        Rectangle {
            id: stickyActionBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 64
            color: AppStyle.isDarkMode ? Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.95) : Qt.rgba(255, 255, 255, 0.95)
            visible: !window.isSidebarMode && window.compactScrolledPastThreshold
            opacity: window.stickyBarOpacity
            z: 500
            // 顶部边框
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: AppStyle.colors.border
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.6
                shadowColor: AppStyle.isDarkMode ? "#80000000" : "#20000000"
                shadowVerticalOffset: -4
                shadowHorizontalOffset: 0
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: AppStyle.durations.fast
                    easing.type: AppStyle.easings.easeOut
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: ResponsiveHelper.contentMargin
                anchors.rightMargin: ResponsiveHelper.contentMargin
                spacing: AppStyle.spacing.md
                Text {
                    text: {
                        var lang = window.currentLanguage;
                        var pc = window.exportProgressController;
                        if (pc && pc.isExporting)
                            return qsTranslate("MainWindow", "正在转换...");
                        if (pc && pc.failureCount > 0)
                            return qsTranslate("MainWindow", "有 %1 项失败").arg(pc.failureCount);
                        return qsTranslate("MainWindow", "就绪");
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    color: AppStyle.colors.textSecondary
                    Layout.alignment: Qt.AlignVCenter
                }

                // 进度条
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    Layout.alignment: Qt.AlignVCenter
                    radius: 3
                    color: AppStyle.colors.border
                    visible: window.exportProgressController && window.exportProgressController.isExporting
                    Rectangle {
                        width: parent.width * (window.exportProgressController ? window.exportProgressController.progress / 100 : 0)
                        height: parent.height
                        radius: 3
                        color: AppStyle.colors.primary
                        Behavior on width {
                            NumberAnimation {
                                duration: 100
                            }
                        }
                    }
                }

                // 打开目录按钮
                ModernButton {
                    objectName: "stickyOpenFolderButton"
                    text: qsTranslate("MainWindow", "打开目录")
                    iconName: "folder"
                    font.pixelSize: AppStyle.fontSizes.sm
                    backgroundColor: AppStyle.colors.primary
                    hoverColor: AppStyle.colors.primaryHover
                    pressedColor: AppStyle.colors.primaryPressed
                    visible: window.exportProgressController && window.exportProgressController.hasCompletedExport
                    onClicked: {
                        if (window.exportProgressController) {
                            window.exportProgressController.openLastExportedFolder();
                        }
                    }
                }

                // 开始/重试按钮
                ModernButton {
                    objectName: "stickyStartButton"
                    text: {
                        var pc = window.exportProgressController;
                        if (pc && pc.isExporting)
                            return qsTranslate("MainWindow", "转换中");
                        if (pc && pc.failureCount > 0)
                            return qsTranslate("MainWindow", "重试");
                        return qsTranslate("MainWindow", "开始转换");
                    }
                    iconName: "play"
                    font.pixelSize: AppStyle.fontSizes.sm
                    Layout.preferredHeight: 40
                    backgroundColor: {
                        var pc = window.exportProgressController;
                        if (pc && pc.isExporting)
                            return AppStyle.colors.textDisabled;
                        if (pc && pc.failureCount > 0)
                            return AppStyle.colors.warning;
                        return AppStyle.colors.primary;
                    }
                    hoverColor: {
                        var pc = window.exportProgressController;
                        if (pc && pc.failureCount > 0 && !(pc && pc.isExporting))
                            return AppStyle.colors.warningDark;
                        return AppStyle.colors.primaryHover;
                    }
                    pressedColor: {
                        var pc = window.exportProgressController;
                        if (pc && pc.failureCount > 0 && !(pc && pc.isExporting))
                            return AppStyle.colors.warningDark;
                        return AppStyle.colors.primaryPressed;
                    }
                    enabled: {
                        var pc = window.exportProgressController;
                        var lc = window.componentListController;
                        var sc = window.exportSettingsController;
                        if (pc && pc.isExporting)
                            return false;
                        if (!lc || lc.componentCount <= 0)
                            return false;
                        if (!sc)
                            return false;
                        return sc.exportSymbol || sc.exportFootprint || sc.exportModel3D;
                    }
                    onClicked: {
                        var pc = window.exportProgressController;
                        var sc = window.exportSettingsController;
                        if (pc && pc.failureCount > 0) {
                            pc.retryFailedComponents();
                        } else if (pc && sc) {
                            var idList = window.componentListController ? window.componentListController.getAllComponentIds() : [];
                            pc.startExport(idList, sc.outputPath || "", sc.libName || "", sc.exportSymbol || false, sc.exportFootprint || false, sc.exportModel3D || false, sc.exportModel3DFormat || 3, sc.exportModel3DPathMode || 0, sc.exportPreviewImages || false, sc.exportDatasheet || false, sc.overwriteExistingFiles || false, (sc.exportMode || 0) === 1, sc.debugMode || false, sc.symbolLibraryDescription || "", sc.footprintLibraryDescription || "", sc.footprintLibraryKeywords || "");
                        }
                    }
                }

                // 停止按钮
                ModernButton {
                    objectName: "stickyStopButton"
                    text: (window.exportProgressController && window.exportProgressController.isStopping) ? qsTranslate("MainWindow", "停止中") : qsTranslate("MainWindow", "停止")
                    iconName: "close"
                    font.pixelSize: AppStyle.fontSizes.sm
                    Layout.preferredHeight: 40
                    backgroundColor: AppStyle.colors.danger
                    hoverColor: AppStyle.colors.dangerDark
                    pressedColor: AppStyle.colors.dangerDark
                    visible: window.exportProgressController && window.exportProgressController.isExporting
                    enabled: window.exportProgressController ? !window.exportProgressController.isStopping : false
                    onClicked: {
                        if (window.exportProgressController) {
                            window.exportProgressController.cancelExport();
                        }
                    }
                }
            }
        }
    }

    // 窗口边缘调整大小手柄
    WindowResizeHandles {
        isMaximized: window.isMaximized
    }

    // ==================== 辅助函数 ====================
    // Compact 模式滚动状态跟踪（用于 sticky bar 显示/隐藏）
    function updateCompactScrollState() {
        var flickable = mainFlickable();
        if (!flickable || window.isSidebarMode) {
            compactScrolledPastThreshold = false;
            stickyBarOpacity = 0;
            return;
        }
        // 仅在内容确实可滚动且超过阈值时激活
        var canScroll = flickable.contentHeight > flickable.height + 10;
        var scrolled = canScroll && flickable.contentY > 200;
        if (scrolled !== compactScrolledPastThreshold) {
            compactScrolledPastThreshold = scrolled;
            stickyBarOpacity = scrolled ? 1 : 0;
        }
    }
}
