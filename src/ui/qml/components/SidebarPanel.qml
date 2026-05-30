import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"
import EasyKiconverter_Cpp_Version 1.0

Rectangle {
    id: root
    color: "transparent"
    // 折叠状态
    property bool collapsed: false
    signal toggleCollapsed
    // 直接使用全局 Context Property
    readonly property var progressController: exportProgressViewModel
    readonly property var settingsController: exportSettingsViewModel
    readonly property var listController: componentListViewModel
    readonly property bool isExporting: progressController ? progressController.isExporting : false
    readonly property int failureCount: progressController ? progressController.failureCount : 0
    // 信号
    signal requestOutputFolderDialog
    signal requestCacheFolderDialog
    // 玻璃态模糊背景
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

    // 分隔线 + 折叠拨片（z: 100 确保在内容层之上）
    Rectangle {
        id: separator
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: AppStyle.colors.border
        z: 100
        // 折叠/展开拨片
        Rectangle {
            id: grabber
            anchors.right: parent.right
            anchors.rightMargin: -16
            anchors.verticalCenter: parent.verticalCenter
            width: 32
            height: 52
            radius: 4
            color: grabberMouse.containsMouse ? AppStyle.colors.primary : (AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.06))
            border.width: AppStyle.borderWidths.thin
            border.color: AppStyle.colors.border
            Behavior on color {
                ColorAnimation {
                    duration: AppStyle.durations.fast
                }
            }

            Text {
                anchors.centerIn: parent
                text: root.collapsed ? "▶" : "◀"
                font.pixelSize: 10
                color: grabberMouse.containsMouse ? AppStyle.colors.textOnPrimary : AppStyle.colors.textSecondary
                rotation: 0
            }

            MouseArea {
                id: grabberMouse
                anchors.fill: parent
                anchors.topMargin: -20
                anchors.bottomMargin: -20
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                preventStealing: true
                onClicked: root.toggleCollapsed()
                onDoubleClicked: root.toggleCollapsed()
            }

            ToolTip.visible: grabberMouse.containsMouse
            ToolTip.text: root.collapsed ? qsTranslate("MainWindow", "展开侧边栏") : qsTranslate("MainWindow", "收缩侧边栏")
            ToolTip.delay: 600
        }
    }

    // ==================== 收缩态：纯导航栏 ====================
    ColumnLayout {
        id: collapsedColumn
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.xs
        spacing: AppStyle.spacing.sm
        visible: root.collapsed
        opacity: root.collapsed ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }

        // 设置图标（点击展开）
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.margins: 4
            radius: AppStyle.radius.sm
            color: settingsIconMouse.containsMouse ? (AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.06)) : "transparent"
            Text {
                anchors.centerIn: parent
                text: "⚙"
                font.pixelSize: 18
                color: AppStyle.colors.textSecondary
            }

            MouseArea {
                id: settingsIconMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleCollapsed()
            }

            ToolTip.visible: settingsIconMouse.containsMouse
            ToolTip.text: qsTranslate("MainWindow", "导出设置")
            ToolTip.delay: 400
        }

        Item {
            Layout.fillHeight: true
        }
    }

    // ==================== 展开态：完整面板 ====================
    ColumnLayout {
        id: expandedColumn
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0
        visible: !root.collapsed
        opacity: root.collapsed ? 0 : 1
        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }

        // 可滚动区域
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
                    exportSettingsController: root.settingsController
                    onOpenOutputFolderDialog: root.requestOutputFolderDialog()
                    onOpenCacheFolderDialog: root.requestCacheFolderDialog()
                    opacity: root.collapsed ? 0 : 1
                    x: root.collapsed ? -20 : 0
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 250
                        }
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: 250
                            easing.type: Easing.OutQuart
                        }
                    }
                }
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: AppStyle.colors.border
        }

        // 固定底部控制面板
        Rectangle {
            id: controlPanel
            Layout.fillWidth: true
            implicitHeight: controlColumn.implicitHeight + AppStyle.spacing.lg * 2
            color: AppStyle.isDarkMode ? Qt.rgba(30 / 255, 41 / 255, 59 / 255, 0.5) : Qt.rgba(241 / 255, 245 / 255, 249 / 255, 0.5)
            ColumnLayout {
                id: controlColumn
                anchors.fill: parent
                anchors.margins: AppStyle.spacing.lg
                spacing: AppStyle.spacing.md
                // 按钮组
                RowLayout {
                    Layout.fillWidth: true
                    spacing: AppStyle.spacing.md
                    ModernButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        text: {
                            if (root.isExporting)
                                return qsTranslate("MainWindow", "导出中");
                            if (root.failureCount > 0)
                                return qsTranslate("MainWindow", "重试失败");
                            return qsTranslate("MainWindow", "开始导出");
                        }
                        backgroundColor: root.failureCount > 0 ? AppStyle.colors.warning : AppStyle.colors.primary
                        onClicked: {
                            var pc = root.progressController;
                            var sc = root.settingsController;
                            var lc = root.listController;
                            if (pc && pc.failureCount > 0) {
                                pc.retryFailedComponents();
                            } else if (pc && sc && lc) {
                                pc.startExport(lc.getAllComponentIds(), sc.outputPath || "", sc.libName || "", sc.exportSymbol || false, sc.exportFootprint || false, sc.exportModel3D || false, sc.exportModel3DFormat || 3, sc.exportModel3DPathMode || 0, sc.exportPreviewImages || false, sc.exportDatasheet || false, sc.overwriteExistingFiles || false, (sc.exportMode || 0) === 1, sc.debugMode || false, sc.symbolLibraryDescription || "", sc.footprintLibraryDescription || "", sc.footprintLibraryKeywords || "");
                            }
                        }
                        enabled: {
                            if (root.isExporting)
                                return false;
                            if (!root.listController || root.listController.componentCount <= 0)
                                return false;
                            if (!root.settingsController)
                                return false;
                            return root.settingsController.exportSymbol || root.settingsController.exportFootprint || root.settingsController.exportModel3D;
                        }
                        ToolTip.visible: hovered && !enabled
                        ToolTip.text: {
                            if (root.isExporting)
                                return qsTranslate("MainWindow", "正在导出中...");
                            if (!root.listController || root.listController.componentCount <= 0)
                                return qsTranslate("MainWindow", "请先添加元器件");
                            if (!root.settingsController)
                                return "";
                            if (!root.settingsController.exportSymbol && !root.settingsController.exportFootprint && !root.settingsController.exportModel3D)
                                return qsTranslate("MainWindow", "请至少选择一种导出类型");
                            return "";
                        }
                        ToolTip.delay: 600
                    }

                    ModernButton {
                        visible: root.isExporting
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 44
                        iconName: "close"
                        backgroundColor: AppStyle.colors.danger
                        onClicked: {
                            if (root.progressController)
                                root.progressController.cancelExport();
                        }
                    }
                }
            }
        }
    }
}
