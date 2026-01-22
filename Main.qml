import QtQuick
import QtQuick.Controls
ApplicationWindow {
    id: appWindow
    width: 1200
    height: 800
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    minimumWidth: 1200
    minimumHeight: 800
    visible: true
    title: "EasyKiConverter - 元器件转换工具"
    // 加载主窗口内容
    Loader {
        anchors.fill: parent
        source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/MainWindow.qml"
    }
}
