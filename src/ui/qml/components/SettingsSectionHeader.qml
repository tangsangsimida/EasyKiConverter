import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 统一的区块标题组件
 * @details 提供带分隔线的区块标题，用于导出设置等卡片中。
 */
ColumnLayout {
    property string title
    Layout.fillWidth: true
    spacing: AppStyle.spacing.xs
    Text {
        Layout.fillWidth: true
        text: title
        font.pixelSize: AppStyle.fontSizes.md
        font.bold: true
        color: AppStyle.colors.textPrimary
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: AppStyle.colors.border
    }
}
