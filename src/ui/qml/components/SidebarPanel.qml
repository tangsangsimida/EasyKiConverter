import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"

Rectangle {
    id: root
    property var window: null // Reference to root window for dialogs
    color: "transparent"
    // 玻璃态模糊背景（独立层，不模糊内容）
    Rectangle {
        id: glassBg
        anchors.fill: parent
        color: AppStyle.isDarkMode ? Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.55) : Qt.rgba(255, 255, 255, 0.55)
        z: -1
        layer.enabled: true
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: 0.6
            blurMax: 32
        }

        Behavior on color {
            ColorAnimation {
                duration: AppStyle.durations.themeSwitch
            }
        }
    }

    // 侧边栏与工作区的分隔线
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: AppStyle.colors.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0
        // ==================== 可滚动区域 ====================
        Flickable {
            id: scrollArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: contentColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            ColumnLayout {
                id: contentColumn
                width: parent.width - AppStyle.spacing.lg * 2
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: AppStyle.spacing.lg
                spacing: AppStyle.spacing.xl
                // 侧边栏导出设置
                SidebarSettingsView {
                    Layout.fillWidth: true
                    exportSettingsController: root.window ? root.window.exportSettingsController : null
                    onOpenOutputFolderDialog: if (root.window)
                        root.window.outputFolderDialog.open()
                    onOpenCacheFolderDialog: if (root.window)
                        root.window.cacheFolderDialog.open()
                    // 动感：横向进入
                    opacity: root.visible ? 1 : 0
                    x: root.visible ? 0 : -20
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 400
                        }
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: 400
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                // 统计信息卡片 (侧边栏简化版)
                Loader {
                    Layout.fillWidth: true
                    active: root.window && root.window.exportProgressController && root.window.exportProgressController.hasCompletedExport
                    sourceComponent: SidebarSection {
                        title: qsTranslate("MainWindow", "执行统计")
                        RowLayout {
                            Layout.fillWidth: true
                            StatItem {
                                label: qsTranslate("MainWindow", "成功")
                                value: root.window.exportProgressController.successCount
                                color: AppStyle.colors.success
                            }
                            StatItem {
                                label: qsTranslate("MainWindow", "失败")
                                value: root.window.exportProgressController.failureCount
                                color: AppStyle.colors.danger
                            }
                        }
                    }
                }
            }
        }

        // ==================== 固定底部的控制面板 ====================
        Rectangle {
            id: controlPanel
            Layout.fillWidth: true
            implicitHeight: controlColumn.implicitHeight + AppStyle.spacing.lg * 2
            color: AppStyle.isDarkMode ? Qt.rgba(30 / 255, 41 / 255, 59 / 255, 0.5) : Qt.rgba(241 / 255, 245 / 255, 249 / 255, 0.5)
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: AppStyle.colors.border
            }

            ColumnLayout {
                id: controlColumn
                anchors.fill: parent
                anchors.margins: AppStyle.spacing.lg
                spacing: AppStyle.spacing.md
                // 进度条
                Loader {
                    Layout.fillWidth: true
                    active: root.window && root.window.exportProgressController && (root.window.exportProgressController.isExporting || root.window.exportProgressController.progress > 0)
                    sourceComponent: ColumnLayout {
                        spacing: 4
                        RowLayout {
                            Text {
                                text: root.window.exportProgressController.isExporting ? qsTranslate("MainWindow", "导出中...") : qsTranslate("MainWindow", "导出完成")
                                font.pixelSize: AppStyle.fontSizes.xs
                                color: AppStyle.colors.textSecondary
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Text {
                                text: root.window.exportProgressController.progress + "%"
                                font.pixelSize: AppStyle.fontSizes.xs
                                font.bold: true
                                color: AppStyle.colors.primary
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 6
                            radius: 3
                            color: AppStyle.colors.border
                            Rectangle {
                                width: parent.width * (root.window.exportProgressController.progress / 100)
                                height: parent.height
                                radius: 3
                                color: AppStyle.colors.primary
                                Behavior on width {
                                    NumberAnimation {
                                        duration: 150
                                    }
                                }
                            }
                        }
                    }
                }

                // 按钮组
                RowLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.md
                    ModernButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        text: {
                            var pc = root.window.exportProgressController;
                            if (pc && pc.isExporting)
                                return qsTranslate("MainWindow", "导出中");
                            if (pc && pc.failureCount > 0)
                                return qsTranslate("MainWindow", "重试失败");
                            return qsTranslate("MainWindow", "开始导出");
                        }
                        backgroundColor: (root.window.exportProgressController && root.window.exportProgressController.failureCount > 0) ? AppStyle.colors.warning : AppStyle.colors.primary
                        onClicked: {
                            var pc = root.window.exportProgressController;
                            var sc = root.window.exportSettingsController;
                            var lc = root.window.componentListController;
                            if (pc && pc.failureCount > 0) {
                                pc.retryFailedComponents();
                            } else if (pc && sc && lc) {
                                pc.startExport(lc.getAllComponentIds(), sc.outputPath, sc.libName, sc.exportSymbol, sc.exportFootprint, sc.exportModel3D, sc.exportModel3DFormat, sc.exportModel3DPathMode, sc.exportPreviewImages, sc.exportDatasheet, sc.overwriteExistingFiles, sc.exportMode === 1, sc.debugMode);
                            }
                        }
                        enabled: root.window && root.window.exportProgressController && !root.window.exportProgressController.isExporting && root.window.componentListController.componentCount > 0
                    }

                    ModernButton {
                        visible: root.window && root.window.exportProgressController && root.window.exportProgressController.isExporting
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 44
                        iconName: "close"
                        backgroundColor: AppStyle.colors.danger
                        onClicked: root.window.exportProgressController.cancelExport()
                    }
                }

                ModernButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    visible: root.window && root.window.exportProgressController && root.window.exportProgressController.hasCompletedExport
                    text: qsTranslate("MainWindow", "查看文件夹")
                    backgroundColor: AppStyle.colors.textSecondary
                    onClicked: root.window.exportProgressController.openLastExportedFolder()
                }
            }
        }
    }

    // ==================== 辅助组件 ====================
    component StatItem: ColumnLayout {
        property string label: ""
        property var value: 0
        property color color: AppStyle.colors.textPrimary
        spacing: 2
        Text {
            text: label
            font.pixelSize: AppStyle.fontSizes.xs
            color: AppStyle.colors.textSecondary
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: value.toString()
            font.pixelSize: AppStyle.fontSizes.lg
            font.bold: true
            color: parent.color
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
