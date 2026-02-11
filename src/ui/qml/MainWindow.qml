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
    // 连接到 ViewModel
    property var componentListController: componentListViewModel
    property var exportSettingsController: exportSettingsViewModel
    property var exportProgressController: exportProgressViewModel
    property var themeController: themeSettingsViewModel

    // 窗口状态属性
    readonly property bool isMaximized: Window.window ? (Window.window.visibility === Window.Maximized || Window.window.visibility === Window.FullScreen) : false
    readonly property int windowRadius: isMaximized ? 0 : AppStyle.radius.lg

    // 绑定 AppStyle.isDarkMode 到 themeSettingsViewModel.isDarkMode
    Binding {
        target: AppStyle
        property: "isDarkMode"
        value: themeSettingsViewModel.isDarkMode
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
            // 从 URL 中提取本地路径
            var path = selectedFolder.toString();
            if (path.startsWith("file:///")) {
                path = path.substring(8); // 移除 "file:///" 前缀
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
        Rectangle {
            id: titleBar
            width: parent.width
            height: 38 // Slightly taller for better touch/click targets
            color: AppStyle.colors.surface
            topLeftRadius: windowRadius
            topRightRadius: windowRadius
            z: 1000 // 确保在最顶层

            // Bottom separator line
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: AppStyle.colors.border
            }

            // 拖动区域
            MouseArea {
                anchors.fill: parent
                property point clickPos: "0,0"
                onPressed: mouse => {
                    clickPos = Qt.point(mouse.x, mouse.y);
                    if (Window.window.visibility === Window.Maximized) {
                        var ratio = mouse.x / width;
                        Window.window.showNormal();
                        // Re-center window horizontally relative to mouse
                        // We need to estimate where the mouse is globally or relative to the restored window
                        // Since startSystemMove grabs it, we just need to set 'x' roughly correct.
                        // A simple approximation is to center it or keep the ratio.
                        Window.window.x = mouse.screenX - (Window.window.width * ratio);
                        Window.window.y = mouse.screenY - (mouse.y);
                    }
                    Window.window.startSystemMove();
                }
                onDoubleClicked: {
                    if (Window.window.visibility === Window.Maximized) {
                        Window.window.showNormal();
                    } else {
                        Window.window.showMaximized();
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // 图标
                Image {
                    source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.leftMargin: 10
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }

                // 标题
                Text {
                    text: "EasyKiConverter"
                    color: AppStyle.colors.textPrimary
                    font.pixelSize: 13 // Slightly larger
                    font.bold: true
                    Layout.leftMargin: 12
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                }

                // 窗口控制按钮
                Row {
                    Layout.alignment: Qt.AlignRight

                    // 最小化
                    Button {
                        width: 46
                        height: 38
                        flat: true

                        icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/minimize.svg"
                        icon.color: "transparent" // Use original SVG colors
                        icon.width: 10
                        icon.height: 10

                        background: Rectangle {
                            color: parent.hovered ? (AppStyle.isDarkMode ? "#1affffff" : "#1a000000") : "transparent"
                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }
                            }
                        }

                        onClicked: Window.window.showMinimized()
                    }

                    // 最大化/还原
                    Button {
                        width: 46
                        height: 38
                        flat: true

                        icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/maximize.svg"
                        icon.color: "transparent" // Use original SVG colors
                        icon.width: 10
                        icon.height: 10

                        background: Rectangle {
                            color: parent.hovered ? (AppStyle.isDarkMode ? "#1affffff" : "#1a000000") : "transparent"
                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }
                            }
                        }

                        onClicked: {
                            if (Window.window.visibility === Window.Maximized) {
                                Window.window.showNormal();
                            } else {
                                Window.window.showMaximized();
                            }
                        }
                    }

                    // 关闭
                    Button {
                        width: 46
                        height: 38
                        flat: true

                        icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/close.svg"
                        icon.color: hovered ? "white" : "transparent" // White on hover, otherwise original color
                        icon.width: 10
                        icon.height: 10

                        background: Rectangle {
                            color: parent.hovered ? "#c42b1c" : "transparent"
                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }
                            }
                        }

                        onClicked: Window.window.close()
                    }
                }
            }
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

                    // 欢迎标题
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 30
                        text: "EasyKiConverter"
                        font.pixelSize: 48
                        font.bold: true
                        color: AppStyle.colors.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("将嘉立创EDA元器件转换为KiCad格式")
                        font.pixelSize: 18
                        color: AppStyle.isDarkMode ? AppStyle.colors.textPrimary : AppStyle.colors.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                    }
                    // 深色模式切换按钮和 GitHub 图标
                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        spacing: AppStyle.spacing.sm
                        z: 10  // 确保在其他元素之上

                        // 语言选择器
                        ComboBox {
                            id: languageComboBox
                            Layout.preferredWidth: 120
                            model: [
                                {
                                    text: "简体中文",
                                    value: "zh_CN"
                                },
                                {
                                    text: "English",
                                    value: "en"
                                }
                            ]
                            textRole: "text"
                            valueRole: "value"

                            property string savedLanguage: ""

                            Component.onCompleted: {
                                // 设置初始语言
                                var currentLang = LanguageManager.currentLanguage;
                                if (currentLang === "auto") {
                                    // 如果是自动/跟随系统，解析出实际语言并选中
                                    var systemLang = LanguageManager.detectSystemLanguage();
                                    currentIndex = indexOfValue(systemLang);
                                } else {
                                    currentIndex = indexOfValue(currentLang);
                                }
                                savedLanguage = currentValue;
                            }

                            onActivated: function (index) {
                                const langValue = currentValue;
                                savedLanguage = langValue;
                                LanguageManager.setLanguage(langValue);
                            }

                            background: Rectangle {
                                color: AppStyle.colors.surface
                                border.color: languageComboBox.hovered ? AppStyle.colors.borderFocus : AppStyle.colors.border
                                border.width: 1
                                radius: AppStyle.radius.md
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: 150
                                    }
                                }
                            }

                            contentItem: Text {
                                text: languageComboBox.displayText
                                font.pixelSize: 12
                                color: AppStyle.colors.textPrimary
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 8
                                rightPadding: 8
                            }

                            indicator: Canvas {
                                id: canvas
                                x: languageComboBox.width - width - languageComboBox.rightPadding
                                y: languageComboBox.topPadding + (languageComboBox.availableHeight - height) / 2
                                width: 12
                                height: 8
                                contextType: "2d"

                                Connections {
                                    target: languageComboBox
                                    function onPressedChanged() {
                                        canvas.requestPaint();
                                    }
                                }

                                onPaint: {
                                    context.reset();
                                    context.moveTo(0, 0);
                                    context.lineTo(width / 2, height);
                                    context.lineTo(width, 0);
                                    context.closePath();
                                    context.fillStyle = AppStyle.colors.textSecondary;
                                    context.fill();
                                }
                            }

                            delegate: ItemDelegate {
                                width: languageComboBox.width
                                height: 36

                                contentItem: Text {
                                    text: modelData.text
                                    font.pixelSize: 12
                                    color: AppStyle.colors.textPrimary
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 8
                                }

                                background: Rectangle {
                                    color: parent.highlighted ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                                }
                            }

                            popup: Popup {
                                y: languageComboBox.height - 1
                                width: languageComboBox.width
                                implicitHeight: listview.contentHeight
                                padding: 0

                                onClosed: {
                                    // 当关闭下拉框时，恢复到保存的语言
                                    languageComboBox.currentIndex = languageComboBox.indexOfValue(languageComboBox.savedLanguage);
                                }

                                contentItem: ListView {
                                    id: listview
                                    clip: true
                                    model: languageComboBox.popup.visible ? languageComboBox.delegateModel : null
                                    currentIndex: languageComboBox.highlightedIndex
                                    ScrollBar.vertical: ScrollBar {
                                        policy: ScrollBar.AsNeeded
                                    }

                                    delegate: ItemDelegate {
                                        width: languageComboBox.width
                                        height: 36

                                        contentItem: Text {
                                            text: modelData.text
                                            font.pixelSize: 12
                                            color: AppStyle.colors.textPrimary
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: 8
                                        }

                                        background: Rectangle {
                                            color: parent.hovered ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                                        }
                                    }
                                }

                                background: Rectangle {
                                    color: AppStyle.colors.surface
                                    border.color: AppStyle.colors.border
                                    border.width: 1
                                    radius: AppStyle.radius.md
                                }
                            }
                        }

                        // GitHub 图标按钮
                        MouseArea {
                            id: githubButton
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            z: 100  // 确保在其他元素之上
                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: githubButton.containsMouse ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                                Behavior on color {
                                    ColorAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                            }
                            // 发光效果
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width + 10
                                height: parent.height + 10
                                radius: 12
                                color: AppStyle.isDarkMode ? "#ffffff" : "#000000"
                                opacity: githubButton.containsMouse ? 0.15 : 0.0
                                scale: githubButton.containsMouse ? 1.3 : 0.8
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: AppStyle.durations.normal
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                            }
                            Image {
                                anchors.centerIn: parent
                                width: 22
                                height: 22
                                source: AppStyle.isDarkMode ? "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark-white.svg" : "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark.svg"
                                fillMode: Image.PreserveAspectFit
                                opacity: githubButton.pressed ? 0.7 : (githubButton.containsMouse ? 1.0 : 0.8)
                                scale: githubButton.containsMouse ? 1.2 : 1.0
                                rotation: githubButton.containsMouse ? 8 : 0
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                                Behavior on rotation {
                                    NumberAnimation {
                                        duration: AppStyle.durations.normal
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                            }
                            onClicked: {
                                Qt.openUrlExternally("https://github.com/tangsangsimida/EasyKiConverter_QT");
                            }
                        }
                        // 深色模式切换按钮（灯泡图标）
                        MouseArea {
                            id: themeSwitchButton
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            z: 100  // 确保在其他元素之上
                            Rectangle {
                                id: themeSwitchBackground
                                anchors.fill: parent
                                radius: 8
                                color: themeSwitchButton.containsMouse ? (AppStyle.isDarkMode ? "#334155" : "#e2e8f0") : "transparent"
                                Behavior on color {
                                    ColorAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                            }
                            // 发光效果
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width + 10
                                height: parent.height + 10
                                radius: 12
                                color: AppStyle.isDarkMode ? "#fbbf24" : "#3b82f6"
                                opacity: themeSwitchButton.containsMouse ? 0.2 : 0.0
                                scale: themeSwitchButton.containsMouse ? 1.3 : 0.8
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: AppStyle.durations.normal
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                            }
                            Image {
                                id: themeSwitchIcon
                                anchors.centerIn: parent
                                width: 22
                                height: 22
                                source: AppStyle.isDarkMode ? "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Grey_light_bulb.svg" : "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Blue_light_bulb.svg"
                                fillMode: Image.PreserveAspectFit
                                opacity: themeSwitchButton.pressed ? 0.7 : (themeSwitchButton.containsMouse ? 1.0 : 0.85)
                                scale: themeSwitchButton.containsMouse ? 1.2 : 1.0
                                rotation: themeSwitchButton.containsMouse ? -12 : 0
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: AppStyle.durations.fast
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                                Behavior on rotation {
                                    NumberAnimation {
                                        duration: AppStyle.durations.normal
                                        easing.type: AppStyle.easings.easeOut
                                    }
                                }
                                Behavior on source {
                                    PropertyAnimation {
                                        duration: 0
                                    }
                                }
                            }
                            onClicked: {
                                themeController.setDarkMode(!AppStyle.isDarkMode);
                            }
                        }
                    }
                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: AppStyle.colors.border
                    }
                    // 元件输入卡片
                    Card {
                        Layout.fillWidth: true
                        title: qsTr("添加元器件")
                        RowLayout {
                            width: parent.width
                            spacing: 12
                            TextField {
                                id: componentInput
                                Layout.fillWidth: true
                                placeholderText: qsTr("输入LCSC元件编号 (例如: C2040)")
                                font.pixelSize: AppStyle.fontSizes.md
                                color: AppStyle.colors.textPrimary
                                placeholderTextColor: AppStyle.colors.textSecondary
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
                                        componentListController.addComponent(componentInput.text);
                                        componentInput.text = ""; // Clear input after adding
                                    }
                                }
                            }
                            ModernButton {
                                text: qsTr("添加")
                                iconName: "add"
                                enabled: componentInput.text.length > 0
                                onClicked: {
                                    if (componentInput.text.length > 0) {
                                        componentListController.addComponent(componentInput.text);
                                        componentInput.text = "";
                                    }
                                }
                            }
                            ModernButton {
                                text: qsTr("粘贴")
                                iconName: "folder"
                                backgroundColor: AppStyle.colors.textSecondary
                                hoverColor: AppStyle.colors.textPrimary
                                pressedColor: AppStyle.colors.textPrimary
                                onClicked: {
                                    componentListController.pasteFromClipboard();
                                }
                            }
                        }
                    }
                    // BOM导入卡片
                    Card {
                        Layout.fillWidth: true
                        title: qsTr("导入BOM文件")
                        RowLayout {
                            width: parent.width
                            spacing: 12
                            ModernButton {
                                text: qsTr("选择BOM文件")
                                iconName: "upload"
                                backgroundColor: AppStyle.colors.success
                                hoverColor: AppStyle.colors.successDark
                                pressedColor: AppStyle.colors.successDark
                                onClicked: {
                                    bomFileDialog.open();
                                }
                            }
                            Text {
                                id: bomFileLabel
                                Layout.fillWidth: true
                                text: componentListController.bomFilePath.length > 0 ? componentListController.bomFilePath.split("/").pop() : qsTr("未选择文件")
                                font.pixelSize: AppStyle.fontSizes.sm
                                color: AppStyle.colors.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideMiddle
                            }
                        }
                        // BOM导入结果
                        Text {
                            id: bomResultLabel
                            Layout.fillWidth: true
                            Layout.topMargin: AppStyle.spacing.md
                            text: componentListController.bomResult
                            font.pixelSize: AppStyle.fontSizes.sm
                            color: AppStyle.colors.success
                            horizontalAlignment: Text.AlignHCenter
                            visible: componentListController.bomResult.length > 0
                        }
                    }
                    // 元件列表卡片
                    Card {
                        id: componentListCard
                        Layout.fillWidth: true
                        title: qsTr("元器件列表")

                        // 默认折叠，只有在有元器件时才展开
                        isCollapsed: componentListController.componentCount === 0

                        resources: [
                            // 监听组件数量变化，自动展开
                            Connections {
                                target: componentListController
                                function onComponentCountChanged() {
                                    if (componentListController.componentCount > 0) {
                                        componentListCard.isCollapsed = false;
                                    }
                                }
                            },
                            // 搜索过滤模型 (作为资源定义，不参与布局)
                            DelegateModel {
                                id: visualModel
                                model: componentListController

                                groups: [
                                    DelegateModelGroup {
                                        id: displayGroup
                                        includeByDefault: true
                                        name: "display"
                                    },
                                    DelegateModelGroup {
                                        id: filterGroup
                                        name: "filter"
                                    }
                                ]

                                filterOnGroup: "display"

                                delegate: ComponentListItem {
                                    width: componentList.cellWidth - AppStyle.spacing.md
                                    anchors.horizontalCenter: parent ? undefined : undefined

                                    // 绑定数据和搜索词
                                    // 注意：QAbstractListModel 暴露的角色名为 "itemData"
                                    itemData: model.itemData
                                    searchText: searchInput.text // 传递搜索词用于高亮

                                    onDeleteClicked: {
                                        if (itemData) {
                                            componentListController.removeComponentById(itemData.componentId);
                                        }
                                    }
                                }

                                // 过滤函数
                                function updateFilter() {
                                    // 移除所有空格，实现更宽容的搜索 (例如 "C 2040" -> "c2040")
                                    var searchTerm = searchInput.text.toLowerCase().replace(/\s+/g, '');

                                    // 遍历所有项进行处理
                                    for (var i = 0; i < items.count; i++) {
                                        var item = items.get(i);

                                        // 如果搜索词为空，显示所有项
                                        if (searchTerm === "") {
                                            item.inDisplay = true;
                                            continue;
                                        }

                                        // 获取内容
                                        // 对于 QAbstractListModel，item.model 包含角色属性
                                        var dataObj = item.model.itemData;

                                        // 获取 ID
                                        var idStr = "";
                                        if (dataObj && dataObj.componentId !== undefined) {
                                            idStr = dataObj.componentId;
                                        }

                                        // 判断是否匹配
                                        if (idStr.toLowerCase().indexOf(searchTerm) !== -1) {
                                            item.inDisplay = true;
                                        } else {
                                            item.inDisplay = false;
                                        }
                                    }
                                }
                            }
                        ]

                        RowLayout {
                            width: parent.width
                            spacing: 12
                            Text {
                                id: componentCountLabel
                                text: qsTr("共 %1 个元器件").arg(componentListController.componentCount)
                                font.pixelSize: 14
                                color: AppStyle.colors.textSecondary
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            // 搜索框

                            TextField {
                                id: searchInput

                                Layout.preferredWidth: 200

                                Layout.preferredHeight: 44

                                placeholderText: qsTr("搜索元器件...")

                                font.pixelSize: AppStyle.fontSizes.sm

                                color: AppStyle.colors.textPrimary

                                placeholderTextColor: AppStyle.colors.textSecondary

                                leftPadding: 32 // 为图标留出空间

                                background: Rectangle {

                                    color: AppStyle.colors.surface

                                    border.color: searchInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border

                                    border.width: searchInput.focus ? 2 : 1

                                    radius: AppStyle.radius.md
                                }

                                // 搜索图标

                                Image {

                                    anchors.left: parent.left

                                    anchors.leftMargin: 8

                                    anchors.verticalCenter: parent.verticalCenter

                                    width: 16

                                    height: 16

                                    source: AppStyle.isDarkMode ? "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark-white.svg" : // 暂时用现有图标替代，或者用文字

                                    "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark.svg"

                                    // 注意：实际上应该用一个 'search' 图标，这里暂时复用或忽略，

                                    // 为了美观，用 Text 替代

                                    visible: false
                                }

                                Text {

                                    anchors.left: parent.left

                                    anchors.leftMargin: 10

                                    anchors.verticalCenter: parent.verticalCenter

                                    text: ""

                                    font.pixelSize: 12

                                    color: AppStyle.colors.textSecondary
                                }

                                onTextChanged: {
                                    visualModel.updateFilter();
                                }
                            }

                            // 复制所有编号按钮

                            Item {
                                id: copyAllButton

                                Layout.preferredWidth: copyAllButtonContent.width + AppStyle.spacing.xl * 2

                                Layout.preferredHeight: 44

                                Layout.alignment: Qt.AlignVCenter

                                // 按钮背景
                                Rectangle {
                                    anchors.fill: parent
                                    color: copyAllButtonMouseArea.pressed ? AppStyle.colors.textPrimary : copyAllButtonMouseArea.containsMouse ? Qt.darker(AppStyle.colors.textSecondary, 1.2) : AppStyle.colors.textSecondary
                                    radius: AppStyle.radius.md
                                    enabled: componentListController.componentCount > 0
                                    opacity: componentListController.componentCount > 0 ? 1.0 : 0.4
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: AppStyle.durations.fast
                                        }
                                    }
                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: AppStyle.durations.fast
                                        }
                                    }
                                }

                                // 按钮文本
                                Text {
                                    id: copyAllButtonContent
                                    anchors.centerIn: parent
                                    text: qsTr("复制所有编号")
                                    font.pixelSize: AppStyle.fontSizes.sm
                                    color: "white"
                                }

                                // 鼠标区域
                                MouseArea {
                                    id: copyAllButtonMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: componentListController.componentCount > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        if (componentListController.componentCount > 0) {
                                            componentListController.copyAllComponentIds();
                                            copyAllFeedback.visible = true;
                                        }
                                    }
                                }

                                // 复制成功提示
                                ToolTip {
                                    id: copyAllFeedback
                                    parent: copyAllButton
                                    x: (parent.width - width) / 2
                                    y: -35
                                    text: qsTr("已复制所有编号")
                                    delay: 0
                                    timeout: 1500
                                }
                            }

                            ModernButton {
                                text: qsTr("清空列表")
                                iconName: "trash"
                                font.pixelSize: AppStyle.fontSizes.sm
                                backgroundColor: AppStyle.colors.danger
                                hoverColor: AppStyle.colors.dangerDark
                                pressedColor: AppStyle.colors.dangerDark
                                onClicked: {
                                    searchInput.text = ""; // 清空搜索
                                    componentListController.clearComponentList();
                                    exportProgressController.resetExport(); // 重置导出状态
                                }
                            }
                        }
                        // 元件列表视图（自适应网格）
                        GridView {
                            id: componentList
                            Layout.fillWidth: true
                            Layout.preferredHeight: 300
                            Layout.topMargin: AppStyle.spacing.md
                            clip: true

                            // 动态计算列宽，实现响应式布局
                            property int minCellWidth: 230
                            property int availableWidth: width - AppStyle.spacing.md // 减去右侧滚动条/边距空间
                            property int columns: Math.max(1, Math.floor(availableWidth / minCellWidth))

                            cellWidth: Math.floor(availableWidth / columns)
                            // 卡片高度 64 + 垂直间距 12 = 76
                            cellHeight: 76

                            flow: GridView.FlowLeftToRight
                            layoutDirection: Qt.LeftToRight

                            // 使用 DelegateModel
                            model: visualModel

                            // delegate 已经在 DelegateModel 中定义了，这里不需要再定义，
                            // 但是 GridView 需要直接使用 visualModel 作为 model。
                            // 注意：当 model 是 DelegateModel 时，不需要指定 delegate 属性，
                            // 因为 DelegateModel 已经包含了 delegate。

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }
                            // 添加列表项进入动画 (DelegateModel 管理时可能需要调整)
                            // 简单的 add/remove 动画在使用 DelegateModel 时可能不生效或表现不同
                        }
                    }

                    // 导出设置卡片 (合并后的)
                    Card {
                        Layout.fillWidth: true
                        title: qsTr("导出设置")
                        GridLayout {
                            width: parent.width
                            columns: 2
                            columnSpacing: 20
                            rowSpacing: 12
                            // 输出路径
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: qsTr("输出路径")
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: AppStyle.colors.textPrimary
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    TextField {
                                        id: outputPathInput
                                        Layout.fillWidth: true
                                        text: exportSettingsController.outputPath
                                        onTextChanged: exportSettingsController.setOutputPath(text)
                                        placeholderText: qsTr("选择输出目录")
                                        font.pixelSize: 14
                                        color: AppStyle.colors.textPrimary
                                        placeholderTextColor: AppStyle.colors.textSecondary
                                        background: Rectangle {
                                            color: AppStyle.colors.surface
                                            border.color: outputPathInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                                            border.width: outputPathInput.focus ? 2 : 1
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
                                    }
                                    ModernButton {
                                        text: qsTr("浏览")
                                        iconName: "folder"
                                        font.pixelSize: AppStyle.fontSizes.sm
                                        backgroundColor: AppStyle.colors.textSecondary
                                        hoverColor: AppStyle.colors.textPrimary
                                        pressedColor: AppStyle.colors.textPrimary
                                        onClicked: {
                                            outputFolderDialog.open();
                                        }
                                    }
                                }
                            }
                            // 库名称
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: qsTr("库名称")
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: AppStyle.colors.textPrimary
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                TextField {
                                    id: libNameInput
                                    Layout.fillWidth: true
                                    text: exportSettingsController.libName
                                    onTextChanged: exportSettingsController.setLibName(text)
                                    placeholderText: qsTr("输入库名称 (例如: MyLibrary)")
                                    font.pixelSize: 14
                                    color: AppStyle.colors.textPrimary
                                    placeholderTextColor: AppStyle.colors.textSecondary
                                    background: Rectangle {
                                        color: AppStyle.colors.surface
                                        border.color: libNameInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                                        border.width: libNameInput.focus ? 2 : 1
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
                                }
                            }
                        }

                        // 分隔
                        Item {
                            Layout.preferredHeight: 10
                            Layout.fillWidth: true
                        }

                        // 原导出选项内容
                        RowLayout {
                            Layout.fillWidth: true
                            width: parent.width
                            spacing: 20
                            // 符号库选项
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 100
                                spacing: 8
                                CheckBox {
                                    id: symbolCheckbox
                                    Layout.fillWidth: true
                                    text: qsTr("符号库")
                                    checked: exportSettingsController.exportSymbol
                                    onCheckedChanged: exportSettingsController.setExportSymbol(checked)
                                    font.pixelSize: 16

                                    indicator: Rectangle {
                                        implicitWidth: 22
                                        implicitHeight: 22
                                        x: symbolCheckbox.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: symbolCheckbox.checked ? AppStyle.colors.primary : "transparent"
                                        border.color: symbolCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                        border.width: 1.5

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
                                            font.pixelSize: 14
                                            color: "#ffffff"
                                            visible: symbolCheckbox.checked
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
                            }
                            // 封装库选项
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 100
                                spacing: 8
                                CheckBox {
                                    id: footprintCheckbox
                                    Layout.fillWidth: true
                                    text: qsTr("封装库")
                                    checked: exportSettingsController.exportFootprint
                                    onCheckedChanged: exportSettingsController.setExportFootprint(checked)
                                    font.pixelSize: 16

                                    indicator: Rectangle {
                                        implicitWidth: 22
                                        implicitHeight: 22
                                        x: footprintCheckbox.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: footprintCheckbox.checked ? AppStyle.colors.primary : "transparent"
                                        border.color: footprintCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                        border.width: 1.5

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
                                            font.pixelSize: 14
                                            color: "#ffffff"
                                            visible: footprintCheckbox.checked
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
                            }
                            // 3D模型选项
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 100
                                spacing: 8
                                CheckBox {
                                    id: model3dCheckbox
                                    Layout.fillWidth: true
                                    text: qsTr("3D模型")
                                    checked: exportSettingsController.exportModel3D
                                    onCheckedChanged: exportSettingsController.setExportModel3D(checked)
                                    font.pixelSize: 16

                                    indicator: Rectangle {
                                        implicitWidth: 22
                                        implicitHeight: 22
                                        x: model3dCheckbox.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: model3dCheckbox.checked ? AppStyle.colors.primary : "transparent"
                                        border.color: model3dCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                        border.width: 1.5

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
                                            font.pixelSize: 14
                                            color: "#ffffff"
                                            visible: model3dCheckbox.checked
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
                            }
                            // 导出模式选项
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 100
                                spacing: 8
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("导出模式")
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: AppStyle.colors.textPrimary
                                    horizontalAlignment: Text.AlignLeft
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    // 追加模式
                                    RowLayout {
                                        spacing: 8
                                        RadioButton {
                                            id: appendModeRadio
                                            text: qsTr("追加")
                                            checked: exportSettingsController.exportMode === 0
                                            onCheckedChanged: {
                                                if (checked) {
                                                    exportSettingsController.setExportMode(0);
                                                }
                                            }
                                            font.pixelSize: 14

                                            indicator: Rectangle {
                                                implicitWidth: 20
                                                implicitHeight: 20
                                                x: appendModeRadio.leftPadding
                                                y: parent.height / 2 - height / 2
                                                radius: 10
                                                color: "transparent"
                                                border.color: appendModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                                border.width: 1.5

                                                Rectangle {
                                                    anchors.centerIn: parent
                                                    width: 10
                                                    height: 10
                                                    radius: 5
                                                    color: AppStyle.colors.primary
                                                    visible: appendModeRadio.checked
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
                                            text: qsTr("保留已存在的元器件")
                                            font.pixelSize: 11
                                            color: AppStyle.colors.textSecondary
                                        }
                                    }
                                    // 更新模式
                                    RowLayout {
                                        spacing: 8
                                        RadioButton {
                                            id: updateModeRadio
                                            text: qsTr("更新")
                                            checked: exportSettingsController.exportMode === 1
                                            onCheckedChanged: {
                                                if (checked) {
                                                    exportSettingsController.setExportMode(1);
                                                }
                                            }
                                            font.pixelSize: 14

                                            indicator: Rectangle {
                                                implicitWidth: 20
                                                implicitHeight: 20
                                                x: updateModeRadio.leftPadding
                                                y: parent.height / 2 - height / 2
                                                radius: 10
                                                color: "transparent"
                                                border.color: updateModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                                border.width: 1.5

                                                Rectangle {
                                                    anchors.centerIn: parent
                                                    width: 10
                                                    height: 10
                                                    radius: 5
                                                    color: AppStyle.colors.primary
                                                    visible: updateModeRadio.checked
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
                                            text: qsTr("覆盖已存在的元器件")
                                            font.pixelSize: 11
                                            color: AppStyle.colors.textSecondary
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 进度显示卡片
                    Card {
                        Layout.fillWidth: true
                        title: qsTr("转换进度")
                        visible: exportProgressController.isExporting || exportProgressController.progress > 0
                        ColumnLayout {
                            width: parent.width
                            spacing: 12

                            // 1. 流程指示器 (Step Indicators)
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 8
                                Layout.bottomMargin: 8
                                spacing: 0

                                // 步骤 1: 抓取
                                StepItem {
                                    // 移除 Layout.fillWidth，让它保持最小宽度
                                    Layout.preferredWidth: implicitWidth
                                    label: qsTr("数据抓取")
                                    index: 1
                                    progress: exportProgressController.fetchProgress
                                    activeColor: "#22c55e" // 绿色
                                }

                                // 连接线 1-2
                                Rectangle {
                                    Layout.fillWidth: true // 让线条占据所有剩余空间
                                    Layout.preferredHeight: 2
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.bottomMargin: 14
                                    color: exportProgressController.fetchProgress >= 100 ? AppStyle.colors.success : AppStyle.colors.border

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 300
                                        }
                                    }
                                }

                                // 步骤 2: 处理
                                StepItem {
                                    Layout.preferredWidth: implicitWidth
                                    label: qsTr("数据处理")
                                    index: 2
                                    progress: exportProgressController.processProgress
                                    activeColor: "#3b82f6" // 蓝色
                                }

                                // 连接线 2-3
                                Rectangle {
                                    Layout.fillWidth: true // 让线条占据所有剩余空间
                                    Layout.preferredHeight: 2
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.bottomMargin: 14
                                    color: exportProgressController.processProgress >= 100 ? AppStyle.colors.success : AppStyle.colors.border

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 300
                                        }
                                    }
                                }

                                // 步骤 3: 写入
                                StepItem {
                                    Layout.preferredWidth: implicitWidth
                                    label: qsTr("文件写入")
                                    index: 3
                                    progress: exportProgressController.writeProgress
                                    activeColor: "#f59e0b" // 橙色
                                }
                            }

                            // 2. 总进度 (多色拼接) - 改为水平布局
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                // 自定义多色进度条容器
                                Rectangle {
                                    id: progressBar
                                    Layout.fillWidth: true
                                    height: 12
                                    color: AppStyle.colors.border
                                    radius: AppStyle.radius.md
                                    clip: true
                                    // 移除 visible 限制，使其在完成后依然可见

                                    Row {
                                        anchors.fill: parent
                                        spacing: 0

                                        // 抓取部分 (Green, 占比 1/3)
                                        Rectangle {
                                            height: parent.height
                                            width: (parent.width / 3) * (exportProgressController.fetchProgress / 100)
                                            color: "#22c55e"
                                            visible: width > 0
                                            Behavior on width {
                                                NumberAnimation {
                                                    duration: 100
                                                }
                                            }
                                        }

                                        // 处理部分 (Blue, 占比 1/3)
                                        Rectangle {
                                            height: parent.height
                                            width: (parent.width / 3) * (exportProgressController.processProgress / 100)
                                            color: "#3b82f6"
                                            visible: width > 0
                                            Behavior on width {
                                                NumberAnimation {
                                                    duration: 100
                                                }
                                            }
                                        }

                                        // 写入部分 (Orange, 占比 1/3)
                                        Rectangle {
                                            height: parent.height
                                            width: (parent.width / 3) * (exportProgressController.writeProgress / 100)
                                            color: "#f59e0b"
                                            visible: width > 0
                                            Behavior on width {
                                                NumberAnimation {
                                                    duration: 100
                                                }
                                            }
                                        }
                                    }
                                }

                                // 总进度文字 (放在右侧)
                                Text {
                                    text: Math.round(exportProgressController.progress) + "%"
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: AppStyle.colors.textPrimary
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }

                            Text {
                                id: statusLabel
                                Layout.fillWidth: true
                                text: exportProgressController.status
                                font.pixelSize: 14
                                color: AppStyle.colors.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                visible: exportProgressController.status.length > 0
                            }
                        }
                    }
                    // 转换结果卡片（延迟加载）
                    Loader {
                        id: resultsLoader
                        Layout.fillWidth: true
                        active: exportProgressController.isExporting || exportProgressController.resultsList.length > 0
                        visible: active  // ✅ 确保 Loader 在没有结果时不占用空间
                        sourceComponent: Card {
                            title: qsTr("转换结果")
                            ColumnLayout {
                                id: resultsContent
                                width: parent.width
                                spacing: AppStyle.spacing.md
                                visible: true

                                // 结果列表（使用 GridView 实现五列显示）
                                GridView {
                                    id: resultsList
                                    Layout.fillWidth: true
                                    Layout.minimumHeight: 200
                                    Layout.preferredHeight: Math.min(resultsList.contentHeight + 20, 500)
                                    Layout.topMargin: AppStyle.spacing.md
                                    visible: exportProgressController.resultsList.length > 0
                                    clip: true
                                    cellWidth: {
                                        var w = width - AppStyle.spacing.md;
                                        var c = Math.max(1, Math.floor(w / 230));
                                        return w / c;
                                    }
                                    cellHeight: 80
                                    flow: GridView.FlowLeftToRight
                                    layoutDirection: Qt.LeftToRight
                                    model: exportProgressController.resultsList
                                    delegate: ResultListItem {
                                        width: resultsList.cellWidth - AppStyle.spacing.md
                                        anchors.horizontalCenter: parent ? undefined : undefined
                                        componentId: modelData.componentId || ""
                                        status: modelData.status || "pending"
                                        message: modelData.message || ""
                                        onRetryClicked: exportProgressController.retryComponent(componentId)
                                    }
                                    ScrollBar.vertical: ScrollBar {
                                        policy: ScrollBar.AsNeeded
                                    }
                                    // 添加列表项进入动画
                                    add: Transition {
                                        NumberAnimation {
                                            property: "opacity"
                                            from: 0
                                            to: 1
                                            duration: AppStyle.durations.normal
                                            easing.type: AppStyle.easings.easeOut
                                        }
                                        NumberAnimation {
                                            property: "scale"
                                            from: 0.8
                                            to: 1
                                            duration: AppStyle.durations.normal
                                            easing.type: AppStyle.easings.easeOut
                                        }
                                    }
                                    // 列表项移除动画
                                    remove: Transition {
                                        NumberAnimation {
                                            property: "opacity"
                                            from: 1
                                            to: 0
                                            duration: AppStyle.durations.normal
                                            easing.type: AppStyle.easings.easeOut
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // 导出统计卡片（仅在导出完成后显示）
                    Card {
                        Layout.fillWidth: true
                        title: qsTr("导出统计")
                        visible: exportProgressController.hasStatistics
                        ColumnLayout {
                            width: parent.width
                            spacing: AppStyle.spacing.md
                            // 基本统计信息
                            Text {
                                text: qsTr("基本统计")
                                font.pixelSize: AppStyle.fontSizes.md
                                font.bold: true
                                color: AppStyle.colors.textPrimary
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: AppStyle.spacing.lg
                                StatItem {
                                    label: qsTr("总数")
                                    value: exportProgressController.statisticsTotal
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("成功")
                                    value: exportProgressController.statisticsSuccess
                                    valueColor: AppStyle.colors.success
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("失败")
                                    value: exportProgressController.statisticsFailed
                                    valueColor: AppStyle.colors.danger
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("成功率")
                                    value: exportProgressController.statisticsSuccessRate.toFixed(2) + "%"
                                    Layout.fillWidth: true
                                }
                            }
                            // 时间统计信息
                            Text {
                                text: qsTr("时间统计")
                                font.pixelSize: AppStyle.fontSizes.md
                                font.bold: true
                                color: AppStyle.colors.textPrimary
                                Layout.topMargin: AppStyle.spacing.sm
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: AppStyle.spacing.lg
                                StatItem {
                                    label: qsTr("总耗时")
                                    value: (exportProgressController.statisticsTotalDuration / 1000).toFixed(2) + "s"
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("平均抓取")
                                    value: exportProgressController.statisticsAvgFetchTime + "ms"
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("平均处理")
                                    value: exportProgressController.statisticsAvgProcessTime + "ms"
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("平均写入")
                                    value: exportProgressController.statisticsAvgWriteTime + "ms"
                                    Layout.fillWidth: true
                                }
                            }
                            // 导出成果统计
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: AppStyle.spacing.lg
                                StatItem {
                                    label: qsTr("符号")
                                    value: exportProgressController.successSymbolCount
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("封装")
                                    value: exportProgressController.successFootprintCount
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("3D模型")
                                    value: exportProgressController.successModel3DCount
                                    Layout.fillWidth: true
                                }
                            }
                            // 网络统计信息
                            Text {
                                text: qsTr("网络统计")
                                font.pixelSize: AppStyle.fontSizes.md
                                font.bold: true
                                color: AppStyle.colors.textPrimary
                                Layout.topMargin: AppStyle.spacing.sm
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: AppStyle.spacing.lg
                                StatItem {
                                    label: qsTr("总请求数")
                                    value: exportProgressController.statisticsTotalNetworkRequests
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("重试次数")
                                    value: exportProgressController.statisticsTotalRetries
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("平均延迟")
                                    value: exportProgressController.statisticsAvgNetworkLatency + "ms"
                                    Layout.fillWidth: true
                                }
                                StatItem {
                                    label: qsTr("速率限制")
                                    value: exportProgressController.statisticsRateLimitHitCount
                                    Layout.fillWidth: true
                                }
                            }
                            // 底部按钮组（居中排列）
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: AppStyle.spacing.sm
                                spacing: AppStyle.spacing.lg

                                // 打开详细报告按钮（只在调试模式下显示）
                                ModernButton {
                                    text: qsTr("打开详细统计报告")
                                    iconName: "folder"
                                    backgroundColor: AppStyle.colors.surface
                                    textColor: AppStyle.colors.textPrimary
                                    hoverColor: AppStyle.colors.border
                                    pressedColor: AppStyle.colors.borderFocus
                                    visible: exportSettingsController.debugMode // 只在调试模式下显示

                                    onClicked: {
                                        Qt.openUrlExternally("file:///" + exportProgressController.statisticsReportPath);
                                    }
                                }
                            }
                        }
                    }
                    // 打开导出目录按钮（始终显示，只要导出已完成）
                    ModernButton {
                        Layout.fillWidth: true
                        Layout.topMargin: AppStyle.spacing.sm
                        text: qsTr("打开导出目录")
                        iconName: "folder"
                        backgroundColor: AppStyle.colors.primary
                        hoverColor: AppStyle.colors.primaryHover
                        pressedColor: AppStyle.colors.primaryPressed
                        visible: exportProgressController.statisticsTotal > 0 // 只要有导出过就显示

                        onClicked: {
                            Qt.openUrlExternally("file:///" + exportSettingsController.outputPath);
                        }
                    }
                    // 导出按钮组
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: AppStyle.spacing.md

                        // “开始转换”或“重试”按钮
                        ModernButton {
                            id: exportButton
                            Layout.preferredHeight: 56
                            Layout.fillWidth: true

                            // 根据是否有失败项来决定按钮文本
                            text: {
                                var lang = LanguageManager.currentLanguage; // Force update on language change
                                if (exportProgressController.isExporting)
                                    return qsTr("正在转换...");
                                if (exportProgressController.failureCount > 0)
                                    return qsTr("重试失败项");
                                return qsTr("开始转换");
                            }

                            iconName: exportProgressController.failureCount > 0 && !exportProgressController.isExporting ? "play" : "play"
                            font.pixelSize: AppStyle.fontSizes.xxl

                            backgroundColor: {
                                if (exportProgressController.isExporting)
                                    return AppStyle.colors.textDisabled;
                                if (exportProgressController.failureCount > 0)
                                    return AppStyle.colors.warning;
                                return AppStyle.colors.primary;
                            }
                            hoverColor: {
                                if (exportProgressController.failureCount > 0 && !exportProgressController.isExporting)
                                    return AppStyle.colors.warningDark;
                                return AppStyle.colors.primaryHover;
                            }
                            pressedColor: {
                                if (exportProgressController.failureCount > 0 && !exportProgressController.isExporting)
                                    return AppStyle.colors.warningDark;
                                return AppStyle.colors.primaryPressed;
                            }

                            // 导出进行中时禁用此按钮
                            enabled: !exportProgressController.isExporting && (componentListController.componentCount > 0 && (exportSettingsController.exportSymbol || exportSettingsController.exportFootprint || exportSettingsController.exportModel3D))

                            onClicked: {
                                if (exportProgressController.failureCount > 0) {
                                    exportProgressController.retryFailedComponents();
                                } else {
                                    // 提取 Component ID 列表
                                    var idList = componentListController.getAllComponentIds();

                                    exportProgressController.startExport(idList, exportSettingsController.outputPath, exportSettingsController.libName, exportSettingsController.exportSymbol, exportSettingsController.exportFootprint, exportSettingsController.exportModel3D, exportSettingsController.overwriteExistingFiles, exportSettingsController.exportMode === 1, exportSettingsController.debugMode);
                                }
                            }
                        }

                        // “停止转换”按钮
                        ModernButton {
                            id: stopButton
                            Layout.preferredHeight: 56
                            Layout.preferredWidth: 180

                            // 仅在导出进行时可见
                            visible: exportProgressController.isExporting

                            text: exportProgressController.isStopping ? qsTr("正在停止...") : qsTr("停止转换")
                            iconName: "close"
                            font.pixelSize: AppStyle.fontSizes.xl

                            backgroundColor: AppStyle.colors.danger
                            hoverColor: AppStyle.colors.dangerDark
                            pressedColor: AppStyle.colors.dangerDark

                            enabled: !exportProgressController.isStopping

                            onClicked: {
                                exportProgressController.cancelExport();
                            }
                        }
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
    // 仅在非最大化时启用
    Item {
        anchors.fill: parent
        z: 9999 // 确保在最顶层
        visible: !window.isMaximized

        // 边框拖拽宽度
        property int gripSize: 8

        // 左
        MouseArea {
            width: parent.gripSize
            height: parent.height - 2 * parent.gripSize
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            cursorShape: Qt.SizeHorCursor
            onPressed: Window.window.startSystemResize(Qt.LeftEdge)
        }
        // 右
        MouseArea {
            width: parent.gripSize
            height: parent.height - 2 * parent.gripSize
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            cursorShape: Qt.SizeHorCursor
            onPressed: Window.window.startSystemResize(Qt.RightEdge)
        }
        // 上
        MouseArea {
            width: parent.width - 2 * parent.gripSize
            height: parent.gripSize
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            cursorShape: Qt.SizeVerCursor
            onPressed: Window.window.startSystemResize(Qt.TopEdge)
        }
        // 下
        MouseArea {
            width: parent.width - 2 * parent.gripSize
            height: parent.gripSize
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            cursorShape: Qt.SizeVerCursor
            onPressed: Window.window.startSystemResize(Qt.BottomEdge)
        }
        // 左上
        MouseArea {
            width: parent.gripSize * 2
            height: parent.gripSize * 2
            anchors.left: parent.left
            anchors.top: parent.top
            cursorShape: Qt.SizeFDiagCursor
            onPressed: Window.window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
        }
        // 右上
        MouseArea {
            width: parent.gripSize * 2
            height: parent.gripSize * 2
            anchors.right: parent.right
            anchors.top: parent.top
            cursorShape: Qt.SizeBDiagCursor
            onPressed: Window.window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
        }
        // 左下
        MouseArea {
            width: parent.gripSize * 2
            height: parent.gripSize * 2
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeBDiagCursor
            onPressed: Window.window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
        }
        // 右下
        MouseArea {
            width: parent.gripSize * 2
            height: parent.gripSize * 2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor
            onPressed: Window.window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }
    }
}
