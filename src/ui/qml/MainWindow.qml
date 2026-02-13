import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import QtQuick.Effects
import QtQml.Models
import "styles"
import "components"
import EasyKiconverter_Cpp_Version 1.0

Item {
    id: window
    // 连接到 ViewModel
    property var componentListController: componentListViewModel
    property var exportSettingsController: exportSettingsViewModel
    property var exportProgressController: exportProgressViewModel
    property var themeController: themeSettingsViewModel

    // 窗口状态属性
    readonly property bool isMaximized: Window.window ? (Window.window.visibility === Window.Maximized || Window.window.visibility === Window.FullScreen) : false
    readonly property int windowRadius: isMaximized ? 0 : AppStyle.radius.lg

    // 绑定 AppStyle.isDarkMode 到 themeSettingsViewModel.isDarkMode
    Binding {
        target: AppStyle
        property: "isDarkMode"
        value: themeSettingsViewModel.isDarkMode
    }
    // BOM 文件选择对话框
    FileDialog {
        id: bomFileDialog
        title: qsTr("选择 BOM 文件")
        nameFilters: ["Supported files (*.txt *.csv *.xlsx *.xls)", "Text files (*.txt)", "CSV files (*.csv)", "Excel files (*.xlsx *.xls)", "All files (*.*)"]
        onAccepted: {
            componentListController.selectBomFile(selectedFile);
        }
    }
    // 输出路径选择对话框
    FolderDialog {
        id: outputFolderDialog
        title: qsTr("选择输出目录")
        onAccepted: {
            // 从 URL 中提取本地路径
            var path = selectedFolder.toString();
            if (path.startsWith("file:///")) {
                path = path.substring(8); // 移除 "file:///" 前缀
            }
            exportSettingsController.setOutputPath(path);
        }
    }

    // 主容器
    Rectangle {
        id: mainContainer
        anchors.fill: parent
        color: "transparent"
        radius: AppStyle.radius.lg

        // 源图片（用于 Canvas 绘制）
        Image {
            id: bgSource
            visible: false
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/imgs/background.jpg"
            asynchronous: true
            cache: true
            onStatusChanged: if (status === Image.Ready)
                backgroundCanvas.requestPaint()
        }

        // 画布背景（实现圆角裁切）
        Canvas {
            id: backgroundCanvas
            anchors.fill: parent

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();

                // 绘制圆角路径
                var r = windowRadius;
                ctx.beginPath();
                ctx.roundedRect(0, 0, width, height, r, r);
                ctx.closePath();

                // 裁切
                ctx.clip();

                // 绘制背景色（作为底色）
                ctx.fillStyle = AppStyle.colors.background;
                ctx.fill();

                // 绘制图片
                if (bgSource.status === Image.Ready) {
                    // 模拟 PreserveAspectCrop
                    var sw = bgSource.sourceSize.width;
                    var sh = bgSource.sourceSize.height;
                    if (sw > 0 && sh > 0) {
                        var scale = Math.max(width / sw, height / sh);
                        var dw = sw * scale;
                        var dh = sh * scale;
                        var dx = (width - dw) / 2;
                        var dy = (height - dh) / 2;
                        ctx.drawImage(bgSource, dx, dy, dw, dh);
                    }
                }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onVisibleChanged: requestPaint()

            // 监听圆角变化
            property int radiusTrigger: windowRadius
            onRadiusTriggerChanged: requestPaint()
        }
        // 半透明遮罩层（确保内容可读性）
        Rectangle {
            anchors.fill: parent
            color: AppStyle.isDarkMode ? "#000000" : "#ffffff"
            opacity: AppStyle.isDarkMode ? 0.3 : 0.5
            radius: windowRadius
            enabled: false  // 不拦截鼠标事件
            Behavior on color {
                ColorAnimation {
                    duration: AppStyle.durations.themeSwitch
                    easing.type: AppStyle.easings.easeOut
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: AppStyle.durations.themeSwitch
                    easing.type: AppStyle.easings.easeOut
                }
            }
        }

        // 自定义标题栏
        TitleBar {
            id: titleBar
            windowRadius: window.windowRadius
        }

        // 主滚动区域
        ScrollView {
            id: scrollView
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            enabled: true  // 确保能传递鼠标事件
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            // 内容容器（添加左右边距）
            Item {
                width: scrollView.width
                implicitHeight: contentLayout.implicitHeight
                enabled: true  // 确保能传递鼠标事件
                // 内容区域
                ColumnLayout {
                    id: contentLayout
                    width: parent.width - AppStyle.spacing.huge * 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 30

                    // 头部区域（标题 + 语言选择器 + GitHub + 主题切换 + 分隔线）
                    HeaderSection {
                        Layout.fillWidth: true
                        themeController: window.themeController
                        componentListController: window.componentListController
                    }

                    // 元件输入卡片
                    ComponentInputCard {
                        Layout.fillWidth: true
                        componentListController: window.componentListController
                    }

                    // BOM导入卡片
                    BomImportCard {
                        Layout.fillWidth: true
                        componentListController: window.componentListController
                        onOpenBomFileDialog: bomFileDialog.open()
                    }

                    // 元件列表卡片
                    ComponentListCard {
                        Layout.fillWidth: true
                        componentListController: window.componentListController
                        exportProgressController: window.exportProgressController
                    }

                    // 导出设置卡片
                    ExportSettingsCard {
                        Layout.fillWidth: true
                        exportSettingsController: window.exportSettingsController
                        onOpenOutputFolderDialog: outputFolderDialog.open()
                    }

                    // 进度显示卡片
                    ExportProgressCard {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                    }

                    // 转换结果卡片（延迟加载）
                    ExportResultsCard {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                    }

                    // 导出统计卡片（仅在导出完成后显示）
                    ExportStatisticsCard {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                        exportSettingsController: window.exportSettingsController
                    }

                    // 导出按钮组
                    ExportButtonsSection {
                        Layout.fillWidth: true
                        exportProgressController: window.exportProgressController
                        exportSettingsController: window.exportSettingsController
                        componentListController: window.componentListController
                    }

                    // 底部边距
                    Item {
                        Layout.preferredHeight: 40
                    }
                }
            }
        }
    }

    // 窗口边缘调整大小手柄
    WindowResizeHandles {
        isMaximized: window.isMaximized
    }
}
