import QtQuick
import QtQuick.Controls
import "components"
import "styles"

ApplicationWindow {
    id: appWindow
    // 默认窗口大小为屏幕的 65%（如果配置中没有保存的值）
    property int defaultWidth: Math.max(800, Screen.desktopAvailableWidth * 0.65)
    property int defaultHeight: Math.max(600, Screen.desktopAvailableHeight * 0.65)

    width: configService ? (configService.getWindowWidth() > 0 ? configService.getWindowWidth() : defaultWidth) : defaultWidth
    height: configService ? (configService.getWindowHeight() > 0 ? configService.getWindowHeight() : defaultHeight) : defaultHeight
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: "EasyKiConverter - 元器件转换工具"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint | Qt.CustomizeWindowHint
    // 美化的确认关闭对话框
    ConfirmDialog {
        id: closeConfirmDialog
        title: qsTr("确认退出")
        message: qsTr("转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？")
        confirmText: qsTr("强制退出")
        cancelText: qsTr("继续转换")
        confirmColor: AppStyle.colors.danger
        onAccepted: {
            if (exportProgressViewModel.handleCloseRequest()) {
                Qt.quit();
            }
        }
    }

    onClosing: close => {
        if (exportProgressViewModel.isExporting) {
            close.accepted = false;
            closeConfirmDialog.open();
        } else {
            // 保存窗口大小和位置（仅在正常关闭时，不是强制退出）
            if (configService) {
                configService.setWindowWidth(width);
                configService.setWindowHeight(height);
                // 只在窗口不是最大化或全屏时保存位置
                if (visibility !== Window.Maximized && visibility !== Window.FullScreen) {
                    configService.setWindowX(x);
                    configService.setWindowY(y);
                }
            }
            exportProgressViewModel.handleCloseRequest();
            close.accepted = true;
        }
    }

    // 在启动时设置窗口位置
    Component.onCompleted: {
        // 窗口位置由 C++ 端控制
    }

    // 加载主窗口内容
    Loader {
        anchors.fill: parent
        source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/MainWindow.qml"
    }
}
