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
                onTextEdited: (txt) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setOutputPath(txt);
                }
                browseIcon: "folder"
                onBrowseClicked: root.openOutputFolderDialog()
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "库名称")
                text: root.exportSettingsController ? root.exportSettingsController.libName : ""
                placeholder: "EasyEDA_Lib"
                onTextEdited: (txt) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setLibName(txt);
                }
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "缓存目录")
                text: root.exportSettingsController ? root.exportSettingsController.cacheDir : ""
                placeholder: qsTranslate("MainWindow", "选择目录...")
                onTextEdited: (txt) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setCacheDir(txt);
                }
                browseIcon: "folder"
                onBrowseClicked: root.openCacheFolderDialog()
            }
        }

        // ==================== 导出选项 ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "导出内容")
            SidebarToggleRow {
                label: qsTranslate("MainWindow", "符号库")
                checked: root.exportSettingsController ? root.exportSettingsController.exportSymbol : false
                onToggled: (val) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportSymbol(val);
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "封装库")
                checked: root.exportSettingsController ? root.exportSettingsController.exportFootprint : false
                onToggled: (val) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportFootprint(val);
                }
            }

            // 3D 模型 - 带有 L 型连接线和背景浸润的子选项
            Item {
                width: parent.width
                height: model3dColumn.implicitHeight
                Column {
                    id: model3dColumn
                    width: parent.width
                    SidebarToggleRow {
                        id: model3dToggle
                        label: qsTranslate("MainWindow", "3D 模型")
                        checked: root.exportSettingsController ? root.exportSettingsController.exportModel3D : false
                        onToggled: (val) => {
                            if (root.exportSettingsController)
                                root.exportSettingsController.setExportModel3D(val);
                        }
                    }

                    // 子选项区域
                    Item {
                        width: parent.width
                        height: model3dToggle.checked ? subOptionsContainer.implicitHeight + AppStyle.spacing.sm : 0
                        clip: true
                        enabled: model3dToggle.checked
                        Behavior on height {
                            NumberAnimation {
                                duration: AppStyle.durations.fast
                                easing.type: Easing.OutQuart
                            }
                        }

                        // L 型连接线
                        Item {
                            id: connectorLine
                            x: AppStyle.spacing.lg + 2
                            width: 16
                            height: parent.height
                            // 垂直段
                            Rectangle {
                                x: 0
                                y: 0
                                width: 2
                                height: parent.height
                                color: AppStyle.colors.border
                                opacity: 0.5
                            }
                            // 水平段（在子项中点位置）
                            Rectangle {
                                x: 0
                                y: Math.min(parent.height * 0.35, 28)
                                width: 12
                                height: 2
                                color: AppStyle.colors.border
                                opacity: 0.5
                            }
                        }

                        // 子选项内容（带背景浸润）
                        Item {
                            id: subOptionsContainer
                            anchors.left: connectorLine.right
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: AppStyle.spacing.xs
                            implicitHeight: subOptionsColumn.implicitHeight + AppStyle.spacing.sm
                            // 背景浸润（不参与布局）
                            Rectangle {
                                anchors.fill: parent
                                anchors.rightMargin: AppStyle.spacing.xs
                                radius: AppStyle.radius.sm
                                color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.04) : Qt.rgba(0, 0, 0, 0.04)
                                border.width: AppStyle.isDarkMode ? 1 : 0
                                border.color: Qt.rgba(255, 255, 255, 0.06)
                            }

                            ColumnLayout {
                                id: subOptionsColumn
                                width: parent.width
                                anchors.top: parent.top
                                anchors.topMargin: AppStyle.spacing.xs
                                spacing: AppStyle.spacing.sm
                                // 格式选择（标签在上，控件占满行）
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: AppStyle.spacing.sm
                                    Layout.rightMargin: AppStyle.spacing.sm
                                    spacing: 2
                                    opacity: model3dToggle.checked ? 1 : 0
                                    x: model3dToggle.checked ? 0 : 12
                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: AppStyle.durations.fast
                                        }
                                    }
                                    Behavior on x {
                                        NumberAnimation {
                                            duration: AppStyle.durations.fast
                                            easing.type: Easing.OutQuart
                                        }
                                    }

                                    Text {
                                        text: qsTranslate("MainWindow", "格式")
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
                                        onIndexChanged: (idx) => {
                                            if (!root.exportSettingsController)
                                                return;
                                            var val = idx === 2 ? 3 : (idx === 1 ? 2 : 1);
                                            root.exportSettingsController.setExportModel3DFormat(val);
                                        }
                                    }
                                }

                                // 路径模式（标签在上，控件占满行，交错入场）
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: AppStyle.spacing.sm
                                    Layout.rightMargin: AppStyle.spacing.sm
                                    spacing: 2
                                    opacity: model3dToggle.checked ? 1 : 0
                                    x: model3dToggle.checked ? 0 : 12
                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: AppStyle.durations.fast
                                        }
                                    }
                                    Behavior on x {
                                        NumberAnimation {
                                            duration: AppStyle.durations.fast
                                            easing.type: Easing.OutQuart
                                        }
                                    }

                                    Text {
                                        text: qsTranslate("MainWindow", "路径")
                                        color: AppStyle.colors.textSecondary
                                        font.pixelSize: AppStyle.fontSizes.xs
                                    }

                                    SidebarSegmentedControl {
                                        Layout.fillWidth: true
                                        model: [qsTranslate("MainWindow", "相对"), qsTranslate("MainWindow", "绝对")]
                                        currentIndex: root.exportSettingsController ? root.exportSettingsController.exportModel3DPathMode : 0
                                        onIndexChanged: (idx) => {
                                            if (root.exportSettingsController)
                                                root.exportSettingsController.setExportModel3DPathMode(idx);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "预览图")
                checked: root.exportSettingsController ? root.exportSettingsController.exportPreviewImages : false
                onToggled: (val) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportPreviewImages(val);
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "数据手册")
                checked: root.exportSettingsController ? root.exportSettingsController.exportDatasheet : false
                onToggled: (val) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setExportDatasheet(val);
                }
            }
        }

        // ==================== 运行策略（滑块式导出模式选择） ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "运行策略")
            // 导出模式：滑块式二选一
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTranslate("MainWindow", "导出模式")
                    font.pixelSize: AppStyle.fontSizes.xs
                    color: AppStyle.colors.textSecondary
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: AppStyle.radius.sm
                    color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(0, 0, 0, 0.06)
                    // 滑块指示器
                    Rectangle {
                        id: modeSlider
                        width: parent.width / 2
                        height: parent.height - 4
                        anchors.verticalCenter: parent.verticalCenter
                        radius: AppStyle.radius.sm - 1
                        color: AppStyle.colors.surface
                        border.width: AppStyle.borderWidths.thin
                        border.color: AppStyle.colors.border
                        x: (root.exportSettingsController ? root.exportSettingsController.exportMode : 0) === 0 ? 2 : parent.width / 2
                        Behavior on x {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutQuart
                            }
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.margins: 2
                        Repeater {
                            model: [qsTranslate("MainWindow", "追加"), qsTranslate("MainWindow", "覆盖")]
                            Item {
                                width: parent.width / 2
                                height: parent.height
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: AppStyle.fontSizes.xs
                                    font.bold: index === (root.exportSettingsController ? root.exportSettingsController.exportMode : 0)
                                    color: index === (root.exportSettingsController ? root.exportSettingsController.exportMode : 0) ? AppStyle.colors.primary : AppStyle.colors.textSecondary
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 200
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (root.exportSettingsController)
                                            root.exportSettingsController.setExportMode(index);
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    text: (root.exportSettingsController ? root.exportSettingsController.exportMode : 0) === 0 ? qsTranslate("MainWindow", "保留已有元器件，追加新的") : qsTranslate("MainWindow", "覆盖已有元器件，追加新的")
                    font.pixelSize: AppStyle.fontSizes.xs - 1
                    color: AppStyle.colors.textSecondary
                    opacity: 0.7
                }
            }

            SidebarToggleRow {
                label: qsTranslate("MainWindow", "弱网模式")
                checked: root.exportSettingsController ? root.exportSettingsController.weakNetworkSupport : false
                onToggled: (val) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setWeakNetworkSupport(val);
                }
            }
        }

        // ==================== 库信息 ====================
        SidebarSection {
            title: qsTranslate("MainWindow", "库信息 (可选)")
            SidebarTextField {
                label: qsTranslate("MainWindow", "符号库描述")
                text: root.exportSettingsController ? root.exportSettingsController.symbolLibraryDescription : ""
                onTextEdited: (txt) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setSymbolLibraryDescription(txt);
                }
            }

            SidebarTextField {
                label: qsTranslate("MainWindow", "封装库描述")
                text: root.exportSettingsController ? root.exportSettingsController.footprintLibraryDescription : ""
                onTextEdited: (txt) => {
                    if (root.exportSettingsController)
                        root.exportSettingsController.setFootprintLibraryDescription(txt);
                }
            }
        }
    }
}
