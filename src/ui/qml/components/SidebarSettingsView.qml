import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Item {
    id: root
    property var exportSettingsController
    signal openOutputFolderDialog
    signal openCacheFolderDialog
    implicitHeight: mainColumn.implicitHeight
    ColumnLayout {
        id: mainColumn
        width: parent.width
        spacing: AppStyle.spacing.lg
        // ==================== 导出路径与库名 ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "输出配置")
            SidebarTextField {
                label: qsTranslate("MainWindow", "输出路径")
                text: root.exportSettingsController ? root.exportSettingsController.outputPath : ""
                placeholder: qsTranslate("MainWindow", "选择目录...")
                onTextEdited: if (root.exportSettingsController)
                    root.exportSettingsController.setOutputPath(text)
                browseIcon: "folder"
                onBrowseClicked: root.openOutputFolderDialog()
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "库名称")
                text: root.exportSettingsController ? root.exportSettingsController.libName : ""
                placeholder: "EasyEDA_Lib"
                onTextEdited: if (root.exportSettingsController)
                    root.exportSettingsController.setLibName(text)
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "缓存目录")
                text: root.exportSettingsController ? root.exportSettingsController.cacheDir : ""
                placeholder: qsTranslate("MainWindow", "选择目录...")
                onTextEdited: if (root.exportSettingsController)
                    root.exportSettingsController.setCacheDir(text)
                browseIcon: "folder"
                onBrowseClicked: root.openCacheFolderDialog()
            }
        }

        // ==================== 导出选项 (带横向收缩动画) ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "导出内容")
            SidebarToggleRow {
                label: qsTranslate("MainWindow", "符号库")
                checked: root.exportSettingsController ? root.exportSettingsController.exportSymbol : false
                onToggled: if (root.exportSettingsController)
                    root.exportSettingsController.setExportSymbol(checked)
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "封装库")
                checked: root.exportSettingsController ? root.exportSettingsController.exportFootprint : false
                onToggled: if (root.exportSettingsController)
                    root.exportSettingsController.setExportFootprint(checked)
            }

            // 3D 模型 - 带有横向收缩的子选项
            Column {
                width: parent.width
                SidebarToggleRow {
                    id: model3dToggle
                    label: qsTranslate("MainWindow", "3D 模型")
                    checked: root.exportSettingsController ? root.exportSettingsController.exportModel3D : false
                    onToggled: if (root.exportSettingsController)
                        root.exportSettingsController.setExportModel3D(checked)
                }

                // 子选项：横向收缩显示
                Item {
                    width: parent.width
                    height: model3dToggle.checked ? subOptionsColumn.implicitHeight + AppStyle.spacing.sm : 0
                    clip: true
                    Behavior on height {
                        NumberAnimation {
                            duration: AppStyle.durations.fast
                            easing.type: Easing.OutCubic
                        }
                    }

                    ColumnLayout {
                        id: subOptionsColumn
                        width: parent.width
                        anchors.top: parent.top
                        anchors.topMargin: AppStyle.spacing.xs
                        opacity: model3dToggle.checked ? 1 : 0
                        x: model3dToggle.checked ? 0 : 20 // 横向偏移动画
                        Behavior on opacity {
                            NumberAnimation {
                                duration: AppStyle.durations.fast
                            }
                        }
                        Behavior on x {
                            NumberAnimation {
                                duration: AppStyle.durations.fast
                                easing.type: Easing.OutBack
                            }
                        }

                        // 格式选择
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: AppStyle.spacing.xl
                            spacing: AppStyle.spacing.sm
                            Text {
                                text: qsTranslate("MainWindow", "格式:")
                                color: AppStyle.colors.textSecondary
                                font.pixelSize: AppStyle.fontSizes.xs
                            }

                            SidebarSegmentedControl {
                                Layout.fillWidth: true
                                model: ["WRL", "STEP", "Both"]
                                currentIndex: {
                                    if (!root.exportSettingsController)
                                        return 0;
                                    var fmt = root.exportSettingsController.exportModel3DFormat;
                                    if (fmt === 3)
                                        return 2;
                                    if (fmt === 2)
                                        return 1;
                                    return 0;
                                }
                                onIndexChanged: {
                                    if (!root.exportSettingsController)
                                        return;
                                    var val = index === 2 ? 3 : (index === 1 ? 2 : 1);
                                    root.exportSettingsController.setExportModel3DFormat(val);
                                }
                            }
                        }

                        // 路径模式
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: AppStyle.spacing.xl
                            spacing: AppStyle.spacing.sm
                            Text {
                                text: qsTranslate("MainWindow", "路径:")
                                color: AppStyle.colors.textSecondary
                                font.pixelSize: AppStyle.fontSizes.xs
                            }

                            SidebarSegmentedControl {
                                Layout.fillWidth: true
                                model: [qsTranslate("MainWindow", "相对"), qsTranslate("MainWindow", "绝对")]
                                currentIndex: root.exportSettingsController ? root.exportSettingsController.exportModel3DPathMode : 0
                                onIndexChanged: if (root.exportSettingsController)
                                    root.exportSettingsController.setExportModel3DPathMode(index)
                            }
                        }
                    }
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "预览图")
                checked: root.exportSettingsController ? root.exportSettingsController.exportPreviewImages : false
                onToggled: if (root.exportSettingsController)
                    root.exportSettingsController.setExportPreviewImages(checked)
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "数据手册")
                checked: root.exportSettingsController ? root.exportSettingsController.exportDatasheet : false
                onToggled: if (root.exportSettingsController)
                    root.exportSettingsController.setExportDatasheet(checked)
            }
        }

        // ==================== 运行策略 ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "运行策略")
            SidebarSegmentedControl {
                label: qsTranslate("MainWindow", "导出模式")
                model: [qsTranslate("MainWindow", "追加"), qsTranslate("MainWindow", "覆盖")]
                currentIndex: root.exportSettingsController ? root.exportSettingsController.exportMode : 0
                onIndexChanged: if (root.exportSettingsController)
                    root.exportSettingsController.setExportMode(index)
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "弱网模式")
                checked: root.exportSettingsController ? root.exportSettingsController.weakNetworkSupport : false
                onToggled: if (root.exportSettingsController)
                    root.exportSettingsController.setWeakNetworkSupport(checked)
            }
        }

        // ==================== 库信息 ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "库信息 (可选)")
            SidebarTextField {
                label: qsTranslate("MainWindow", "符号库描述")
                text: root.exportSettingsController ? root.exportSettingsController.symbolLibraryDescription : ""
                onTextEdited: if (root.exportSettingsController)
                    root.exportSettingsController.setSymbolLibraryDescription(text)
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "封装库描述")
                text: root.exportSettingsController ? root.exportSettingsController.footprintLibraryDescription : ""
                onTextEdited: if (root.exportSettingsController)
                    root.exportSettingsController.setFootprintLibraryDescription(text)
            }
        }
    }
}
