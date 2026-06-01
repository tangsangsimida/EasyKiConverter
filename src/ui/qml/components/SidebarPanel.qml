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
    readonly property bool hasCompletedExport: progressController ? progressController.hasCompletedExport : false
    // 信号
    signal requestOutputFolderDialog
    signal requestCacheFolderDialog
    function resetScrollPosition() {
        if (scrollArea) {
            scrollArea.cancelFlick();
            scrollArea.contentY = 0;
        }
    }

    onCollapsedChanged: {
        if (!collapsed) {
            Qt.callLater(resetScrollPosition);
        }
    }

    onVisibleChanged: {
        if (visible && !collapsed) {
            Qt.callLater(resetScrollPosition);
        }
    }

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

    // 分隔线（纯视觉，无交互）
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: AppStyle.colors.border
        z: 100
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
            ToolTip.text: qsTranslate("MainWindow", "展开设置")
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

        // 顶部标题栏（标题 + 收起按钮）
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "transparent"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: AppStyle.spacing.md
                anchors.rightMargin: AppStyle.spacing.xs
                spacing: AppStyle.spacing.sm
                Text {
                    text: qsTranslate("MainWindow", "导出设置")
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.bold: true
                    color: AppStyle.colors.textSecondary
                    Layout.fillWidth: true
                }

                // 收起按钮
                Rectangle {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    radius: AppStyle.radius.sm
                    color: collapseBtnMouse.containsMouse ? (AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.06)) : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "«"
                        font.pixelSize: 14
                        font.bold: true
                        color: collapseBtnMouse.containsMouse ? AppStyle.colors.textPrimary : AppStyle.colors.textSecondary
                    }

                    MouseArea {
                        id: collapseBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.toggleCollapsed()
                    }

                    ToolTip.visible: collapseBtnMouse.containsMouse
                    ToolTip.text: qsTranslate("MainWindow", "收起设置")
                    ToolTip.delay: 400
                }
            }
            // 底部分隔线
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: AppStyle.colors.border
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

                SidebarExportControls {
                    Layout.fillWidth: true
                    exportProgressController: root.progressController
                    exportSettingsController: root.settingsController
                    componentListController: root.listController
                }
            }
        }
    }
}
