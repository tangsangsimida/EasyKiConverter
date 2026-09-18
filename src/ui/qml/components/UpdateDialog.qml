import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 显示版本信息并承载版本更新操作。
 *
 * 版本号位于标题栏，因此更新相关操作集中在此对话框中，侧边栏只保留导出设置。
 */
Dialog {
    id: root
    property var updateChecker
    modal: true
    focus: true
    width: Math.min(parent ? parent.width - AppStyle.spacing.xxl : 520, 520)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: AppStyle.colors.surface
        border.color: AppStyle.colors.border
        border.width: AppStyle.borderWidths.thin
        radius: AppStyle.radius.lg
    }

    contentItem: ColumnLayout {
        spacing: AppStyle.spacing.lg

        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.xs

            Text {
                Layout.fillWidth: true
                text: qsTranslate("MainWindow", "版本更新")
                color: AppStyle.colors.textPrimary
                font.pixelSize: AppStyle.fontSizes.xl
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: updateChecker ? qsTranslate("MainWindow", "当前版本 %1").arg(updateChecker.currentVersion) : ""
                color: AppStyle.colors.textSecondary
                font.pixelSize: AppStyle.fontSizes.sm
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: AppStyle.colors.border
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.sm

            Text {
                Layout.fillWidth: true
                text: {
                    if (!updateChecker)
                        return "";
                    if (updateChecker.statusText === "checking")
                        return qsTranslate("MainWindow", "正在检查更新");
                    if (updateChecker.statusText === "failed")
                        return qsTranslate("MainWindow", "更新检查失败，可稍后重试。");
                    if (updateChecker.statusText === "up_to_date")
                        return qsTranslate("MainWindow", "已是最新版本");
                    if (updateChecker.statusText === "ignored")
                        return qsTranslate("MainWindow", "已忽略当前版本");
                    if (updateChecker.statusText === "update_available")
                        return qsTranslate("MainWindow", "发现新版本 %1").arg(updateChecker.latestVersion);
                    return qsTranslate("MainWindow", "尚未检查");
                }
                color: updateChecker && updateChecker.statusText === "failed" ? AppStyle.colors.danger : AppStyle.colors.textPrimary
                font.pixelSize: AppStyle.fontSizes.md
                font.bold: true
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                text: {
                    if (!updateChecker)
                        return "";
                    if (updateChecker.error && updateChecker.statusText === "failed")
                        return updateChecker.error;
                    if (updateChecker.releaseName)
                        return qsTranslate("MainWindow", "当前版本 %1，最新发布：%2").arg(updateChecker.currentVersion).arg(updateChecker.releaseName);
                    return qsTranslate("MainWindow", "当前版本 %1，可前往 GitHub 查看发布说明。").arg(updateChecker.currentVersion);
                }
                color: AppStyle.colors.textSecondary
                font.pixelSize: AppStyle.fontSizes.sm
                wrapMode: Text.Wrap
            }
        }

        SidebarToggleRow {
            Layout.fillWidth: true
            label: qsTranslate("MainWindow", "启动时自动检查")
            checked: updateChecker ? updateChecker.autoCheckEnabled : true
            onToggled: {
                if (updateChecker)
                    updateChecker.setAutoCheckEnabled(checked);
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.sm

            ModernButton {
                objectName: "dialogUpdateActionButton"
                Layout.preferredHeight: 38
                text: updateChecker && updateChecker.statusText === "failed" ? qsTranslate("MainWindow", "重试") : qsTranslate("MainWindow", "检查更新")
                enabled: updateChecker ? !updateChecker.checking : false
                onClicked: {
                    if (updateChecker)
                        updateChecker.checkForUpdates();
                }
            }

            ModernButton {
                objectName: "dialogViewReleaseButton"
                Layout.preferredHeight: 38
                visible: updateChecker && updateChecker.hasUpdate
                text: qsTranslate("MainWindow", "查看更新")
                backgroundColor: AppStyle.colors.textSecondary
                hoverColor: AppStyle.colors.textPrimary
                pressedColor: AppStyle.colors.textPrimary
                onClicked: {
                    if (updateChecker)
                        Qt.openUrlExternally(updateChecker.assetUrl || updateChecker.releaseUrl);
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                objectName: "closeUpdateDialogButton"
                text: qsTranslate("MainWindow", "取消")
                onClicked: root.close()
            }
        }
    }
}
