import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

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

    // 确认关闭对话框
    MessageDialog {
        id: closeConfirmDialog
        title: qsTr("确认退出")
        text: qsTr("转换正在进行中。确定要退出吗？")
        informativeText: qsTr("退出将取消当前转换，已导出的文件会保留。")
        buttons: MessageDialog.Ok | MessageDialog.Cancel
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
