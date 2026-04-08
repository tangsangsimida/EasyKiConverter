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
    isCollapsed: componentListController ? componentListController.componentCount === 0 : true
    resources: [
        // 防抖定时器，避免频繁调用 updateFilter()
        Timer {
            id: filterUpdateDebounceTimer
            interval: 50
            onTriggered: visualModel.updateFilter()
        },
        // 监听组件数量变化，自动展开
        Connections {
            target: componentListCard.componentListController
            function onComponentCountChanged() {
                if (componentListCard.componentListController.componentCount > 0) {
                    componentListCard.isCollapsed = false;
                }
            }
        },
        // 监听筛选模式变化，自动更新过滤
        Connections {
            target: componentListCard.componentListController
            function onFilterModeChanged() {
                filterUpdateDebounceTimer.restart();
            }
        },
        // 监听筛选数量变化，自动更新过滤
        Connections {
            target: componentListCard.componentListController
            function onFilteredCountChanged() {
                filterUpdateDebounceTimer.restart();
            }
        },
        // 搜索过滤模型 (作为资源定义，不参与布局)
        DelegateModel {
            id: visualModel
            model: componentListCard.componentListController ?? null
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
                onRetryClicked: {
                    if (itemData) {
                        componentListCard.componentListController.refreshComponentInfo(index);
                    }
                }
            }

            // 过滤函数
            function updateFilter() {
                // 移除所有空格，实现更宽容的搜索 (例如 "C 2040" -> "c2040")
                var searchTerm = searchInput.text.toLowerCase().replace(/\s+/g, '');
                var filterMode = componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all";
                // 遍历所有项进行处理
                for (var i = 0; i < items.count; i++) {
                    var item = items.get(i);
                    // 获取数据对象
                    var dataObj = item.model.itemData;
                    var idStr = dataObj && dataObj.componentId !== undefined ? dataObj.componentId : "";
                    var validationPhase = dataObj && dataObj.validationPhase !== undefined ? dataObj.validationPhase : "idle";
                    // 验证状态筛选（使用 validationPhase）
                    var passFilter = false;
                    if (filterMode === "all") {
                        passFilter = true;
                    } else if (filterMode === "validating") {
                        passFilter = (validationPhase === "validating" || validationPhase === "fetching_preview");
                    } else if (filterMode === "valid") {
                        passFilter = (validationPhase === "completed");
                    } else if (filterMode === "invalid") {
                        passFilter = (validationPhase === "failed");
                    }

                    // 搜索词筛选
                    var passSearch = true;
                    if (searchTerm !== "" && idStr.toLowerCase().indexOf(searchTerm) === -1) {
                        passSearch = false;
                    }

                    // 同时满足筛选和搜索条件才显示
                    item.inDisplay = passFilter && passSearch;
                }
            }
        }
    ]
    Item {
        id: toolbarContainer
        width: parent.width
        height: 50
        MouseArea {
            anchors.fill: parent
            onWheel: function (wheel) {
                var currentX = toolbarFlickable.contentX;
                var maxScroll = Math.max(0, toolbarRowLayout.width - parent.width);
                var canScrollToolbar = maxScroll > 0;
                var atStart = currentX <= 0;
                var atEnd = currentX >= maxScroll;
                var canScrollUp = !atStart;
                var canScrollDown = !atEnd;
                if (wheel.angleDelta.y > 0 && canScrollToolbar && canScrollUp) {
                    var newX = Math.max(0, currentX - 50);
                    if (newX !== currentX) {
                        if (newX === 0) {
                            bounceAnimation.to = newX;
                            bounceAnimation.start();
                        } else {
                            toolbarFlickable.contentX = newX;
                        }
                    }
                } else if (wheel.angleDelta.y < 0 && canScrollToolbar && canScrollDown) {
                    var newX = Math.min(maxScroll, currentX + 50);
                    if (newX !== currentX) {
                        if (newX === maxScroll) {
                            bounceAnimation.to = newX;
                            bounceAnimation.start();
                        } else {
                            toolbarFlickable.contentX = newX;
                        }
                    }
                } else {
                    wheel.accepted = false;
                }
            }
            PropertyAnimation {
                id: bounceAnimation
                target: toolbarFlickable
                property: "contentX"
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
        Flickable {
            id: toolbarFlickable
            width: parent.width
            height: parent.height
            contentWidth: toolbarRowLayout.width
            contentHeight: height
            clip: true
            interactive: true
            boundsBehavior: Flickable.StopAtBounds
            RowLayout {
                id: toolbarRowLayout
                width: implicitWidth
                height: parent.height
                spacing: 12
                // 左边：元器件数量 + 筛选
                Text {
                    id: componentCountLabel
                    text: qsTranslate("MainWindow", "共 %1 个元器件").arg(componentListCard.componentListController ? componentListCard.componentListController.componentCount : 0)
                    font.pixelSize: 14
                    color: AppStyle.colors.textSecondary
                    Layout.alignment: Qt.AlignVCenter
                }

                // 筛选 Segmented Control (左边)
                Rectangle {
                    id: filterSegmentedControl
                    Layout.preferredWidth: 400
                    Layout.preferredHeight: 42
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: 16
                    color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(0, 0, 0, 0.05)
                    radius: AppStyle.radius.lg
                    visible: componentListCard.componentListController ? componentListCard.componentListController.componentCount > 0 : false
                    clip: true
                    // 滑块背景指示器
                    Rectangle {
                        id: sliderIndicator
                        width: (filterSegmentedControl.width - 8) / 4
                        height: filterSegmentedControl.height - 8
                        anchors.verticalCenter: parent.verticalCenter
                        radius: AppStyle.radius.md
                        color: {
                            var mode = componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all";
                            if (mode === "validating")
                                return AppStyle.colors.warning;
                            if (mode === "valid")
                                return AppStyle.colors.success;
                            if (mode === "invalid")
                                return AppStyle.colors.danger;
                            return AppStyle.colors.primary;
                        }

                        x: {
                            var step = (filterSegmentedControl.width - 8) / 4;
                            var mode = componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all";
                            if (mode === "validating")
                                return 4 + step;
                            if (mode === "valid")
                                return 4 + step * 2;
                            if (mode === "invalid")
                                return 4 + step * 3;
                            return 4;
                        }

                        Behavior on x {
                            NumberAnimation {
                                duration: 450
                                easing.type: AppStyle.easings.easeInOut
                            }
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: 450
                                easing.type: AppStyle.easings.easeInOut
                            }
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.margins: 4
                        // 全部
                        Button {
                            width: (filterSegmentedControl.width - 8) / 4
                            height: filterSegmentedControl.height - 8
                            flat: true
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: qsTr("全部 (%1)").arg(componentListCard.componentListController ? componentListCard.componentListController.componentCount : 0)
                                color: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "all" ? "#ffffff" : AppStyle.colors.textSecondary
                                font.bold: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "all"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 12
                            }
                            onClicked: componentListCard.componentListController.setFilterMode("all")
                        }

                        // 验证中
                        Button {
                            width: (filterSegmentedControl.width - 8) / 4
                            height: filterSegmentedControl.height - 8
                            flat: true
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: qsTr("验证中 (%1)").arg(componentListCard.componentListController ? componentListCard.componentListController.validatingCount : 0)
                                color: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "validating" ? "#ffffff" : AppStyle.colors.textSecondary
                                font.bold: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "validating"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 12
                            }
                            onClicked: componentListCard.componentListController.setFilterMode("validating")
                        }

                        // 有效
                        Button {
                            width: (filterSegmentedControl.width - 8) / 4
                            height: filterSegmentedControl.height - 8
                            flat: true
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: qsTr("有效 (%1)").arg(componentListCard.componentListController ? componentListCard.componentListController.validCount : 0)
                                color: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "valid" ? "#ffffff" : AppStyle.colors.textSecondary
                                font.bold: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "valid"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 12
                            }
                            onClicked: componentListCard.componentListController.setFilterMode("valid")
                        }

                        // 无效
                        Button {
                            width: (filterSegmentedControl.width - 8) / 4
                            height: filterSegmentedControl.height - 8
                            flat: true
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: qsTr("无效 (%1)").arg(componentListCard.componentListController ? componentListCard.componentListController.invalidCount : 0)
                                color: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "invalid" ? "#ffffff" : AppStyle.colors.textSecondary
                                font.bold: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "invalid"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 12
                            }
                            onClicked: componentListCard.componentListController.setFilterMode("invalid")
                        }
                    }
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
                        filterUpdateDebounceTimer.restart();
                    }
                }

                // 重试所有验证失败元器件按钮
                Item {
                    id: retryAllButton
                    Layout.preferredWidth: retryAllButtonContent.width + AppStyle.spacing.xl * 2
                    Layout.preferredHeight: 44
                    Layout.alignment: Qt.AlignVCenter
                    visible: componentListCard.componentListController ? componentListCard.componentListController.hasInvalidComponents : false
                    // 按钮背景
                    Rectangle {
                        anchors.fill: parent
                        color: retryAllButtonMouseArea.pressed ? AppStyle.colors.primaryHover : retryAllButtonMouseArea.containsMouse ? Qt.darker(AppStyle.colors.primary, 1.1) : AppStyle.colors.primary
                        radius: AppStyle.radius.md
                        opacity: 0.8
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
                        id: retryAllButtonContent
                        anchors.centerIn: parent
                        text: qsTranslate("MainWindow", "重试所有")
                        font.pixelSize: AppStyle.fontSizes.sm
                        color: "white"
                    }

                    // 鼠标区域
                    MouseArea {
                        id: retryAllButtonMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (componentListCard.componentListController) {
                                componentListCard.componentListController.retryAllInvalidComponents();
                            }
                        }
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
                        enabled: componentListCard.componentListController ? componentListCard.componentListController.componentCount > 0 : false
                        opacity: componentListCard.componentListController ? (componentListCard.componentListController.componentCount > 0 ? 1.0 : 0.4) : 0.4
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
                        cursorShape: (componentListCard.componentListController && componentListCard.componentListController.componentCount > 0) ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (componentListCard.componentListController && componentListCard.componentListController.componentCount > 0) {
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
                        if (componentListCard.componentListController) {
                            componentListCard.componentListController.clearComponentList();
                        }
                        if (componentListCard.exportProgressController) {
                            componentListCard.exportProgressController.resetExport(); // 重置导出状态
                        }
                    }
                }
            }
        }
    }
    ColumnLayout {
        width: parent.width
        // 元件列表视图（自适应网格）
        GridView {
            id: componentList
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            Layout.topMargin: AppStyle.spacing.md
            clip: true
            // 启用虚拟化，缓存上下各一屏的项
            cacheBuffer: 500
            // 启用 Item 回收，减少创建/销毁开销
            reuseItems: true
            cellWidth: {
                var w = width - AppStyle.spacing.md;
                var c = Math.max(1, Math.floor(w / 230));
                return w / c;
            }
            cellHeight: 76
            flow: GridView.FlowLeftToRight
            layoutDirection: Qt.LeftToRight
            // 使用 DelegateModel
            model: visualModel
            // 监听滚动状态
            onMovingChanged: {
                if (componentListCard.componentListController) {
                    componentListCard.componentListController.setScrolling(moving);
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
}
