import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import QtQuick.Effects
import QtQml.Models
import "styles"
import "components"

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
        title: "选择 BOM 文件"
        nameFilters: ["Supported files (*.txt *.csv *.xlsx *.xls)", "Text files (*.txt)", "CSV files (*.csv)", "Excel files (*.xlsx *.xls)", "All files (*.*)"]
        onAccepted: {
            componentListController.selectBomFile(selectedFile)
        }
    }
    // 输出路径选择对话框
    FolderDialog {
        id: outputFolderDialog
        title: "选择输出目录"
        onAccepted: {
            // 从 URL 中提取本地路径
            var path = selectedFolder.toString()
            if (path.startsWith("file:///")) {
                path = path.substring(8) // 移除 "file:///" 前缀
            }
            exportSettingsController.setOutputPath(path)
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
        onStatusChanged: if (status === Image.Ready) backgroundCanvas.requestPaint()
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
            onPressed: (mouse) => {
                clickPos = Qt.point(mouse.x,mouse.y)
                if (Window.window.visibility === Window.Maximized) {
                    var ratio = mouse.x / width
                    Window.window.showNormal()
                    // Re-center window horizontally relative to mouse
                    // We need to estimate where the mouse is globally or relative to the restored window
                    // Since startSystemMove grabs it, we just need to set 'x' roughly correct.
                    // A simple approximation is to center it or keep the ratio.
                    Window.window.x = mouse.screenX - (Window.window.width * ratio)
                    Window.window.y = mouse.screenY - (mouse.y)
                }
                Window.window.startSystemMove()
            }
            onDoubleClicked: {
                if (Window.window.visibility === Window.Maximized) {
                    Window.window.showNormal()
                } else {
                    Window.window.showMaximized()
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
                        Behavior on color { ColorAnimation { duration: 150 } }
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
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                    
                    onClicked: {
                        if (Window.window.visibility === Window.Maximized) {
                            Window.window.showNormal()
                        } else {
                            Window.window.showMaximized()
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
                        Behavior on color { ColorAnimation { duration: 150 } }
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
                    text: "将嘉立创EDA元器件转换为KiCad格式"
                    font.pixelSize: 18
                    color: AppStyle.isDarkMode ? AppStyle.colors.textPrimary : AppStyle.colors.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                }
                // 深色模式切换按钮和 GitHub 图标
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: AppStyle.spacing.sm
                    z: 10  // 确保在其他元素之上
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
                            source: AppStyle.isDarkMode ?
                                    "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark-white.svg" :
                                    "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark.svg"
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
                            Qt.openUrlExternally("https://github.com/tangsangsimida/EasyKiConverter_QT")
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
                            source: AppStyle.isDarkMode ?
                                    "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Grey_light_bulb.svg" :
                                    "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Blue_light_bulb.svg"
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
                            themeController.setDarkMode(!AppStyle.isDarkMode)
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
                    title: "添加元器件"
                    RowLayout {
                        width: parent.width
                        spacing: 12
                        TextField {
                            id: componentInput
                            Layout.fillWidth: true
                            placeholderText: "输入LCSC元件编号 (例如: C2040)"
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
                                                            componentListController.addComponent(componentInput.text)
                                                            componentInput.text = "" // Clear input after adding
                                                        }
                                                    }                        }
                        ModernButton {
                            text: "添加"
                            iconName: "add"
                            enabled: componentInput.text.length > 0
                            onClicked: {
                                if (componentInput.text.length > 0) {
                                    componentListController.addComponent(componentInput.text)
                                    componentInput.text = ""
                                }
                            }
                        }
                        ModernButton {
                            text: "粘贴"
                            iconName: "folder"
                            backgroundColor: AppStyle.colors.textSecondary
                            hoverColor: AppStyle.colors.textPrimary
                            pressedColor: AppStyle.colors.textPrimary
                            onClicked: {
                                componentListController.pasteFromClipboard()
                            }
                        }
                    }
                }
                // BOM导入卡片
                Card {
                    Layout.fillWidth: true
                    title: "导入BOM文件"
                    RowLayout {
                        width: parent.width
                        spacing: 12
                        ModernButton {
                            text: "选择BOM文件"
                            iconName: "upload"
                            backgroundColor: AppStyle.colors.success
                            hoverColor: AppStyle.colors.successDark
                            pressedColor: AppStyle.colors.successDark
                            onClicked: {
                                bomFileDialog.open()
                            }
                        }
                        Text {
                                                id: bomFileLabel
                                                Layout.fillWidth: true
                                                text: componentListController.bomFilePath.length > 0 ? componentListController.bomFilePath.split("/").pop() : "未选择文件"
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
                                        }            }
                // 元件列表卡片
                Card {
                    Layout.fillWidth: true
                    title: "元器件列表"

                    // 搜索过滤模型 (作为资源定义，不参与布局)
                    resources: [
                        DelegateModel {
                            id: visualModel
                            model: componentListController.componentList

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
                                componentId: modelData
                                searchText: searchInput.text // 传递搜索词用于高亮

                                onDeleteClicked: {
                                    var sourceIndex = -1;
                                    var currentId = modelData;
                                    var list = componentListController.componentList;
                                    for(var i = 0; i < list.length; i++) {
                                        if(list[i] === currentId) {
                                            sourceIndex = i;
                                            break;
                                        }
                                    }
                                    if(sourceIndex !== -1) {
                                        componentListController.removeComponent(sourceIndex);
                                    }
                                }
                            }

                            // 过滤函数
                            function updateFilter() {
                                // 移除所有空格，实现更宽容的搜索 (例如 "C 2040" -> "c2040")
                                var searchTerm = searchInput.text.toLowerCase().replace(/\s+/g, '')

                                // 遍历所有项进行处理
                                for (var i = 0; i < items.count; i++) {
                                    var item = items.get(i)

                                    // 如果搜索词为空，显示所有项
                                    if (searchTerm === "") {
                                        item.inDisplay = true
                                        continue
                                    }

                                    // 获取内容
                                    var content = item.model

                                    // 如果是对象（通常是包装过的），尝试获取 modelData
                                    if (typeof content === 'object' && content !== null) {
                                        if (content.modelData !== undefined) {
                                            content = content.modelData
                                        } else if (content.display !== undefined) {
                                            content = content.display
                                        }
                                    }

                                    // 强制转换为字符串并处理
                                    var idStr = String(content)

                                    // 判断是否匹配
                                    if (idStr.toLowerCase().indexOf(searchTerm) !== -1) {
                                        item.inDisplay = true
                                    } else {
                                        item.inDisplay = false
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
                            text: "共 " + componentListController.componentCount + " 个元器件"
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
                            placeholderText: "搜索元器件..."
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
                                source: AppStyle.isDarkMode ?
                                        "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark-white.svg" : // 暂时用现有图标替代，或者用文字
                                        "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark.svg"
                                // 注意：实际上应该用一个 'search' 图标，这里暂时复用或忽略，
                                // 为了美观，用 Text 替代
                                visible: false
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: "🔍"
                                font.pixelSize: 12
                                color: AppStyle.colors.textSecondary
                            }

                            onTextChanged: {
                                visualModel.updateFilter()
                            }
                        }

                        ModernButton {
                            text: "清空列表"
                            iconName: "trash"
                            font.pixelSize: AppStyle.fontSizes.sm
                            backgroundColor: AppStyle.colors.danger
                            hoverColor: AppStyle.colors.dangerDark
                            pressedColor: AppStyle.colors.dangerDark
                            onClicked: {
                                searchInput.text = "" // 清空搜索
                                componentListController.clearComponentList()
                            }
                        }
                    }
                    // 元件列表视图（5列网格）
                    GridView {
                        id: componentList
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300
                        Layout.topMargin: AppStyle.spacing.md
                        clip: true
                        cellWidth: (width - AppStyle.spacing.md) / 5
                        cellHeight: 56
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
                    title: "导出设置"
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
                                text: "输出路径"
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
                                    placeholderText: "选择输出目录"
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
                                    text: "浏览"
                                    iconName: "folder"
                                    font.pixelSize: AppStyle.fontSizes.sm
                                    backgroundColor: AppStyle.colors.textSecondary
                                    hoverColor: AppStyle.colors.textPrimary
                                    pressedColor: AppStyle.colors.textPrimary
                                    onClicked: {
                                        outputFolderDialog.open()
                                    }
                                }
                            }
                        }
                        // 库名称
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: "库名称"
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
                                placeholderText: "输入库名称 (例如: MyLibrary)"
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
                    Item { Layout.preferredHeight: 10; Layout.fillWidth: true }
                    
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
                                Layout.fillWidth: true
                                id: symbolCheckbox
                                text: "符号库"
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
                                    
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                    
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
                                Layout.fillWidth: true
                                id: footprintCheckbox
                                text: "封装库"
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
                                    
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                    
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
                                Layout.fillWidth: true
                                id: model3dCheckbox
                                text: "3D模型"
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
                                    
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                    
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
                        // 调试模式选项
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 100
                            spacing: 8
                            CheckBox {
                                Layout.fillWidth: true
                                id: debugModeCheckbox
                                text: "调试模式"
                                checked: exportSettingsController.debugMode
                                onCheckedChanged: exportSettingsController.setDebugMode(checked)
                                font.pixelSize: 16
                                
                                indicator: Rectangle {
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    x: debugModeCheckbox.leftPadding
                                    y: parent.height / 2 - height / 2
                                    radius: 4
                                    color: debugModeCheckbox.checked ? AppStyle.colors.primary : "transparent"
                                    border.color: debugModeCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                    border.width: 1.5
                                    
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: "✓"
                                        font.pixelSize: 14
                                        color: "#ffffff"
                                        visible: debugModeCheckbox.checked
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
                                text: "导出模式"
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
                                        text: "追加"
                                        checked: exportSettingsController.exportMode === 0
                                        onCheckedChanged: {
                                            if (checked) {
                                                exportSettingsController.setExportMode(0)
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
                                        text: "保留已存在的元器件"
                                        font.pixelSize: 11
                                        color: AppStyle.colors.textSecondary
                                    }
                                }
                                // 更新模式
                                RowLayout {
                                    spacing: 8
                                    RadioButton {
                                        id: updateModeRadio
                                        text: "更新"
                                        checked: exportSettingsController.exportMode === 1
                                        onCheckedChanged: {
                                            if (checked) {
                                                exportSettingsController.setExportMode(1)
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
                                        text: "覆盖已存在的元器件"
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
                    title: "转换进度"
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
                                label: "数据抓取"
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
                                
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }

                            // 步骤 2: 处理
                            StepItem {
                                Layout.preferredWidth: implicitWidth
                                label: "数据处理"
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
                                
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }

                            // 步骤 3: 写入
                            StepItem {
                                Layout.preferredWidth: implicitWidth
                                label: "文件写入"
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
                                        Behavior on width { NumberAnimation { duration: 100 } }
                                    }
                                    
                                    // 处理部分 (Blue, 占比 1/3)
                                    Rectangle {
                                        height: parent.height
                                        width: (parent.width / 3) * (exportProgressController.processProgress / 100)
                                        color: "#3b82f6"
                                        visible: width > 0
                                        Behavior on width { NumberAnimation { duration: 100 } }
                                    }
                                    
                                    // 写入部分 (Orange, 占比 1/3)
                                    Rectangle {
                                        height: parent.height
                                        width: (parent.width / 3) * (exportProgressController.writeProgress / 100)
                                        color: "#f59e0b"
                                        visible: width > 0
                                        Behavior on width { NumberAnimation { duration: 100 } }
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
                    sourceComponent: Card {
                        title: "转换结果"
                        ColumnLayout {
                            id: resultsContent
                            width: parent.width
                            spacing: AppStyle.spacing.md
                            visible: true

                            // 工具栏（显示重试按钮）
                            RowLayout {
                                Layout.fillWidth: true
                                visible: exportProgressController.failureCount > 0 && !exportProgressController.isExporting
                                
                                Item { Layout.fillWidth: true } // Spacer
                                
                                ModernButton {
                                    text: "重试失败项"
                                    iconName: "play" 
                                    backgroundColor: AppStyle.colors.warning
                                    hoverColor: AppStyle.colors.warningDark
                                    pressedColor: AppStyle.colors.warning
                                    font.pixelSize: 14
                                    
                                    onClicked: exportProgressController.retryFailedComponents()
                                }
                            }
                            // 结果列表（使用 GridView 实现五列显示）
                            GridView {
                                id: resultsList
                                Layout.fillWidth: true
                                Layout.minimumHeight: 200
                                Layout.preferredHeight: Math.min(resultsList.contentHeight + 20, 500)
                                Layout.topMargin: AppStyle.spacing.md
                                clip: true
                                cellWidth: (width - AppStyle.spacing.md) / 5
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
                    title: "导出统计"
                    visible: exportProgressController.hasStatistics
                    ColumnLayout {
                        width: parent.width
                        spacing: AppStyle.spacing.md
                        // 基本统计信息
                        Text {
                            text: "基本统计"
                            font.pixelSize: AppStyle.fontSizes.md
                            font.bold: true
                            color: AppStyle.colors.textPrimary
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppStyle.spacing.lg
                            StatItem {
                                label: "总数"
                                value: exportProgressController.statisticsTotal
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "成功"
                                value: exportProgressController.statisticsSuccess
                                valueColor: AppStyle.colors.success
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "失败"
                                value: exportProgressController.statisticsFailed
                                valueColor: AppStyle.colors.danger
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "成功率"
                                value: exportProgressController.statisticsSuccessRate.toFixed(2) + "%"
                                Layout.fillWidth: true
                            }
                        }
                        // 时间统计信息
                        Text {
                            text: "时间统计"
                            font.pixelSize: AppStyle.fontSizes.md
                            font.bold: true
                            color: AppStyle.colors.textPrimary
                            Layout.topMargin: AppStyle.spacing.sm
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppStyle.spacing.lg
                            StatItem {
                                label: "总耗时"
                                value: (exportProgressController.statisticsTotalDuration / 1000).toFixed(2) + "s"
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "平均抓取"
                                value: exportProgressController.statisticsAvgFetchTime + "ms"
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "平均处理"
                                value: exportProgressController.statisticsAvgProcessTime + "ms"
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "平均写入"
                                value: exportProgressController.statisticsAvgWriteTime + "ms"
                                Layout.fillWidth: true
                            }
                        }
                        // 导出成果统计
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppStyle.spacing.lg
                            StatItem {
                                label: "符号"
                                value: exportProgressController.successSymbolCount
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "封装"
                                value: exportProgressController.successFootprintCount
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "3D模型"
                                value: exportProgressController.successModel3DCount
                                Layout.fillWidth: true
                            }
                        }
                        // 网络统计信息
                        Text {
                            text: "网络统计"
                            font.pixelSize: AppStyle.fontSizes.md
                            font.bold: true
                            color: AppStyle.colors.textPrimary
                            Layout.topMargin: AppStyle.spacing.sm
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppStyle.spacing.lg
                            StatItem {
                                label: "总请求数"
                                value: exportProgressController.statisticsTotalNetworkRequests
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "重试次数"
                                value: exportProgressController.statisticsTotalRetries
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "平均延迟"
                                value: exportProgressController.statisticsAvgNetworkLatency + "ms"
                                Layout.fillWidth: true
                            }
                            StatItem {
                                label: "速率限制"
                                value: exportProgressController.statisticsRateLimitHitCount
                                Layout.fillWidth: true
                            }
                        }
                        // 底部按钮组（居中排列）
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: AppStyle.spacing.sm
                            spacing: AppStyle.spacing.lg

                            // 打开详细报告按钮
                            ModernButton {
                                text: "打开详细统计报告"
                                iconName: "folder" // 或者其他合适的图标
                                backgroundColor: AppStyle.colors.surface
                                textColor: AppStyle.colors.textPrimary
                                hoverColor: AppStyle.colors.border
                                pressedColor: AppStyle.colors.borderFocus
                                // 稍微加个边框让它看起来像二级按钮
                                
                                onClicked: {
                                    Qt.openUrlExternally("file:///" + exportProgressController.statisticsReportPath)
                                }
                            }

                            // 打开导出目录按钮
                            ModernButton {
                                text: "打开导出目录"
                                iconName: "folder"
                                backgroundColor: AppStyle.colors.primary
                                hoverColor: AppStyle.colors.primaryHover
                                pressedColor: AppStyle.colors.primaryPressed
                                
                                onClicked: {
                                    // 打开输出路径
                                    Qt.openUrlExternally("file:///" + exportSettingsController.outputPath)
                                }
                            }
                        }
                    }
                }
                // 导出按钮
                ModernButton {
                    id: exportButton
                    Layout.preferredHeight: 56
                    Layout.fillWidth: true
                    text: exportProgressController.isStopping ? "正在停止..." : (exportProgressController.isExporting ? "结束转换" : "开始转换")
                    iconName: exportProgressController.isExporting ? "close" : "play"
                    font.pixelSize: AppStyle.fontSizes.xxl
                    
                    backgroundColor: exportProgressController.isStopping ? AppStyle.colors.textDisabled : (exportProgressController.isExporting ? AppStyle.colors.danger : AppStyle.colors.primary)
                    hoverColor: exportProgressController.isExporting ? AppStyle.colors.dangerDark : AppStyle.colors.primaryHover
                    pressedColor: exportProgressController.isExporting ? AppStyle.colors.dangerDark : AppStyle.colors.primaryPressed

                    // 防止重复点击：当正在停止时禁用按钮
                    enabled: !exportProgressController.isStopping &&
                             ((componentListController.componentCount > 0 &&
                             (exportSettingsController.exportSymbol || exportSettingsController.exportFootprint || exportSettingsController.exportModel3D)) ||
                             exportProgressController.isExporting)

                    onClicked: {
                        if (exportProgressController.isExporting) {
                            exportProgressController.cancelExport()
                        } else {
                            exportProgressController.startExport(
                                componentListController.componentList,
                                exportSettingsController.outputPath,
                                exportSettingsController.libName,
                                exportSettingsController.exportSymbol,
                                exportSettingsController.exportFootprint,
                                exportSettingsController.exportModel3D,
                                exportSettingsController.overwriteExistingFiles,
                                exportSettingsController.exportMode === 1,  // exportMode === 1 表示更新模式
                                exportSettingsController.debugMode  // 调试模式
                            )
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
}