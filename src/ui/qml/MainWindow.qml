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
    property bool sidebarCollapsed: false
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
    // 将窗口实际尺寸写入 ResponsiveHelper，驱动断点系统
    Binding {
        target: ResponsiveHelper
        property: "windowWidth"
        value: window.width
    }

    Binding {
        target: ResponsiveHelper
        property: "windowHeight"
        value: window.height
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
                z: 1
                active: window.isSidebarMode
                Layout.preferredWidth: {
                    if (!active)
                        return 0;
                    return window.sidebarCollapsed ? 48 : ResponsiveHelper.sidebarWidth;
                }
                Layout.fillHeight: true
                Behavior on Layout.preferredWidth {
                    NumberAnimation {
                        duration: 280
                        easing.type: Easing.OutQuart
                    }
                }
                sourceComponent: Component {
                    SidebarPanel {
                        collapsed: window.sidebarCollapsed
                        onToggleCollapsed: window.sidebarCollapsed = !window.sidebarCollapsed
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

                        // ==================== 元器件添加方式 ====================
                        Card {
                            Layout.fillWidth: true
                            title: qsTranslate("MainWindow", "元器件添加方式")
                            ColumnLayout {
                                width: parent.width
                                spacing: AppStyle.spacing.lg
                                // 滑块式模式切换
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 36
                                    radius: AppStyle.radius.sm
                                    color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(0, 0, 0, 0.06)
                                    // 滑块指示器
                                    Rectangle {
                                        width: parent.width / 2
                                        height: parent.height - 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        radius: AppStyle.radius.sm - 1
                                        color: AppStyle.colors.surface
                                        border.width: AppStyle.borderWidths.thin
                                        border.color: AppStyle.colors.border
                                        x: inputMode === 0 ? 2 : parent.width / 2
                                        Behavior on x {
                                            NumberAnimation {
                                                duration: 200
                                                easing.type: Easing.OutQuart
                                            }
                                        }
                                    }

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        Repeater {
                                            model: [qsTranslate("MainWindow", "手动添加元器件"), qsTranslate("MainWindow", "通过BOM表导入元器件")]
                                            Item {
                                                width: parent.width / 2
                                                height: parent.height
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData
                                                    font.pixelSize: AppStyle.fontSizes.xs
                                                    font.bold: index === inputMode
                                                    color: index === inputMode ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                                    Behavior on color {
                                                        ColorAnimation {
                                                            duration: 200
                                                        }
                                                    }
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: inputMode = index
                                                }
                                            }
                                        }
                                    }
                                }

                                // 单个元器件输入 / BOM 导入（带横向滑动切换）
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: inputMode === 0 ? singleInputColumn.implicitHeight : bomImportColumn.implicitHeight
                                    clip: true
                                    Behavior on Layout.preferredHeight {
                                        NumberAnimation {
                                            duration: 200
                                            easing.type: Easing.OutQuart
                                        }
                                    }
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

                                    // BOM 导入（拖拽式卡片）
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

                                        property bool hasFile: window.componentListController && window.componentListController.bomFilePath && window.componentListController.bomFilePath.length > 0
                                        property string fileName: hasFile ? window.componentListController.bomFilePath.split("/").pop() : ""
                                        property string bomResult: window.componentListController ? window.componentListController.bomResult : ""
                                        // 文件选择区域
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.minimumHeight: 100
                                            radius: AppStyle.radius.md
                                            color: {
                                                if (bomImportColumn.hasFile)
                                                    return AppStyle.isDarkMode ? Qt.rgba(34, 197, 94, 0.08) : Qt.rgba(34, 197, 94, 0.06);
                                                return AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.03) : Qt.rgba(0, 0, 0, 0.02);
                                            }
                                            border.width: bomImportColumn.hasFile ? 2 : 0
                                            border.color: bomImportColumn.hasFile ? AppStyle.colors.success : "transparent"
                                            Behavior on border.color {
                                                ColorAnimation {
                                                    duration: AppStyle.durations.fast
                                                }
                                            }
                                            Behavior on color {
                                                ColorAnimation {
                                                    duration: AppStyle.durations.fast
                                                }
                                            }

                                            // 虚线边框（未选择文件时显示）
                                            Canvas {
                                                id: dashBorder
                                                anchors.fill: parent
                                                visible: !bomImportColumn.hasFile
                                                opacity: bomDropArea.containsMouse ? 0.8 : 0.4
                                                onPaint: {
                                                    var ctx = getContext("2d");
                                                    ctx.clearRect(0, 0, width, height);
                                                    ctx.strokeStyle = bomDropArea.containsMouse ? AppStyle.colors.primary : AppStyle.colors.textSecondary;
                                                    ctx.lineWidth = 1.5;
                                                    ctx.setLineDash([6, 4]);
                                                    var r = AppStyle.radius.md;
                                                    ctx.beginPath();
                                                    ctx.roundedRect(1, 1, width - 2, height - 2, r, r);
                                                    ctx.stroke();
                                                }
                                                Connections {
                                                    target: bomDropArea
                                                    function onContainsMouseChanged() {
                                                        dashBorder.requestPaint();
                                                    }
                                                }
                                            }

                                            ColumnLayout {
                                                anchors.centerIn: parent
                                                spacing: AppStyle.spacing.sm
                                                // 文件图标
                                                Text {
                                                    Layout.alignment: Qt.AlignHCenter
                                                    text: bomImportColumn.hasFile ? "📄" : "📁"
                                                    font.pixelSize: 28
                                                }
                                                // 文件名或提示文字
                                                Text {
                                                    Layout.alignment: Qt.AlignHCenter
                                                    text: {
                                                        if (bomImportColumn.hasFile)
                                                            return bomImportColumn.fileName;
                                                        return qsTranslate("MainWindow", "点击选择 BOM 文件");
                                                    }
                                                    font.pixelSize: AppStyle.fontSizes.sm
                                                    font.bold: bomImportColumn.hasFile
                                                    color: bomImportColumn.hasFile ? AppStyle.colors.textPrimary : AppStyle.colors.textSecondary
                                                    elide: Text.ElideMiddle
                                                    Layout.maximumWidth: parent.parent.width - AppStyle.spacing.xl * 2
                                                }
                                                // 解析结果
                                                Text {
                                                    Layout.alignment: Qt.AlignHCenter
                                                    text: {
                                                        if (bomImportColumn.bomResult && bomImportColumn.bomResult.length > 0)
                                                            return bomImportColumn.bomResult;
                                                        if (bomImportColumn.hasFile)
                                                            return qsTranslate("MainWindow", "文件已就绪");
                                                        return qsTranslate("MainWindow", "支持格式: .xlsx, .csv, .txt");
                                                    }
                                                    font.pixelSize: AppStyle.fontSizes.xs
                                                    color: {
                                                        if (bomImportColumn.bomResult && bomImportColumn.bomResult.length > 0)
                                                            return AppStyle.colors.success;
                                                        if (bomImportColumn.hasFile)
                                                            return AppStyle.colors.success;
                                                        return AppStyle.colors.textSecondary;
                                                    }
                                                    opacity: 0.8
                                                }
                                            }

                                            MouseArea {
                                                id: bomDropArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: bomFileDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 元件列表卡片
                        // 元件列表卡片
                        ComponentListCard {
                            id: componentListCardItem
                            Layout.fillWidth: true
                            componentListController: window.componentListController
                            exportProgressController: window.exportProgressController
                            isExporting: window.exportProgressController ? window.exportProgressController.isExporting : false
                        }

                        // ==================== 工作区内容（始终可见） ====================
                        // 转换进度
                        ExportProgressCard {
                            Layout.fillWidth: true
                            exportProgressController: window.exportProgressController
                            visible: (window.exportProgressController && window.exportProgressController.isExporting) || (window.exportProgressController && window.exportProgressController.progress > 0)
                        }

                        // 转换结果
                        ExportResultsCard {
                            Layout.fillWidth: true
                            exportProgressController: window.exportProgressController
                        }

                        // 导出统计
                        ExportStatisticsCard {
                            Layout.fillWidth: true
                            exportProgressController: window.exportProgressController
                            exportSettingsController: window.exportSettingsController
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

    // 底部进度指示条（2px 全局进度，z: -1 避免遮挡 resize 手柄）
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        z: -1
        color: "transparent"
        visible: window.exportProgressController && (window.exportProgressController.isExporting || window.exportProgressController.progress > 0)
        Rectangle {
            width: parent.width * ((window.exportProgressController ? window.exportProgressController.progress : 0) / 100)
            height: parent.height
            color: {
                var pc = window.exportProgressController;
                if (pc && pc.isExporting)
                    return AppStyle.colors.warning;
                if (pc && pc.failureCount > 0)
                    return AppStyle.colors.danger;
                return AppStyle.colors.success;
            }
            Behavior on width {
                NumberAnimation {
                    duration: 150
                }
            }
            Behavior on color {
                ColorAnimation {
                    duration: 300
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
