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

        StyledCheckBox {
            text: qsTranslate("MainWindow", "符号库 (.SchLib)")
            ToolTip.text: qsTranslate("MainWindow", "导出 Altium .SchLib 符号库文件")
            checked: altiumCard.exportSettingsController ? altiumCard.exportSettingsController.exportSymbol : false
            onCheckedChanged: {
                if (altiumCard.exportSettingsController)
                    altiumCard.exportSettingsController.setExportSymbol(checked);
            }
        }

        StyledCheckBox {
            text: qsTranslate("MainWindow", "封装库 (.PcbLib)")
            ToolTip.text: qsTranslate("MainWindow", "导出 Altium .PcbLib 封装库文件")
            checked: altiumCard.exportSettingsController ? altiumCard.exportSettingsController.exportFootprint : false
            onCheckedChanged: {
                if (altiumCard.exportSettingsController)
                    altiumCard.exportSettingsController.setExportFootprint(checked);
            }
        }

        StyledCheckBox {
            text: qsTranslate("MainWindow", "3D模型 (STEP)")
            ToolTip.text: qsTranslate("MainWindow", "导出 STEP 格式 3D 模型")
            checked: altiumCard.exportSettingsController ? altiumCard.exportSettingsController.exportModel3D : false
            onCheckedChanged: {
                if (altiumCard.exportSettingsController) {
                    altiumCard.exportSettingsController.setExportModel3D(checked);
                    if (checked) altiumCard.exportSettingsController.setExportModel3DFormat(2);
                }
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
