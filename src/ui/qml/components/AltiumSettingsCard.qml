import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief Altium Designer 特有设置卡片
 * @details 包含 Altium 格式独有的导出选项。
 */
ColumnLayout {
    id: altiumCard

    /** @brief 导出设置控制器 */
    property var exportSettingsController

    spacing: AppStyle.spacing.md

    Flow {
        Layout.fillWidth: true
        spacing: AppStyle.spacing.lg

        // 符号库选项
        CheckBox {
            id: symbolCheckbox
            text: qsTranslate("MainWindow", "符号库 (.SchLib)")
            checked: altiumCard.exportSettingsController ? altiumCard.exportSettingsController.exportSymbol : false
            onCheckedChanged: {
                if (altiumCard.exportSettingsController) {
                    altiumCard.exportSettingsController.setExportSymbol(checked);
                }
            }
            font.pixelSize: AppStyle.fontSizes.sm
            ToolTip.visible: hovered
            ToolTip.text: qsTranslate("MainWindow", "导出 Altium .SchLib 符号库文件")
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
            text: qsTranslate("MainWindow", "封装库 (.PcbLib)")
            checked: altiumCard.exportSettingsController ? altiumCard.exportSettingsController.exportFootprint : false
            onCheckedChanged: {
                if (altiumCard.exportSettingsController) {
                    altiumCard.exportSettingsController.setExportFootprint(checked);
                }
            }
            font.pixelSize: AppStyle.fontSizes.sm
            ToolTip.visible: hovered
            ToolTip.text: qsTranslate("MainWindow", "导出 Altium .PcbLib 封装库文件")
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
        CheckBox {
            id: model3dCheckbox
            text: qsTranslate("MainWindow", "3D模型 (STEP)")
            checked: altiumCard.exportSettingsController ? altiumCard.exportSettingsController.exportModel3D : false
            onCheckedChanged: {
                if (altiumCard.exportSettingsController) {
                    altiumCard.exportSettingsController.setExportModel3D(checked);
                    // Altium 优先使用 STEP 格式
                    if (checked) {
                        altiumCard.exportSettingsController.setExportModel3DFormat(2);  // STEP only
                    }
                }
            }
            font.pixelSize: AppStyle.fontSizes.sm
            ToolTip.visible: hovered
            ToolTip.text: qsTranslate("MainWindow", "导出 STEP 格式 3D 模型（Altium 原生支持）")
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
    }

    // Altium 特有信息提示
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: altiumInfoText.implicitHeight + AppStyle.spacing.md * 2
        radius: AppStyle.radius.sm
        color: AppStyle.colors.surface
        border.color: AppStyle.colors.border
        border.width: 1

        Text {
            id: altiumInfoText
            anchors.fill: parent
            anchors.margins: AppStyle.spacing.md
            text: qsTranslate("MainWindow", "Altium 导出说明：\n"
                              + "- 符号库导出为 .SchLib 格式\n"
                              + "- 封装库导出为 .PcbLib 格式\n"
                              + "- 3D 模型以 STEP 格式嵌入封装\n"
                              + "- 生成的文件可直接在 Altium Designer 中打开")
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }
    }
}
