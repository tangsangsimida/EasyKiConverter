import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "components"
import "styles"

ApplicationWindow {
    id: appWindow

    property alias windowController: appWindowController
    property int defaultWidth: Math.max(900, Screen.desktopAvailableWidth * 0.68)
    property int defaultHeight: Math.max(680, Screen.desktopAvailableHeight * 0.72)
    property int windowRadius: visibility === Window.Maximized ? 0 : 10
    property int dynamicMinimumWidth: 855

    width: configService ? (configService.getWindowWidth() > 0 ? configService.getWindowWidth() : defaultWidth) : defaultWidth
    height: configService ? (configService.getWindowHeight() > 0 ? configService.getWindowHeight() : defaultHeight) : defaultHeight
    x: configService && configService.getWindowX() > 0 ? configService.getWindowX() : (Screen.desktopAvailableWidth - width) / 2
    y: configService && configService.getWindowY() > 0 ? configService.getWindowY() : (Screen.desktopAvailableHeight - height) / 2
    minimumWidth: dynamicMinimumWidth
    minimumHeight: 620
    visible: true
    title: "EasyKiConverter - 元器件转换工具"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.CustomizeWindowHint | Qt.WindowMinimizeButtonHint |
        Qt.WindowMaximizeButtonHint

    WindowController {
        id: appWindowController
        window: appWindow
        configService: configService
        exportProgressController: exportProgressViewModel
        closeConfirmDialog: closeConfirmDialog
        exitOptionDialog: exitOptionDialog
    }

    ConfirmDialog {
        id: closeConfirmDialog
        title: qsTr("确认退出")
        message: qsTr("转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？")
        confirmText: qsTr("强制退出")
        cancelText: qsTr("继续转换")
        confirmColor: AppStyle.colors.danger
        onAccepted: appWindowController.confirmExit()
    }

    ExitDialog {
        id: exitOptionDialog
        onMinimizeToTray: function (remember) {
            if (remember && configService) {
                configService.setExitPreference("minimize");
            }
            appWindowController.requestMinimize();
        }
        onExitApp: function (remember) {
            if (remember && configService) {
                configService.setExitPreference("exit");
            }
            appWindowController.confirmExit();
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: appWindowController.handleEsc()
    }

    onClosing: close => appWindowController.handleClosing(close)

    Component.onCompleted: {
        appWindowController.restoreWindowState();
    }

    Item {
        anchors.fill: parent

        Loader {
            id: mainWindowLoader
            anchors.fill: parent
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/MainWindow.qml"
            onLoaded: {
                if (item && item.calculatedMinimumWindowWidth) {
                    appWindow.dynamicMinimumWidth = item.calculatedMinimumWindowWidth;
                }
            }
        }

        Binding {
            target: appWindow
            property: "dynamicMinimumWidth"
            value: mainWindowLoader.item ? mainWindowLoader.item.calculatedMinimumWindowWidth : 855
            when: mainWindowLoader.status === Loader.Ready
        }
    }
}
