import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: exportSettingsCard
    // 外部依赖
    property var exportSettingsController
    // 信号：请求打开输出目录对话框
    signal openOutputFolderDialog
    title: qsTranslate("MainWindow", "导出设置")
    GridLayout {
        width: parent.width
        columns: 2
        columnSpacing: 20
        rowSpacing: 12
        // 输出路径
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTranslate("MainWindow", "输出路径")
                font.pixelSize: 14
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                TextField {
                    id: outputPathInput
                    Layout.fillWidth: true
                    text: exportSettingsCard.exportSettingsController.outputPath
                    onTextChanged: exportSettingsCard.exportSettingsController.setOutputPath(text)
                    placeholderText: qsTranslate("MainWindow", "选择输出目录")
                    font.pixelSize: 14
                    color: AppStyle.colors.textPrimary
                    placeholderTextColor: AppStyle.colors.textSecondary
                    background: Rectangle {
                        color: AppStyle.colors.surface
                        border.color: outputPathInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                        border.width: outputPathInput.focus ? 2 : 1
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
                }
                ModernButton {
                    text: qsTranslate("MainWindow", "浏览")
                    iconName: "folder"
                    font.pixelSize: AppStyle.fontSizes.sm
                    backgroundColor: AppStyle.colors.textSecondary
                    hoverColor: AppStyle.colors.textPrimary
                    pressedColor: AppStyle.colors.textPrimary
                    onClicked: {
                        exportSettingsCard.openOutputFolderDialog();
                    }
                }
            }
        }
        // 库名称
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTranslate("MainWindow", "库名称")
                font.pixelSize: 14
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }
            TextField {
                id: libNameInput
                Layout.fillWidth: true
                text: exportSettingsCard.exportSettingsController.libName
                onTextChanged: exportSettingsCard.exportSettingsController.setLibName(text)
                placeholderText: qsTranslate("MainWindow", "输入库名称 (例如: MyLibrary)")
                font.pixelSize: 14
                color: AppStyle.colors.textPrimary
                placeholderTextColor: AppStyle.colors.textSecondary
                background: Rectangle {
                    color: AppStyle.colors.surface
                    border.color: libNameInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                    border.width: libNameInput.focus ? 2 : 1
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
            }
        }
    }

    // 分隔
    Item {
        Layout.preferredHeight: 10
        Layout.fillWidth: true
    }

    // 原导出选项内容
    RowLayout {
        Layout.fillWidth: true
        width: parent.width
        spacing: 20
        // 符号库选项
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 100
            spacing: 8
            CheckBox {
                id: symbolCheckbox
                Layout.fillWidth: true
                text: qsTranslate("MainWindow", "符号库")
                checked: exportSettingsCard.exportSettingsController.exportSymbol
                onCheckedChanged: exportSettingsCard.exportSettingsController.setExportSymbol(checked)
                font.pixelSize: 16
                indicator: Rectangle {
                    implicitWidth: 22
                    implicitHeight: 22
                    x: symbolCheckbox.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: 4
                    color: symbolCheckbox.checked ? AppStyle.colors.primary : "transparent"
                    border.color: symbolCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: 1.5
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                    Behavior on border.color {
                        ColorAnimation {
                            duration: 150
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        font.pixelSize: 14
                        color: "#ffffff"
                        visible: symbolCheckbox.checked
                    }
                }

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: AppStyle.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
        }
        // 封装库选项
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 100
            spacing: 8
            CheckBox {
                id: footprintCheckbox
                Layout.fillWidth: true
                text: qsTranslate("MainWindow", "封装库")
                checked: exportSettingsCard.exportSettingsController.exportFootprint
                onCheckedChanged: exportSettingsCard.exportSettingsController.setExportFootprint(checked)
                font.pixelSize: 16
                indicator: Rectangle {
                    implicitWidth: 22
                    implicitHeight: 22
                    x: footprintCheckbox.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: 4
                    color: footprintCheckbox.checked ? AppStyle.colors.primary : "transparent"
                    border.color: footprintCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: 1.5
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                    Behavior on border.color {
                        ColorAnimation {
                            duration: 150
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        font.pixelSize: 14
                        color: "#ffffff"
                        visible: footprintCheckbox.checked
                    }
                }

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: AppStyle.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
        }
        // 3D模型选项
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 100
            spacing: 8
            CheckBox {
                id: model3dCheckbox
                Layout.fillWidth: true
                text: qsTranslate("MainWindow", "3D模型")
                checked: exportSettingsCard.exportSettingsController.exportModel3D
                onCheckedChanged: exportSettingsCard.exportSettingsController.setExportModel3D(checked)
                font.pixelSize: 16
                indicator: Rectangle {
                    implicitWidth: 22
                    implicitHeight: 22
                    x: model3dCheckbox.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: 4
                    color: model3dCheckbox.checked ? AppStyle.colors.primary : "transparent"
                    border.color: model3dCheckbox.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                    border.width: 1.5
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                    Behavior on border.color {
                        ColorAnimation {
                            duration: 150
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        font.pixelSize: 14
                        color: "#ffffff"
                        visible: model3dCheckbox.checked
                    }
                }

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: AppStyle.colors.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
        }
        // 导出模式选项
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 100
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: qsTranslate("MainWindow", "导出模式")
                font.pixelSize: 14
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignLeft
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                // 追加模式
                RowLayout {
                    spacing: 8
                    RadioButton {
                        id: appendModeRadio
                        text: qsTranslate("MainWindow", "追加")
                        checked: exportSettingsCard.exportSettingsController.exportMode === 0
                        onCheckedChanged: {
                            if (checked) {
                                exportSettingsCard.exportSettingsController.setExportMode(0);
                            }
                        }
                        font.pixelSize: 14
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: appendModeRadio.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 10
                            color: "transparent"
                            border.color: appendModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                            border.width: 1.5
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 5
                                color: AppStyle.colors.primary
                                visible: appendModeRadio.checked
                            }
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: AppStyle.colors.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: parent.indicator.width + parent.spacing
                        }
                    }
                    Text {
                        text: qsTranslate("MainWindow", "保留已存在的元器件")
                        font.pixelSize: 11
                        color: AppStyle.colors.textSecondary
                    }
                }
                // 更新模式
                RowLayout {
                    spacing: 8
                    RadioButton {
                        id: updateModeRadio
                        text: qsTranslate("MainWindow", "更新")
                        checked: exportSettingsCard.exportSettingsController.exportMode === 1
                        onCheckedChanged: {
                            if (checked) {
                                exportSettingsCard.exportSettingsController.setExportMode(1);
                            }
                        }
                        font.pixelSize: 14
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: updateModeRadio.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 10
                            color: "transparent"
                            border.color: updateModeRadio.checked ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                            border.width: 1.5
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 5
                                color: AppStyle.colors.primary
                                visible: updateModeRadio.checked
                            }
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: AppStyle.colors.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: parent.indicator.width + parent.spacing
                        }
                    }
                    Text {
                        text: qsTranslate("MainWindow", "覆盖已存在的元器件")
                        font.pixelSize: 11
                        color: AppStyle.colors.textSecondary
                    }
                }
            }
        }
    }
}
