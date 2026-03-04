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

    // 错误提示对话框
    Dialog {
        id: errorDialog
        parent: exportButtonsSection.Window.contentItem
        modal: true
        title: qsTranslate("MainWindow", "错误")
        anchors.centerIn: parent

        contentItem: Text {
            text: qsTranslate("MainWindow", "打开导出目录失败，请检查导出路径是否存在。")
            font.pixelSize: AppStyle.fontSizes.md
            color: AppStyle.colors.textPrimary
            horizontalAlignment: Text.AlignHCenter
        }

        background: Rectangle {
            color: AppStyle.colors.surface
            radius: AppStyle.radius.lg
            border.color: AppStyle.colors.border
            border.width: 1
        }

        standardButtons: Dialog.Ok

        onAccepted: {
            close();
        }
    }
    // 打开导出目录按钮（始终显示，只要导出已完成）
    ModernButton {
        Layout.fillWidth: true
        Layout.topMargin: AppStyle.spacing.sm
        text: qsTranslate("MainWindow", "打开导出目录")
        iconName: "folder"
        backgroundColor: AppStyle.colors.primary
        hoverColor: AppStyle.colors.primaryHover
        pressedColor: AppStyle.colors.primaryPressed
        visible: exportButtonsSection.exportProgressController && exportButtonsSection.exportProgressController.statisticsTotal > 0 // 只要有导出过就显示
        onClicked: {
            if (exportButtonsSection.exportProgressController) {
                var success = exportButtonsSection.exportProgressController.openLastExportedFolder();
                if (!success) {
                    errorDialog.open();
                }
            }
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
                var progressController = exportButtonsSection.exportProgressController;
                if (progressController && progressController.isExporting)
                    return qsTranslate("MainWindow", "正在转换...");
                if (progressController && progressController.failureCount > 0)
                    return qsTranslate("MainWindow", "重试失败项");
                return qsTranslate("MainWindow", "开始转换");
            }

            iconName: (exportButtonsSection.exportProgressController && exportButtonsSection.exportProgressController.failureCount > 0 && !exportButtonsSection.exportProgressController.isExporting) ? "play" : "play"
            font.pixelSize: AppStyle.fontSizes.xxl
            backgroundColor: {
                var progressController = exportButtonsSection.exportProgressController;
                if (progressController && progressController.isExporting)
                    return AppStyle.colors.textDisabled;
                if (progressController && progressController.failureCount > 0)
                    return AppStyle.colors.warning;
                return AppStyle.colors.primary;
            }
            hoverColor: {
                var progressController = exportButtonsSection.exportProgressController;
                if (progressController && progressController.failureCount > 0 && !(progressController && progressController.isExporting))
                    return AppStyle.colors.warningDark;
                return AppStyle.colors.primaryHover;
            }
            pressedColor: {
                var progressController = exportButtonsSection.exportProgressController;
                if (progressController && progressController.failureCount > 0 && !(progressController && progressController.isExporting))
                    return AppStyle.colors.warningDark;
                return AppStyle.colors.primaryPressed;
            }

            // 导出进行中时禁用此按钮
            enabled: {
                var progressController = exportButtonsSection.exportProgressController;
                var listController = exportButtonsSection.componentListController;
                var settingsController = exportButtonsSection.exportSettingsController;
                if (progressController && progressController.isExporting)
                    return false;
                if (!listController || listController.componentCount <= 0)
                    return false;
                if (!settingsController)
                    return false;
                return settingsController.exportSymbol || settingsController.exportFootprint || settingsController.exportModel3D;
            }
            onClicked: {
                var progressController = exportButtonsSection.exportProgressController;
                if (progressController && progressController.failureCount > 0) {
                    progressController.retryFailedComponents();
                } else {
                    // 提取 Component ID 列表
                    var idList = exportButtonsSection.componentListController ? exportButtonsSection.componentListController.getAllComponentIds() : [];
                    var settingsController = exportButtonsSection.exportSettingsController;
                    if (progressController && settingsController) {
                        progressController.startExport(idList, settingsController.outputPath || "", settingsController.libName || "", settingsController.exportSymbol || false, settingsController.exportFootprint || false, settingsController.exportModel3D || false, settingsController.overwriteExistingFiles || false, (settingsController.exportMode || 0) === 1, settingsController.debugMode || false);
                    }
                }
            }
        }

        // "停止转换"按钮
        ModernButton {
            id: stopButton
            Layout.preferredHeight: 56
            Layout.preferredWidth: 180
            // 仅在导出进行时可见
            visible: exportButtonsSection.exportProgressController && exportButtonsSection.exportProgressController.isExporting
            text: (exportButtonsSection.exportProgressController && exportButtonsSection.exportProgressController.isStopping) ? qsTranslate("MainWindow", "正在停止...") : qsTranslate("MainWindow", "停止转换")
            iconName: "close"
            font.pixelSize: AppStyle.fontSizes.xl
            backgroundColor: AppStyle.colors.danger
            hoverColor: AppStyle.colors.dangerDark
            pressedColor: AppStyle.colors.dangerDark
            enabled: exportButtonsSection.exportProgressController ? !exportButtonsSection.exportProgressController.isStopping : false
            onClicked: {
                if (exportButtonsSection.exportProgressController) {
                    exportButtonsSection.exportProgressController.cancelExport();
                }
            }
        }
    }
}
