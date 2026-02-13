import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0
import EasyKiconverter_Cpp_Version 1.0

ColumnLayout {
    id: exportButtonsSection

    // 外部依赖
    property var exportProgressController
    property var exportSettingsController
    property var componentListController

    spacing: AppStyle.spacing.md

    // 打开导出目录按钮（始终显示，只要导出已完成）
    ModernButton {
        Layout.fillWidth: true
        Layout.topMargin: AppStyle.spacing.sm
        text: qsTranslate("MainWindow", "打开导出目录")
        iconName: "folder"
        backgroundColor: AppStyle.colors.primary
        hoverColor: AppStyle.colors.primaryHover
        pressedColor: AppStyle.colors.primaryPressed
        visible: exportButtonsSection.exportProgressController.statisticsTotal > 0 // 只要有导出过就显示

        onClicked: {
            Qt.openUrlExternally("file:///" + exportButtonsSection.exportSettingsController.outputPath);
        }
    }
    // 导出按钮组
    RowLayout {
        Layout.fillWidth: true
        spacing: AppStyle.spacing.md

        // "开始转换"或"重试"按钮
        ModernButton {
            id: exportButton
            Layout.preferredHeight: 56
            Layout.fillWidth: true

            // 根据是否有失败项来决定按钮文本
            text: {
                var lang = LanguageManager.currentLanguage; // Force update on language change
                if (exportButtonsSection.exportProgressController.isExporting)
                    return qsTranslate("MainWindow", "正在转换...");
                if (exportButtonsSection.exportProgressController.failureCount > 0)
                    return qsTranslate("MainWindow", "重试失败项");
                return qsTranslate("MainWindow", "开始转换");
            }

            iconName: exportButtonsSection.exportProgressController.failureCount > 0 && !exportButtonsSection.exportProgressController.isExporting ? "play" : "play"
            font.pixelSize: AppStyle.fontSizes.xxl

            backgroundColor: {
                if (exportButtonsSection.exportProgressController.isExporting)
                    return AppStyle.colors.textDisabled;
                if (exportButtonsSection.exportProgressController.failureCount > 0)
                    return AppStyle.colors.warning;
                return AppStyle.colors.primary;
            }
            hoverColor: {
                if (exportButtonsSection.exportProgressController.failureCount > 0 && !exportButtonsSection.exportProgressController.isExporting)
                    return AppStyle.colors.warningDark;
                return AppStyle.colors.primaryHover;
            }
            pressedColor: {
                if (exportButtonsSection.exportProgressController.failureCount > 0 && !exportButtonsSection.exportProgressController.isExporting)
                    return AppStyle.colors.warningDark;
                return AppStyle.colors.primaryPressed;
            }

            // 导出进行中时禁用此按钮
            enabled: !exportButtonsSection.exportProgressController.isExporting && (exportButtonsSection.componentListController.componentCount > 0 && (exportButtonsSection.exportSettingsController.exportSymbol || exportButtonsSection.exportSettingsController.exportFootprint || exportButtonsSection.exportSettingsController.exportModel3D))

            onClicked: {
                if (exportButtonsSection.exportProgressController.failureCount > 0) {
                    exportButtonsSection.exportProgressController.retryFailedComponents();
                } else {
                    // 提取 Component ID 列表
                    var idList = exportButtonsSection.componentListController.getAllComponentIds();

                    exportButtonsSection.exportProgressController.startExport(idList, exportButtonsSection.exportSettingsController.outputPath, exportButtonsSection.exportSettingsController.libName, exportButtonsSection.exportSettingsController.exportSymbol, exportButtonsSection.exportSettingsController.exportFootprint, exportButtonsSection.exportSettingsController.exportModel3D, exportButtonsSection.exportSettingsController.overwriteExistingFiles, exportButtonsSection.exportSettingsController.exportMode === 1, exportButtonsSection.exportSettingsController.debugMode);
                }
            }
        }

        // "停止转换"按钮
        ModernButton {
            id: stopButton
            Layout.preferredHeight: 56
            Layout.preferredWidth: 180

            // 仅在导出进行时可见
            visible: exportButtonsSection.exportProgressController.isExporting

            text: exportButtonsSection.exportProgressController.isStopping ? qsTranslate("MainWindow", "正在停止...") : qsTranslate("MainWindow", "停止转换")
            iconName: "close"
            font.pixelSize: AppStyle.fontSizes.xl

            backgroundColor: AppStyle.colors.danger
            hoverColor: AppStyle.colors.dangerDark
            pressedColor: AppStyle.colors.dangerDark

            enabled: !exportButtonsSection.exportProgressController.isStopping

            onClicked: {
                exportButtonsSection.exportProgressController.cancelExport();
            }
        }
    }
}
