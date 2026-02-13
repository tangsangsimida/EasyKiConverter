import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: componentListCard

    // 外部依赖
    property var componentListController
    property var exportProgressController

    title: qsTranslate("MainWindow", "元器件列表")

    // 默认折叠，只有在有元器件时才展开
    isCollapsed: componentListController.componentCount === 0

    resources: [
        // 监听组件数量变化，自动展开
        Connections {
            target: componentListCard.componentListController
            function onComponentCountChanged() {
                if (componentListCard.componentListController.componentCount > 0) {
                    componentListCard.isCollapsed = false;
                }
            }
        },
        // 搜索过滤模型 (作为资源定义，不参与布局)
        DelegateModel {
            id: visualModel
            model: componentListCard.componentListController

            groups: [
                DelegateModelGroup {
                    id: displayGroup
                    includeByDefault: true
                    name: "display"
                },
                DelegateModelGroup {
                    id: filterGroup
                    name: "filter"
                }
            ]

            filterOnGroup: "display"

            delegate: ComponentListItem {
                width: componentList.cellWidth - AppStyle.spacing.md
                anchors.horizontalCenter: parent ? undefined : undefined

                // 绑定数据和搜索词
                // 注意：QAbstractListModel 暴露的角色名为 "itemData"
                itemData: model.itemData
                searchText: searchInput.text // 传递搜索词用于高亮

                onDeleteClicked: {
                    if (itemData) {
                        componentListCard.componentListController.removeComponentById(itemData.componentId);
                    }
                }
            }

            // 过滤函数
            function updateFilter() {
                // 移除所有空格，实现更宽容的搜索 (例如 "C 2040" -> "c2040")
                var searchTerm = searchInput.text.toLowerCase().replace(/\s+/g, '');

                // 遍历所有项进行处理
                for (var i = 0; i < items.count; i++) {
                    var item = items.get(i);

                    // 如果搜索词为空，显示所有项
                    if (searchTerm === "") {
                        item.inDisplay = true;
                        continue;
                    }

                    // 获取内容
                    // 对于 QAbstractListModel，item.model 包含角色属性
                    var dataObj = item.model.itemData;

                    // 获取 ID
                    var idStr = "";
                    if (dataObj && dataObj.componentId !== undefined) {
                        idStr = dataObj.componentId;
                    }

                    // 判断是否匹配
                    if (idStr.toLowerCase().indexOf(searchTerm) !== -1) {
                        item.inDisplay = true;
                    } else {
                        item.inDisplay = false;
                    }
                }
            }
        }
    ]

    RowLayout {
        width: parent.width
        spacing: 12
        Text {
            id: componentCountLabel
            text: qsTranslate("MainWindow", "共 %1 个元器件").arg(componentListCard.componentListController.componentCount)
            font.pixelSize: 14
            color: AppStyle.colors.textSecondary
        }

        Item {
            Layout.fillWidth: true
        }

        // 搜索框

        TextField {
            id: searchInput

            Layout.preferredWidth: 200

            Layout.preferredHeight: 44

            placeholderText: qsTranslate("MainWindow", "搜索元器件...")

            font.pixelSize: AppStyle.fontSizes.sm

            color: AppStyle.colors.textPrimary

            placeholderTextColor: AppStyle.colors.textSecondary

            leftPadding: 32 // 为图标留出空间

            background: Rectangle {

                color: AppStyle.colors.surface

                border.color: searchInput.focus ? AppStyle.colors.borderFocus : AppStyle.colors.border

                border.width: searchInput.focus ? 2 : 1

                radius: AppStyle.radius.md
            }

            // 搜索图标

            Image {

                anchors.left: parent.left

                anchors.leftMargin: 8

                anchors.verticalCenter: parent.verticalCenter

                width: 16

                height: 16

                source: AppStyle.isDarkMode ? "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark-white.svg" : // 暂时用现有图标替代，或者用文字

                "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/github-mark.svg"

                // 注意：实际上应该用一个 'search' 图标，这里暂时复用或忽略，

                // 为了美观，用 Text 替代

                visible: false
            }

            Text {

                anchors.left: parent.left

                anchors.leftMargin: 10

                anchors.verticalCenter: parent.verticalCenter

                text: ""

                font.pixelSize: 12

                color: AppStyle.colors.textSecondary
            }

            onTextChanged: {
                visualModel.updateFilter();
            }
        }

        // 复制所有编号按钮

        Item {
            id: copyAllButton

            Layout.preferredWidth: copyAllButtonContent.width + AppStyle.spacing.xl * 2

            Layout.preferredHeight: 44

            Layout.alignment: Qt.AlignVCenter

            // 按钮背景
            Rectangle {
                anchors.fill: parent
                color: copyAllButtonMouseArea.pressed ? AppStyle.colors.textPrimary : copyAllButtonMouseArea.containsMouse ? Qt.darker(AppStyle.colors.textSecondary, 1.2) : AppStyle.colors.textSecondary
                radius: AppStyle.radius.md
                enabled: componentListCard.componentListController.componentCount > 0
                opacity: componentListCard.componentListController.componentCount > 0 ? 1.0 : 0.4
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
                Behavior on opacity {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }

            // 按钮文本
            Text {
                id: copyAllButtonContent
                anchors.centerIn: parent
                text: qsTranslate("MainWindow", "复制所有编号")
                font.pixelSize: AppStyle.fontSizes.sm
                color: "white"
            }

            // 鼠标区域
            MouseArea {
                id: copyAllButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: componentListCard.componentListController.componentCount > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (componentListCard.componentListController.componentCount > 0) {
                        componentListCard.componentListController.copyAllComponentIds();
                        copyAllFeedback.visible = true;
                    }
                }
            }

            // 复制成功提示
            ToolTip {
                id: copyAllFeedback
                parent: copyAllButton
                x: (parent.width - width) / 2
                y: -35
                text: qsTranslate("MainWindow", "已复制所有编号")
                delay: 0
                timeout: 1500
            }
        }

        ModernButton {
            text: qsTranslate("MainWindow", "清空列表")
            iconName: "trash"
            font.pixelSize: AppStyle.fontSizes.sm
            backgroundColor: AppStyle.colors.danger
            hoverColor: AppStyle.colors.dangerDark
            pressedColor: AppStyle.colors.dangerDark
            onClicked: {
                searchInput.text = ""; // 清空搜索
                componentListCard.componentListController.clearComponentList();
                componentListCard.exportProgressController.resetExport(); // 重置导出状态
            }
        }
    }
    // 元件列表视图（自适应网格）
    GridView {
        id: componentList
        Layout.fillWidth: true
        Layout.preferredHeight: 300
        Layout.topMargin: AppStyle.spacing.md
        clip: true

        // 动态计算列宽，实现响应式布局
        property int minCellWidth: 230
        property int availableWidth: width - AppStyle.spacing.md // 减去右侧滚动条/边距空间
        property int columns: Math.max(1, Math.floor(availableWidth / minCellWidth))

        cellWidth: Math.floor(availableWidth / columns)
        // 卡片高度 64 + 垂直间距 12 = 76
        cellHeight: 76

        flow: GridView.FlowLeftToRight
        layoutDirection: Qt.LeftToRight

        // 使用 DelegateModel
        model: visualModel

        // delegate 已经在 DelegateModel 中定义了，这里不需要再定义，
        // 但是 GridView 需要直接使用 visualModel 作为 model。
        // 注意：当 model 是 DelegateModel 时，不需要指定 delegate 属性，
        // 因为 DelegateModel 已经包含了 delegate。

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }
        // 添加列表项进入动画 (DelegateModel 管理时可能需要调整)
        // 简单的 add/remove 动画在使用 DelegateModel 时可能不生效或表现不同
    }
}
