import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0
import EasyKiconverter_Cpp_Version.src.ui.qml.components

Item {
    id: window

    // 连接到 MainController
    property var controller: mainController

    // 绑定 AppStyle.isDarkMode 到 mainController.isDarkMode
    Binding {
        target: AppStyle
        property: "isDarkMode"
        value: mainController.isDarkMode
    }

    // BOM 文件选择对话框
    FileDialog {
        id: bomFileDialog
        title: "选择 BOM 文件"
        nameFilters: ["Text files (*.txt)", "CSV files (*.csv)", "All files (*.*)"]
        onAccepted: {
            controller.selectBomFile(selectedFile)
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
            controller.selectOutputPath(path)
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

        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        // 内容容器（添加左右边距）
        Item {
            width: scrollView.width
            implicitHeight: contentLayout.implicitHeight

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
                color: "#1e293b"
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: "将嘉立创EDA元器件转换为KiCad格式"
                font.pixelSize: 18
                color: "#64748b"
                horizontalAlignment: Text.AlignHCenter
            }

            // 深色模式切换按钮和 GitHub 图标
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: AppStyle.spacing.sm

                // GitHub 图标按钮
                MouseArea {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: AppStyle.isDarkMode ? 
                                "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark-white.svg" :
                                "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark.svg"
                        fillMode: Image.PreserveAspectFit
                        opacity: parent.pressed ? 0.7 : (parent.hovered ? 1.0 : 0.8)

                        Behavior on opacity {
                            NumberAnimation {
                                duration: AppStyle.durations.fast
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
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"

                        Image {
                            id: themeSwitchIcon
                            anchors.centerIn: parent
                            width: parent.width - 4
                            height: parent.height - 4
                            source: AppStyle.isDarkMode ? 
                                    "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Grey_light_bulb.svg" :
                                    "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/Blue_light_bulb.svg"
                            fillMode: Image.PreserveAspectFit
                            opacity: 0.85
                            scale: 1.0

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

                            Behavior on source {
                                PropertyAnimation {
                                    duration: 0
                                }
                            }
                        }

                        // 悬停和点击效果
                        states: [
                            State {
                                name: "normal"
                                when: !themeSwitchButton.hovered && !themeSwitchButton.pressed
                                PropertyChanges { target: themeSwitchIcon; opacity: 0.85; scale: 1.0 }
                            },
                            State {
                                name: "hovered"
                                when: themeSwitchButton.hovered && !themeSwitchButton.pressed
                                PropertyChanges { target: themeSwitchIcon; opacity: 1.0; scale: 1.15 }
                            },
                            State {
                                name: "pressed"
                                when: themeSwitchButton.pressed
                                PropertyChanges { target: themeSwitchIcon; opacity: 0.7; scale: 0.95 }
                            }
                        ]
                    }

                    onClicked: {
                        controller.setDarkMode(!AppStyle.isDarkMode)
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
                                controller.addComponent(componentInput.text)
                            }
                        }
                    }

                    ModernButton {
                        text: "添加"
                        iconName: "add"
                        enabled: componentInput.text.length > 0

                        onClicked: {
                            if (componentInput.text.length > 0) {
                                controller.addComponent(componentInput.text)
                                componentInput.text = ""
                            }
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

                                            text: controller.bomFilePath.length > 0 ? controller.bomFilePath.split("/").pop() : "未选择文件"

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

                                        text: controller.bomResult

                                        font.pixelSize: AppStyle.fontSizes.sm

                                        color: AppStyle.colors.success

                                        horizontalAlignment: Text.AlignHCenter

                                        visible: controller.bomResult.length > 0

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
                        text: "共 " + controller.componentCount + " 个元器件"
                        font.pixelSize: 14
                        color: "#64748b"
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
                            controller.clearComponentList()
                        }
                    }
                }

                // 元件列表视图（两列网格）
                GridView {
                    id: componentList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: AppStyle.spacing.md
                    clip: true
                    cellWidth: (width - AppStyle.spacing.md) / 2
                    cellHeight: 56
                    flow: GridView.FlowLeftToRight
                    layoutDirection: Qt.LeftToRight

                    model: controller.componentList

                    delegate: ComponentListItem {
                        width: componentList.cellWidth - AppStyle.spacing.md
                        anchors.horizontalCenter: parent ? undefined : undefined
                        componentId: modelData
                        onDeleteClicked: {
                            controller.removeComponent(index)
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

                GridLayout {
                    width: parent.width
                    columns: 3
                    columnSpacing: 20
                    rowSpacing: 12

                    // 符号库选项
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        CheckBox {
                            id: symbolCheckbox
                            text: "符号库"
                            checked: controller.exportSymbol
                            onCheckedChanged: controller.exportSymbol = checked
                            font.pixelSize: 16

                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: "#1e293b"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: parent.indicator.width + parent.spacing
                            }
                        }

                        Text {
                            text: "导出 KiCad 符号库文件"
                            font.pixelSize: 12
                            color: "#64748b"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // 封装库选项
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        CheckBox {
                            id: footprintCheckbox
                            text: "封装库"
                            checked: controller.exportFootprint
                            onCheckedChanged: controller.exportFootprint = checked
                            font.pixelSize: 16

                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: "#1e293b"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: parent.indicator.width + parent.spacing
                            }
                        }

                        Text {
                            text: "导出 KiCad 封装库文件"
                            font.pixelSize: 12
                            color: "#64748b"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // 3D模型选项
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        CheckBox {
                            id: model3dCheckbox
                            text: "3D模型"
                            checked: controller.exportModel3D
                            onCheckedChanged: controller.exportModel3D = checked
                            font.pixelSize: 16

                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: "#1e293b"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: parent.indicator.width + parent.spacing
                            }
                        }

                        Text {
                            text: "导出 3D 模型文件 (WRL/STEP)"
                            font.pixelSize: 12
                            color: "#64748b"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
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
                            color: "#1e293b"
                            horizontalAlignment: Text.AlignHCenter
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            TextField {
                                id: outputPathInput
                                Layout.fillWidth: true
                                text: controller.outputPath
                                onTextChanged: controller.outputPath = text
                                placeholderText: "选择输出目录"
                                font.pixelSize: 14

                                background: Rectangle {
                                    color: "#ffffff"
                                    border.color: "#cbd5e1"
                                    border.width: 2
                                    radius: 8
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
                            color: "#1e293b"
                            horizontalAlignment: Text.AlignHCenter
                        }

                        TextField {
                            id: libNameInput
                            Layout.fillWidth: true
                            text: controller.libName
                            onTextChanged: controller.libName = text
                            placeholderText: "输入库名称 (例如: MyLibrary)"
                            font.pixelSize: 14

                            background: Rectangle {
                                color: "#ffffff"
                                border.color: "#cbd5e1"
                                border.width: 2
                                radius: 8
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

                    ProgressBar {
                        id: progressBar
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: controller.progress
                        visible: controller.isExporting

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

                    Text {
                        id: statusLabel
                        Layout.fillWidth: true
                        text: controller.status
                        font.pixelSize: 14
                        color: "#64748b"
                        horizontalAlignment: Text.AlignHCenter
                        visible: controller.status.length > 0
                    }
                }
            }

            // 转换结果卡片（延迟加载）
            Loader {
                id: resultsLoader
                Layout.fillWidth: true
                active: false

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

                        // 结果列表
                        ListView {
                            id: resultsList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.topMargin: AppStyle.spacing.md
                            clip: true
                            spacing: AppStyle.spacing.md

                            model: controller.resultsList

                            delegate: ResultListItem {
                                width: resultsList.width
                                componentId: model.componentId
                                status: model.status
                                message: model.message
                            }

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
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
                text: controller.isExporting ? "转换中..." : "开始转换"
                iconName: controller.isExporting ? "loading" : "play"
                font.pixelSize: AppStyle.fontSizes.xxl
                enabled: controller.componentCount > 0 && !controller.isExporting

                onClicked: {
                    controller.startExport()
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