import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: exportSettingsCard
    // 外部依赖
    property var exportSettingsController
    // 信号：请求打开输出目录对话框
    signal openOutputFolderDialog
    title: qsTranslate("MainWindow", "导出设置")
    // 用于测量文本宽度的 Text 元素（不可见）
    Text {
        id: textMeasurer
        visible: false
        font.pixelSize: ResponsiveHelper.fontSizes.md
        font.family: "Arial"
    }

    // 计算选项文本所需的宽度
    readonly property int optionTextWidth: {
        // 测量所有选项文本的宽度
        var texts = [qsTranslate("MainWindow", "符号库"), qsTranslate("MainWindow", "封装库"), qsTranslate("MainWindow", "3D模型"), qsTranslate("MainWindow", "预览图"), qsTranslate("MainWindow", "手册")];
        var maxWidth = 0;
        for (var i = 0; i < texts.length; i++) {
            textMeasurer.text = texts[i];
            var textWidth = textMeasurer.paintedWidth;
            if (textWidth > maxWidth) {
                maxWidth = textWidth;
            }
        }

        // 加上复选框指示器的宽度（22px）和间距（8px）
        return maxWidth + 22 + 8;
    }

    // 计算导出模式选项文本所需的宽度
    readonly property int exportModeTextWidth: {
        // 测量导出模式文本的宽度
        var texts = [qsTranslate("MainWindow", "追加"), qsTranslate("MainWindow", "更新")];
        var maxWidth = 0;
        for (var i = 0; i < texts.length; i++) {
            textMeasurer.text = texts[i];
            var textWidth = textMeasurer.paintedWidth;
            if (textWidth > maxWidth) {
                maxWidth = textWidth;
            }
        }

        // 加上单选按钮指示器的宽度（20px）和间距（8px）
        return maxWidth + 20 + 8;
    }

    // 计算导出选项所需的最小宽度
    readonly property int minimumRequiredWidth: {
        // 5个普通选项 + 1个导出模式选项
        var optionsWidth = optionTextWidth * 5 + exportModeTextWidth;
        // 5个间距（使用最小间距）
        var spacingWidth = 6 * 5;
        // 卡片的左右内边距（24px * 2）
        var cardPadding = 24 * 2;
        // 卡片的边框（1px * 2）
        var cardBorder = 2;
        // 额外的安全边距（20px）
        var safetyMargin = 20;
        return optionsWidth + spacingWidth + cardPadding + cardBorder + safetyMargin;
    }
    GridLayout {
        width: parent.width
        columns: 2
        columnSpacing: ResponsiveHelper.spacing.xl
        rowSpacing: ResponsiveHelper.spacing.md
        // 输出路径
        ColumnLayout {
            Layout.fillWidth: true
            spacing: ResponsiveHelper.spacing.sm
            Text {
                text: qsTranslate("MainWindow", "输出路径")
                font.pixelSize: ResponsiveHelper.fontSizes.sm
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: ResponsiveHelper.spacing.md
                TextField {
                    id: outputPathInput
                    Layout.fillWidth: true
                    text: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.outputPath : ""
                    onTextChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setOutputPath(text);
                        }
                    }
                    placeholderText: qsTranslate("MainWindow", "选择输出目录")
                    font.pixelSize: ResponsiveHelper.fontSizes.sm
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
                    text: qsTranslate("MainWindow", "浏览")
                    iconName: "folder"
                    font.pixelSize: AppStyle.fontSizes.sm
                    backgroundColor: AppStyle.colors.textSecondary
                    hoverColor: AppStyle.colors.textPrimary
                    pressedColor: AppStyle.colors.textPrimary
                    onClicked: {
                        exportSettingsCard.openOutputFolderDialog();
                    }
                }
            }
        }
        // 库名称
        ColumnLayout {
            Layout.fillWidth: true
            spacing: ResponsiveHelper.spacing.sm
            Text {
                text: qsTranslate("MainWindow", "库名称")
                font.pixelSize: ResponsiveHelper.fontSizes.sm
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }
            TextField {
                id: libNameInput
                Layout.fillWidth: true
                text: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.libName : ""
                onTextChanged: {
                    if (exportSettingsCard.exportSettingsController) {
                        exportSettingsCard.exportSettingsController.setLibName(text);
                    }
                }
                placeholderText: qsTranslate("MainWindow", "输入库名称 (例如: MyLibrary)")
                font.pixelSize: ResponsiveHelper.fontSizes.sm
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
    Item {
        Layout.fillWidth: true
        Layout.columnSpan: 10  // 跨越10列
        Layout.preferredHeight: exportOptionsLayout.implicitHeight
        Layout.minimumWidth: ResponsiveHelper.minimumWindowWidth  // 使用计算的最小窗口宽度
        RowLayout {
            id: exportOptionsLayout
            anchors.fill: parent
            spacing: ResponsiveHelper.spacing.lg
            // 符号库选项
            ColumnLayout {
                Layout.minimumWidth: 80
                spacing: ResponsiveHelper.spacing.sm
                CheckBox {
                    id: symbolCheckbox
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "符号库")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportSymbol : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportSymbol(checked);
                        }
                    }
                    font.pixelSize: ResponsiveHelper.fontSizes.md
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出符号库文件")
                    ToolTip.delay: 500
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
                Layout.minimumWidth: 80
                spacing: ResponsiveHelper.spacing.sm
                CheckBox {
                    id: footprintCheckbox
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "封装库")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportFootprint : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportFootprint(checked);
                        }
                    }
                    font.pixelSize: ResponsiveHelper.fontSizes.md
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出封装库文件")
                    ToolTip.delay: 500
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
                Layout.minimumWidth: 80
                spacing: ResponsiveHelper.spacing.sm
                CheckBox {
                    id: model3dCheckbox
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "3D模型")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportModel3D : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportModel3D(checked);
                        }
                    }
                    font.pixelSize: ResponsiveHelper.fontSizes.md
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出 3D 模型文件")
                    ToolTip.delay: 500
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
            // 预览图选项
            ColumnLayout {
                Layout.minimumWidth: 80
                spacing: ResponsiveHelper.spacing.sm
                CheckBox {
                    id: previewImagesCheckbox
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "预览图")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportPreviewImages : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportPreviewImages(checked);
                        }
                    }
                    font.pixelSize: ResponsiveHelper.fontSizes.md
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出预览图文件")
                    ToolTip.delay: 500
                    indicator: Rectangle {
                        implicitWidth: 22
                        implicitHeight: 22
                        x: previewImagesCheckbox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: 4
                        color: previewImagesCheckbox.checked ? AppStyle.colors.primary : "transparent"
                        border.color: previewImagesCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
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
                            visible: previewImagesCheckbox.checked
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
            // 手册选项
            ColumnLayout {
                Layout.minimumWidth: 80
                spacing: ResponsiveHelper.spacing.sm
                CheckBox {
                    id: datasheetCheckbox
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "手册")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportDatasheet : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportDatasheet(checked);
                        }
                    }
                    font.pixelSize: ResponsiveHelper.fontSizes.md
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出数据手册文件")
                    ToolTip.delay: 500
                    indicator: Rectangle {
                        implicitWidth: 22
                        implicitHeight: 22
                        x: datasheetCheckbox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: 4
                        color: datasheetCheckbox.checked ? AppStyle.colors.primary : "transparent"
                        border.color: datasheetCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
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
                            visible: datasheetCheckbox.checked
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
                Layout.columnSpan: 2  // 跨越两列，分配更多空间
                Layout.fillWidth: true
                Layout.minimumWidth: 280  // 大幅增加最小宽度
                spacing: ResponsiveHelper.spacing.sm
                Text {
                    Layout.fillWidth: true
                    text: qsTranslate("MainWindow", "导出模式")
                    font.pixelSize: ResponsiveHelper.fontSizes.sm
                    font.bold: true
                    color: AppStyle.colors.textPrimary
                    horizontalAlignment: Text.AlignLeft
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: ResponsiveHelper.spacing.xs
                    // 追加模式
                    RowLayout {
                        spacing: ResponsiveHelper.spacing.sm
                        RadioButton {
                            id: appendModeRadio
                            text: qsTranslate("MainWindow", "追加")
                            checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportMode === 0 : true
                            onCheckedChanged: {
                                if (checked && exportSettingsCard.exportSettingsController) {
                                    exportSettingsCard.exportSettingsController.setExportMode(0);
                                }
                            }
                            font.pixelSize: ResponsiveHelper.fontSizes.sm
                            ToolTip.visible: hovered
                            ToolTip.text: qsTranslate("MainWindow", "保留已存在的元器件，只追加新的元器件")
                            ToolTip.delay: 500
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
                            text: qsTranslate("MainWindow", "保留已存在的元器件")
                            font.pixelSize: ResponsiveHelper.fontSizes.xs
                            color: AppStyle.colors.textSecondary
                        }
                    }
                    // 更新模式
                    RowLayout {
                        spacing: ResponsiveHelper.spacing.sm
                        RadioButton {
                            id: updateModeRadio
                            text: qsTranslate("MainWindow", "更新")
                            checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportMode === 1 : false
                            onCheckedChanged: {
                                if (checked && exportSettingsCard.exportSettingsController) {
                                    exportSettingsCard.exportSettingsController.setExportMode(1);
                                }
                            }
                            font.pixelSize: ResponsiveHelper.fontSizes.sm
                            ToolTip.visible: hovered
                            ToolTip.text: qsTranslate("MainWindow", "覆盖已存在的元器件")
                            ToolTip.delay: 500
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
                            text: qsTranslate("MainWindow", "覆盖已存在的元器件")
                            font.pixelSize: ResponsiveHelper.fontSizes.xs
                            color: AppStyle.colors.textSecondary
                        }
                    }
                }
            }
        }
    }
}
