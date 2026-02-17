import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: bomImportCard
    // 外部依赖
    property var componentListController
    // 信号：请求打开BOM文件对话框
    signal openBomFileDialog
    title: qsTranslate("MainWindow", "导入BOM文件")
    RowLayout {
        width: parent.width
        spacing: 12
        ModernButton {
            text: qsTranslate("MainWindow", "选择BOM文件")
            iconName: "upload"
            backgroundColor: AppStyle.colors.success
            hoverColor: AppStyle.colors.successDark
            pressedColor: AppStyle.colors.successDark
            onClicked: {
                bomImportCard.openBomFileDialog();
            }
        }
        Text {
            id: bomFileLabel
            Layout.fillWidth: true
            text: bomImportCard.componentListController.bomFilePath.length > 0 ? bomImportCard.componentListController.bomFilePath.split("/").pop() : qsTranslate("MainWindow", "未选择文件")
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textSecondary
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
        }
    }
    // BOM导入结果
    Text {
        id: bomResultLabel
        Layout.fillWidth: true
        Layout.topMargin: AppStyle.spacing.md
        text: bomImportCard.componentListController.bomResult
        font.pixelSize: AppStyle.fontSizes.sm
        color: AppStyle.colors.success
        horizontalAlignment: Text.AlignHCenter
        visible: bomImportCard.componentListController.bomResult.length > 0
    }
}
