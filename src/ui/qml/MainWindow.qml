import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0
import EasyKiconverter_Cpp_Version.src.ui.qml.components
Item {
    id: window
    // 连接到 ViewModel
    property var componentListController: componentListViewModel
    property var exportSettingsController: exportSettingsViewModel
    property var exportProgressController: exportProgressViewModel
    property var themeController: themeSettingsViewModel
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
    // 背景图片
    Image {
        id: backgroundImage
        anchors.fill: parent
        source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/imgs/background.jpg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        enabled: false  // 不拦截鼠标事件
        // 添加加载状态检查
        onStatusChanged: {
            if (status === Image.Error) {
                console.log("Failed to load background image:", source)
            } else if (status === Image.Ready) {
                console.log("Background image loaded successfully")
            }
        }
    }
    // 半透明遮罩层（确保内容可读性）
    Rectangle {
        anchors.fill: parent
        color: AppStyle.isDarkMode ? "#000000" : "#ffffff"
        opacity: AppStyle.isDarkMode ? 0.3 : 0.5
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
    // 主滚动区域
    ScrollView {
        id: scrollView
        anchors.fill: parent
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
                // 顶部边距
                Item {
                    Layout.preferredHeight: 40
                }
            // 欢迎标题
            Text {
                Layout.fillWidth: true
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
                color: AppStyle.colors.textSecondary
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
                            }
                        }
                    }
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
                Layout.preferredHeight: 300
                title: "元器件列表"
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
                    ModernButton {
                        text: "清空列表"
                        iconName: "trash"
                        font.pixelSize: AppStyle.fontSizes.sm
                        backgroundColor: AppStyle.colors.danger
                        hoverColor: AppStyle.colors.dangerDark
                        pressedColor: AppStyle.colors.dangerDark
                        onClicked: {
                            componentListController.clearComponentList()
                        }
                    }
                }
                // 元件列表视图（5列网格）
                GridView {
                    id: componentList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: AppStyle.spacing.md
                    clip: true
                    cellWidth: (width - AppStyle.spacing.md) / 5
                    cellHeight: 56
                    flow: GridView.FlowLeftToRight
                    layoutDirection: Qt.LeftToRight
                    model: componentListController.componentList
                    delegate: ComponentListItem {
                        width: componentList.cellWidth - AppStyle.spacing.md
                        anchors.horizontalCenter: parent ? undefined : undefined
                        componentId: modelData
                        onDeleteClicked: {
                            componentListController.removeComponent(index)
                        }
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
                            easing.type: AppStyle.easings.easeIn
                        }
                        NumberAnimation {
                            property: "scale"
                            from: 1
                            to: 0.8
                            duration: AppStyle.durations.normal
                            easing.type: AppStyle.easings.easeIn
                        }
                    }
                }
            }
            // 导出选项卡片
            Card {
                Layout.fillWidth: true
                title: "导出选项"
                RowLayout {
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
                            checked: false
                            onCheckedChanged: exportSettingsController.setDebugMode(checked)
                            font.pixelSize: 16
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
                                    color: "#94a3b8"
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
                                    color: "#94a3b8"
                                }
                            }
                        }
                    }
                }
            }
            // 导出设置卡片
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
            }
            // 进度显示卡片
                        Card {
                            Layout.fillWidth: true
                            title: "转换进度"
                            visible: progressBar.visible
                            ColumnLayout {
                                width: parent.width
                                spacing: 12
                                // 总进度条
                                Text {
                                    text: "总进度: " + Math.round(exportProgressController.progress) + "%"
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: AppStyle.colors.textPrimary
                                }
                                ProgressBar {
                                    id: progressBar
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    value: exportProgressController.progress
                                    visible: exportProgressController.isExporting
                                    background: Rectangle {
                                        color: AppStyle.colors.border
                                        radius: AppStyle.radius.md
                                    }
                                    contentItem: Item {
                                        Rectangle {
                                            width: progressBar.visualPosition * parent.width
                                            height: parent.height
                                            radius: AppStyle.radius.md
                                            color: AppStyle.colors.primary
                                            Behavior on width {
                                                NumberAnimation {
                                                    duration: AppStyle.durations.normal
                                                    easing.type: AppStyle.easings.easeOut
                                                }
                                            }
                                        }
                                    }
                                }
                                // 分段进度条
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: exportProgressController.isExporting
                                    // 抓取阶段进度
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Text {
                                            text: "抓取"
                                            font.pixelSize: 12
                                            color: AppStyle.colors.textSecondary
                                            Layout.preferredWidth: 40
                                        }
                                        ProgressBar {
                                            Layout.fillWidth: true
                                            from: 0
                                            to: 100
                                            value: exportProgressController.fetchProgress
                                            background: Rectangle {
                                                color: AppStyle.colors.border
                                                radius: AppStyle.radius.sm
                                            }
                                            contentItem: Item {
                                                Rectangle {
                                                    width: parent.visualPosition * parent.width
                                                    height: parent.height
                                                    radius: AppStyle.radius.sm
                                                    color: "#22c55e"  // 绿色
                                                    Behavior on width {
                                                        NumberAnimation {
                                                            duration: AppStyle.durations.normal
                                                            easing.type: AppStyle.easings.easeOut
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        Text {
                                            text: Math.round(exportProgressController.fetchProgress) + "%"
                                            font.pixelSize: 12
                                            color: AppStyle.colors.textSecondary
                                            Layout.preferredWidth: 35
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                    // 处理阶段进度
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Text {
                                            text: "处理"
                                            font.pixelSize: 12
                                            color: AppStyle.colors.textSecondary
                                            Layout.preferredWidth: 40
                                        }
                                        ProgressBar {
                                            Layout.fillWidth: true
                                            from: 0
                                            to: 100
                                            value: exportProgressController.processProgress
                                            background: Rectangle {
                                                color: AppStyle.colors.border
                                                radius: AppStyle.radius.sm
                                            }
                                            contentItem: Item {
                                                Rectangle {
                                                    width: parent.visualPosition * parent.width
                                                    height: parent.height
                                                    radius: AppStyle.radius.sm
                                                    color: "#3b82f6"  // 蓝色
                                                    Behavior on width {
                                                        NumberAnimation {
                                                            duration: AppStyle.durations.normal
                                                            easing.type: AppStyle.easings.easeOut
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        Text {
                                            text: Math.round(exportProgressController.processProgress) + "%"
                                            font.pixelSize: 12
                                            color: AppStyle.colors.textSecondary
                                            Layout.preferredWidth: 35
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                    // 写入阶段进度
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Text {
                                            text: "写入"
                                            font.pixelSize: 12
                                            color: AppStyle.colors.textSecondary
                                            Layout.preferredWidth: 40
                                        }
                                        ProgressBar {
                                            Layout.fillWidth: true
                                            from: 0
                                            to: 100
                                            value: exportProgressController.writeProgress
                                            background: Rectangle {
                                                color: AppStyle.colors.border
                                                radius: AppStyle.radius.sm
                                            }
                                            contentItem: Item {
                                                Rectangle {
                                                    width: parent.visualPosition * parent.width
                                                    height: parent.height
                                                    radius: AppStyle.radius.sm
                                                    color: "#f59e0b"  // 橙色
                                                    Behavior on width {
                                                        NumberAnimation {
                                                            duration: AppStyle.durations.normal
                                                            easing.type: AppStyle.easings.easeOut
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        Text {
                                            text: Math.round(exportProgressController.writeProgress) + "%"
                                            font.pixelSize: 12
                                            color: AppStyle.colors.textSecondary
                                            Layout.preferredWidth: 35
                                            horizontalAlignment: Text.AlignRight
                                        }
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
                        // 统计信息
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppStyle.spacing.xl
                            Rectangle {
                                Layout.preferredWidth: 120
                                Layout.preferredHeight: 80
                                color: AppStyle.colors.successLight
                                radius: AppStyle.radius.md
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: AppStyle.spacing.xs
                                    Text {
                                        text: "成功"
                                        font.pixelSize: AppStyle.fontSizes.xs
                                        color: AppStyle.colors.successDark
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    Text {
                                        id: successCountLabel
                                        text: "0"
                                        font.pixelSize: AppStyle.fontSizes.xxl
                                        font.bold: true
                                        color: AppStyle.colors.successDark
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 120
                                Layout.preferredHeight: 80
                                color: AppStyle.colors.dangerLight
                                radius: AppStyle.radius.md
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: AppStyle.spacing.xs
                                    Text {
                                        text: "失败"
                                        font.pixelSize: AppStyle.fontSizes.xs
                                        color: AppStyle.colors.dangerDark
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    Text {
                                        id: failedCountLabel
                                        text: "0"
                                        font.pixelSize: AppStyle.fontSizes.xxl
                                        font.bold: true
                                        color: AppStyle.colors.dangerDark
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 120
                                Layout.preferredHeight: 80
                                color: AppStyle.colors.warningLight
                                radius: AppStyle.radius.md
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: AppStyle.spacing.xs
                                    Text {
                                        text: "部分成功"
                                        font.pixelSize: AppStyle.fontSizes.xs
                                        color: AppStyle.colors.warningDark
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    Text {
                                        id: partialCountLabel
                                        text: "0"
                                        font.pixelSize: AppStyle.fontSizes.xxl
                                        font.bold: true
                                        color: AppStyle.colors.warningDark
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
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
            // 导出按钮
            ModernButton {
                id: exportButton
                Layout.preferredHeight: 56
                Layout.fillWidth: true
                text: exportProgressController.isExporting ? "转换中..." : "开始转换"
                iconName: exportProgressController.isExporting ? "loading" : "play"
                font.pixelSize: AppStyle.fontSizes.xxl
                enabled: componentListController.componentCount > 0 && !exportProgressController.isExporting
                onClicked: {
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
            // 底部边距
            Item {
                Layout.preferredHeight: 40
            }
        }
        } // 关闭内容容器 Item
    }
}
