import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

/**
 * @brief 统一样式复选框
 * @details 提取自 ExportSettingsCard 等文件中重复的 CheckBox 样式。
 *          所有导出设置卡片统一使用此组件。
 */
CheckBox {
    id: control

    font.pixelSize: AppStyle.fontSizes.sm
    ToolTip.visible: hovered

    indicator: Rectangle {
        implicitWidth: AppStyle.sizes.checkbox
        implicitHeight: AppStyle.sizes.checkbox
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: AppStyle.radius.xs
        color: control.checked ? AppStyle.colors.primary : "transparent"
        border.color: control.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
        border.width: AppStyle.borderWidths.normal

        Text {
            anchors.centerIn: parent
            text: "✓"
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textOnPrimary
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: AppStyle.colors.textPrimary
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
