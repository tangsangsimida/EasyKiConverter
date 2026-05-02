import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: exportSettingsCard
    property var exportSettingsController
    signal openOutputFolderDialog
    title: qsTranslate("MainWindow", "导出设置")
    ColumnLayout {
        id: rootLayout
        width: parent.width
        spacing: AppStyle.spacing.lg
        anchors.margins: AppStyle.spacing.md
        // ==================== SectionHeader 组件 ====================
        component SectionHeader: RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.sm
            property string sectionTitle: ""
            Text {
                text: sectionTitle
                font.pixelSize: AppStyle.fontSizes.sm
                font.bold: true
                color: AppStyle.colors.textSecondary
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: AppStyle.colors.border
                opacity: 0.5
            }
        }

            // ==================== 基础配置区块 ====================
            SectionHeader {
                id: basicConfigHeader
                sectionTitle: qsTranslate("MainWindow", "基础配置")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: AppStyle.spacing.xl
                rowSpacing: AppStyle.spacing.md
                // 输出路径
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.xs
                    Text {
                        text: qsTranslate("MainWindow", "输出路径")
                        font.pixelSize: AppStyle.fontSizes.sm
                        font.bold: true
                        color: AppStyle.colors.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: AppStyle.spacing.md
                        TextField {
                            id: outputPathInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            text: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.outputPath : ""
                            onTextChanged: {
                                if (exportSettingsCard.exportSettingsController) {
                                    exportSettingsCard.exportSettingsController.setOutputPath(text);
                                }
                            }
                            placeholderText: qsTranslate("MainWindow", "选择输出目录")
                            font.pixelSize: AppStyle.fontSizes.sm
                            color: AppStyle.colors.textPrimary
                            placeholderTextColor: AppStyle.colors.textSecondary
                                background: Rectangle {
                                color: AppStyle.colors.surface
                                border.color: outputPathInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                                border.width: outputPathInput.focus ? 2 : 1
                                radius: AppStyle.radius.md
                            }
                        }
                        ModernButton {
                            text: qsTranslate("MainWindow", "浏览")
                            iconName: "folder"
                            font.pixelSize: AppStyle.fontSizes.sm
                            backgroundColor: AppStyle.colors.textSecondary
                            hoverColor: AppStyle.colors.textPrimary
                            pressedColor: AppStyle.colors.textPrimary
                            onClicked: exportSettingsCard.openOutputFolderDialog()
                        }
                    }
                }

                // 库名称
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.xs
                    Text {
                        text: qsTranslate("MainWindow", "库名称")
                        font.pixelSize: AppStyle.fontSizes.sm
                        font.bold: true
                        color: AppStyle.colors.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                    }
                    TextField {
                        id: libNameInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        text: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.libName : ""
                        onTextChanged: {
                            if (exportSettingsCard.exportSettingsController) {
                                exportSettingsCard.exportSettingsController.setLibName(text);
                            }
                        }
                        placeholderText: qsTranslate("MainWindow", "输入库名称 (例如: MyLibrary)")
                        font.pixelSize: AppStyle.fontSizes.sm
                        color: AppStyle.colors.textPrimary
                        placeholderTextColor: AppStyle.colors.textSecondary
                        background: Rectangle {
                            color: AppStyle.colors.surface
                            border.color: libNameInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                            border.width: libNameInput.focus ? 2 : 1
                            radius: AppStyle.radius.md
                        }
                    }
                }
            }

            // ==================== 库元数据区块 ====================
            SectionHeader {
                id: metadataHeader
                sectionTitle: qsTranslate("MainWindow", "库元数据")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: AppStyle.spacing.md
                rowSpacing: AppStyle.spacing.sm
                // 符号库描述
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.xs
                    Text {
                        text: qsTranslate("MainWindow", "符号库描述")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textSecondary
                    }
                    TextField {
                        id: symbolDescInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        text: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.symbolLibraryDescription : ""
                        onTextChanged: {
                            if (exportSettingsCard.exportSettingsController) {
                                exportSettingsCard.exportSettingsController.setSymbolLibraryDescription(text);
                            }
                        }
                        placeholderText: qsTranslate("MainWindow", "符号库描述")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textPrimary
                        placeholderTextColor: AppStyle.colors.textSecondary
                        background: Rectangle {
                            color: AppStyle.colors.surface
                            border.color: symbolDescInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                            border.width: symbolDescInput.focus ? 2 : 1
                            radius: AppStyle.radius.sm
                        }
                    }
                }

                // 封装库描述
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.xs
                    Text {
                        text: qsTranslate("MainWindow", "封装库描述")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textSecondary
                    }
                    TextField {
                        id: footprintDescInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        text: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.footprintLibraryDescription : ""
                        onTextChanged: {
                            if (exportSettingsCard.exportSettingsController) {
                                exportSettingsCard.exportSettingsController.setFootprintLibraryDescription(text);
                            }
                        }
                        placeholderText: qsTranslate("MainWindow", "封装库描述")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textPrimary
                        placeholderTextColor: AppStyle.colors.textSecondary
                        background: Rectangle {
                            color: AppStyle.colors.surface
                            border.color: footprintDescInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                            border.width: footprintDescInput.focus ? 2 : 1
                            radius: AppStyle.radius.sm
                        }
                    }
                }

                // 封装库关键词
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.xs
                    Text {
                        text: qsTranslate("MainWindow", "关键词")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textSecondary
                    }
                    TextField {
                        id: footprintKeywordsInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        text: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.footprintLibraryKeywords : ""
                        onTextChanged: {
                            if (exportSettingsCard.exportSettingsController) {
                                exportSettingsCard.exportSettingsController.setFootprintLibraryKeywords(text);
                            }
                        }
                        placeholderText: qsTranslate("MainWindow", "关键词")
                        font.pixelSize: AppStyle.fontSizes.xs
                        color: AppStyle.colors.textPrimary
                        placeholderTextColor: AppStyle.colors.textSecondary
                        background: Rectangle {
                            color: AppStyle.colors.surface
                            border.color: footprintKeywordsInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                            border.width: footprintKeywordsInput.focus ? 2 : 1
                            radius: AppStyle.radius.sm
                        }
                    }
                }
            }

            // ==================== 导出选项区块 ====================
            SectionHeader {
                id: exportOptionsHeader
                sectionTitle: qsTranslate("MainWindow", "导出选项")
            }

            Flow {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.lg
                // 符号库选项
                CheckBox {
                    id: symbolCheckbox
                    text: qsTranslate("MainWindow", "符号库")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportSymbol : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportSymbol(checked);
                        }
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出符号库文件")
                    indicator: Rectangle {
                        implicitWidth: AppStyle.sizes.checkbox
                        implicitHeight: AppStyle.sizes.checkbox
                        x: symbolCheckbox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: AppStyle.radius.xs
                        color: symbolCheckbox.checked ? AppStyle.colors.primary : "transparent"
                        border.color: symbolCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                        border.width: AppStyle.borderWidths.normal
                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            font.pixelSize: AppStyle.fontSizes.sm
                            color: AppStyle.colors.textOnPrimary
                            visible: symbolCheckbox.checked
                        }
                    }
                    contentItem: Text {
                        text: symbolCheckbox.text
                        font: symbolCheckbox.font
                        color: AppStyle.colors.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: symbolCheckbox.indicator.width + symbolCheckbox.spacing
                    }
                }

                // 封装库选项
                CheckBox {
                    id: footprintCheckbox
                    text: qsTranslate("MainWindow", "封装库")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportFootprint : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportFootprint(checked);
                        }
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出封装库文件")
                    indicator: Rectangle {
                        implicitWidth: AppStyle.sizes.checkbox
                        implicitHeight: AppStyle.sizes.checkbox
                        x: footprintCheckbox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: AppStyle.radius.xs
                        color: footprintCheckbox.checked ? AppStyle.colors.primary : "transparent"
                        border.color: footprintCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                        border.width: AppStyle.borderWidths.normal
                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            font.pixelSize: AppStyle.fontSizes.sm
                            color: AppStyle.colors.textOnPrimary
                            visible: footprintCheckbox.checked
                        }
                    }
                    contentItem: Text {
                        text: footprintCheckbox.text
                        font: footprintCheckbox.font
                        color: AppStyle.colors.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: footprintCheckbox.indicator.width + footprintCheckbox.spacing
                    }
                }

                // 3D模型选项
                RowLayout {
                    spacing: AppStyle.spacing.sm
                    CheckBox {
                        id: model3dCheckbox
                        text: qsTranslate("MainWindow", "3D模型")
                        checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportModel3D : false
                        onCheckedChanged: {
                            if (exportSettingsCard.exportSettingsController) {
                                exportSettingsCard.exportSettingsController.setExportModel3D(checked);
                            }
                        }
                        font.pixelSize: AppStyle.fontSizes.sm
                        ToolTip.visible: hovered
                        ToolTip.text: qsTranslate("MainWindow", "导出 3D 模型文件")
                        indicator: Rectangle {
                            implicitWidth: AppStyle.sizes.checkbox
                            implicitHeight: AppStyle.sizes.checkbox
                            x: model3dCheckbox.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: AppStyle.radius.xs
                            color: model3dCheckbox.checked ? AppStyle.colors.primary : "transparent"
                            border.color: model3dCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                            border.width: AppStyle.borderWidths.normal
                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                font.pixelSize: AppStyle.fontSizes.sm
                                color: AppStyle.colors.textOnPrimary
                                visible: model3dCheckbox.checked
                            }
                        }
                        contentItem: Text {
                            text: model3dCheckbox.text
                            font: model3dCheckbox.font
                            color: AppStyle.colors.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: model3dCheckbox.indicator.width + model3dCheckbox.spacing
                        }
                    }

                    // 3D模型格式按钮组
                    RowLayout {
                        visible: model3dCheckbox.checked
                        spacing: AppStyle.spacing.xs
                        Rectangle {
                            Layout.preferredWidth: 46
                            Layout.preferredHeight: 26
                            radius: AppStyle.radius.xs
                            property bool wrlActive: exportSettingsCard.exportSettingsController ? (exportSettingsCard.exportSettingsController.exportModel3DFormat & 1) !== 0 : false
                            color: wrlActive ? AppStyle.colors.primary : "transparent"
                            border.color: wrlActive ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                            border.width: AppStyle.borderWidths.normal
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    var current = exportSettingsCard.exportSettingsController.exportModel3DFormat;
                                    if ((current & 1) !== 0) {
                                        var newFormat = current & ~1;
                                        if (newFormat === 0) {
                                            exportSettingsCard.exportSettingsController.setExportModel3D(false);
                                        } else {
                                            exportSettingsCard.exportSettingsController.setExportModel3DFormat(newFormat);
                                        }
                                    } else {
                                        exportSettingsCard.exportSettingsController.setExportModel3DFormat(current | 1);
                                    }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                text: "WRL"
                                font.pixelSize: AppStyle.fontSizes.xs
                                color: parent.wrlActive ? "#ffffff" : AppStyle.colors.textSecondary
                            }
                        }
                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 26
                            radius: AppStyle.radius.xs
                            property bool stepActive: exportSettingsCard.exportSettingsController ? (exportSettingsCard.exportSettingsController.exportModel3DFormat & 2) !== 0 : false
                            color: stepActive ? AppStyle.colors.primary : "transparent"
                            border.color: stepActive ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                            border.width: AppStyle.borderWidths.normal
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    var current = exportSettingsCard.exportSettingsController.exportModel3DFormat;
                                    if ((current & 2) !== 0) {
                                        var newFormat = current & ~2;
                                        if (newFormat === 0) {
                                            exportSettingsCard.exportSettingsController.setExportModel3D(false);
                                        } else {
                                            exportSettingsCard.exportSettingsController.setExportModel3DFormat(newFormat);
                                        }
                                    } else {
                                        exportSettingsCard.exportSettingsController.setExportModel3DFormat(current | 2);
                                    }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                text: "STEP"
                                font.pixelSize: AppStyle.fontSizes.xs
                                color: parent.stepActive ? "#ffffff" : AppStyle.colors.textSecondary
                            }
                        }
                    }
                }

                // 预览图选项
                CheckBox {
                    id: previewImagesCheckbox
                    text: qsTranslate("MainWindow", "预览图")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportPreviewImages : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportPreviewImages(checked);
                        }
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出预览图文件")
                    indicator: Rectangle {
                        implicitWidth: AppStyle.sizes.checkbox
                        implicitHeight: AppStyle.sizes.checkbox
                        x: previewImagesCheckbox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: AppStyle.radius.xs
                        color: previewImagesCheckbox.checked ? AppStyle.colors.primary : "transparent"
                        border.color: previewImagesCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                        border.width: AppStyle.borderWidths.normal
                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            font.pixelSize: AppStyle.fontSizes.sm
                            color: AppStyle.colors.textOnPrimary
                            visible: previewImagesCheckbox.checked
                        }
                    }
                    contentItem: Text {
                        text: previewImagesCheckbox.text
                        font: previewImagesCheckbox.font
                        color: AppStyle.colors.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: previewImagesCheckbox.indicator.width + previewImagesCheckbox.spacing
                    }
                }

                // 手册选项
                CheckBox {
                    id: datasheetCheckbox
                    text: qsTranslate("MainWindow", "手册")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportDatasheet : false
                    onCheckedChanged: {
                        if (exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportDatasheet(checked);
                        }
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "导出数据手册文件")
                    indicator: Rectangle {
                        implicitWidth: AppStyle.sizes.checkbox
                        implicitHeight: AppStyle.sizes.checkbox
                        x: datasheetCheckbox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: AppStyle.radius.xs
                        color: datasheetCheckbox.checked ? AppStyle.colors.primary : "transparent"
                        border.color: datasheetCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                        border.width: AppStyle.borderWidths.normal
                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            font.pixelSize: AppStyle.fontSizes.sm
                            color: AppStyle.colors.textOnPrimary
                            visible: datasheetCheckbox.checked
                        }
                    }
                    contentItem: Text {
                        text: datasheetCheckbox.text
                        font: datasheetCheckbox.font
                        color: AppStyle.colors.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: datasheetCheckbox.indicator.width + datasheetCheckbox.spacing
                    }
                }
            }

            // ==================== 导出模式区块 ====================
            SectionHeader {
                id: exportModeHeader
                sectionTitle: qsTranslate("MainWindow", "导出模式")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.xl
                // 追加模式
                RadioButton {
                    id: appendModeRadio
                    text: qsTranslate("MainWindow", "追加模式")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportMode === 0 : true
                    onCheckedChanged: {
                        if (checked && exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportMode(0);
                        }
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "保留已存在的元器件，只追加新的元器件")
                    indicator: Rectangle {
                        implicitWidth: AppStyle.sizes.radioButton
                        implicitHeight: AppStyle.sizes.radioButton
                        x: appendModeRadio.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: width / 2
                        color: "transparent"
                        border.color: appendModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                        border.width: AppStyle.borderWidths.normal
                        Rectangle {
                            anchors.centerIn: parent
                            width: AppStyle.sizes.radioButtonIndicator
                            height: AppStyle.sizes.radioButtonIndicator
                            radius: width / 2
                            color: AppStyle.colors.primary
                            visible: appendModeRadio.checked
                        }
                    }
                    contentItem: Text {
                        text: appendModeRadio.text
                        font: appendModeRadio.font
                        color: AppStyle.colors.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: appendModeRadio.indicator.width + appendModeRadio.spacing
                    }
                }

                Text {
                    text: qsTranslate("MainWindow", "保留已存在的元器件")
                    font.pixelSize: AppStyle.fontSizes.sm
                    color: AppStyle.colors.textSecondary
                    verticalAlignment: Text.AlignVCenter
                }

                // 更新模式
                RadioButton {
                    id: updateModeRadio
                    text: qsTranslate("MainWindow", "更新模式")
                    checked: exportSettingsCard.exportSettingsController ? exportSettingsCard.exportSettingsController.exportMode === 1 : false
                    onCheckedChanged: {
                        if (checked && exportSettingsCard.exportSettingsController) {
                            exportSettingsCard.exportSettingsController.setExportMode(1);
                        }
                    }
                    font.pixelSize: AppStyle.fontSizes.sm
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("MainWindow", "覆盖已存在的元器件")
                    indicator: Rectangle {
                        implicitWidth: AppStyle.sizes.radioButton
                        implicitHeight: AppStyle.sizes.radioButton
                        x: updateModeRadio.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: width / 2
                        color: "transparent"
                        border.color: updateModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                        border.width: AppStyle.borderWidths.normal
                        Rectangle {
                            anchors.centerIn: parent
                            width: AppStyle.sizes.radioButtonIndicator
                            height: AppStyle.sizes.radioButtonIndicator
                            radius: width / 2
                            color: AppStyle.colors.primary
                            visible: updateModeRadio.checked
                        }
                    }
                    contentItem: Text {
                        text: updateModeRadio.text
                        font: updateModeRadio.font
                        color: AppStyle.colors.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: updateModeRadio.indicator.width + updateModeRadio.spacing
                    }
                }

                Text {
                    text: qsTranslate("MainWindow", "覆盖已存在的元器件")
                    font.pixelSize: AppStyle.fontSizes.sm
                    color: AppStyle.colors.textSecondary
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
