import QtQuick
import QtQuick.Controls
import "components"
import "styles"

ApplicationWindow {
    id: appWindow
    width: Screen.desktopAvailableWidth * 0.8
    height: Screen.desktopAvailableHeight * 0.8
    // x: (Screen.width - width) / 2  <-- Removed to prevent binding issues with frameless window
    // y: (Screen.height - height) / 2 <-- Removed
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: "EasyKiConverter - 元器件转换工具"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint

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
            exportProgressViewModel.handleCloseRequest();
            close.accepted = true;
        }
    }

    // Move to center on startup to ensure window manager registers the position correctly
    Component.onCompleted: {
        x = (Screen.width - width) / 2;
        y = (Screen.height - height) / 2;
    }

    // 加载主窗口内容
    Loader {
        anchors.fill: parent
        source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/MainWindow.qml"
    }
}
