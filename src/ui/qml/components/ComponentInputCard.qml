import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: componentInputCard

    // 外部依赖
    property var componentListController

    title: qsTranslate("MainWindow", "添加元器件")

    RowLayout {
        width: parent.width
        spacing: 12
        TextField {
            id: componentInput
            Layout.fillWidth: true
            placeholderText: qsTranslate("MainWindow", "输入LCSC元件编号 (例如: C2040)")
            font.pixelSize: AppStyle.fontSizes.md
            color: AppStyle.colors.textPrimary
            placeholderTextColor: AppStyle.colors.textSecondary
            background: Rectangle {
                color: componentInput.enabled ? AppStyle.colors.surface : AppStyle.colors.background
                border.color: componentInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                border.width: componentInput.focus ? 2 : 1
                radius: AppStyle.radius.md
                Behavior on border.color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
                Behavior on border.width {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            onAccepted: {
                if (componentInput.text.length > 0) {
                    componentInputCard.componentListController.addComponent(componentInput.text);
                    componentInput.text = "";
                }
            }
        }
        ModernButton {
            text: qsTranslate("MainWindow", "添加")
            iconName: "add"
            enabled: componentInput.text.length > 0
            onClicked: {
                if (componentInput.text.length > 0) {
                    componentInputCard.componentListController.addComponent(componentInput.text);
                    componentInput.text = "";
                }
            }
        }
        ModernButton {
            text: qsTranslate("MainWindow", "粘贴")
            iconName: "folder"
            backgroundColor: AppStyle.colors.textSecondary
            hoverColor: AppStyle.colors.textPrimary
            pressedColor: AppStyle.colors.textPrimary
            onClicked: {
                componentInputCard.componentListController.pasteFromClipboard();
            }
        }
    }
}
