import QtQuick
Item {
    id: root
    property string iconName: ""
    property int size: 24
    property color iconColor: "#000000"
    width: size
    height: size
    // 尝试加载图标
    Image {
        id: iconImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/" + iconName + ".svg"
        cache: true
        smooth: true
        antialiasing: true
        visible: status === Image.Ready
        onStatusChanged: {
            if (status === Image.Error) {
                console.warn("Icon not found:", iconName)
            }
        }
    }
    // 占位符（当图标不存在时显示）
    Text {
        anchors.centerIn: parent
        text: getIconSymbol(iconName)
        font.pixelSize: size * 0.6
        color: iconColor
        visible: !iconImage.visible
        font.bold: true
    }
    function getIconSymbol(name) {
        switch(name) {
            case "play": return "▶"
            case "folder": return "📁"
            case "trash": return "🗑"
            case "upload": return "↑"
            case "add": return "+"
            case "loading": return "⟳"
            default: return "?"
        }
    }
}
