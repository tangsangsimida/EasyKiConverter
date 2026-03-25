import QtQuick

Item {
    id: root
    property string iconName: ""
    property string iconNameDark: ""
    property int size: 24
    property color iconColor: AppStyle.iconColors.primary
    width: size
    height: size
    implicitWidth: size
    implicitHeight: size

    function getIconSource() {
        if (iconName.length === 0)
            return "";
        var basePath = "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/";
        if (AppStyle.isDarkMode && iconNameDark.length > 0) {
            return basePath + iconNameDark + ".svg";
        }
        return basePath + iconName + ".svg";
    }

    Image {
        id: iconImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: getIconSource()
        cache: true
        smooth: true
        antialiasing: true
        visible: status === Image.Ready && iconName.length > 0
        onStatusChanged: {
            if (status === Image.Error && iconName.length > 0) {
                var basePath = "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/";
                iconImage.source = basePath + iconName + ".svg";
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: getIconSymbol(iconName)
        font.pixelSize: size * 0.6
        color: iconColor
        visible: !iconImage.visible
        font.bold: true
    }

    function getIconSymbol(name) {
        switch (name) {
        case "play":
            return "▶";
        case "folder":
            return "📁";
        case "trash":
            return "🗑";
        case "upload":
            return "↑";
        case "add":
            return "+";
        case "loading":
            return "⟳";
        default:
            return "?";
        }
    }
}
