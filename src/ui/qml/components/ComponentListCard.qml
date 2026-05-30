import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQml.Models
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: componentListCard
    // 外部依赖
    property var componentListController
    property var exportProgressController
    readonly property bool showAttentionHint: showValidationHint || showPreviewHint
    readonly property bool showValidationHint: componentListController ? componentListController.validationReadyHint : false
    readonly property bool showPreviewHint: componentListController ? componentListController.previewReadyHint : false
    readonly property color hintColor: showPreviewHint ? "#31c36b" : (showValidationHint ? "#2f7ef8" : "transparent")
    property string editingDescriptionComponentId: ""
    // 导出状态查找表（由 exportProgressController 驱动）
    property var exportStatusMap: ({})
    title: qsTranslate("MainWindow", "元器件列表")
    // 默认折叠，只有在有元器件时才展开
    isCollapsed: componentListController ? componentListController.componentCount === 0 : true
    resources: [
        // 防抖定时器，避免频繁调用 updateFilter()
        Timer {
            id: filterUpdateDebounceTimer
            interval: 120
            onTriggered: visualModel.updateFilter()
        },
        Timer {
            id: previewPrefetchTimer
            interval: 120
            repeat: false
            onTriggered: {
                if (componentListCard.componentListController) {
                    componentListCard.componentListController.fetchPreviewImages(componentListCard.componentListController.getAllComponentIds());
                }
            }
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
        Connections {
            target: componentListCard.componentListController
            function onPreviewFetchRequested() {
                previewPrefetchTimer.restart();
            }
        },
        // 监听导出结果变化，更新导出状态查找表
        Connections {
            target: componentListCard.exportProgressController
            function onResultsListChanged() {
                componentListCard.updateExportStatusMap();
            }
            function onIsExportingChanged() {
                componentListCard.updateExportStatusMap();
            }
        },
        Connections {
            target: componentListCard.componentListController
            function onListCleared() {
                searchInput.text = ""; // 清空搜索
                if (componentListCard.exportProgressController) {
                    componentListCard.exportProgressController.resetExport(); // 重置导出状态
                }
            }
        },
        Connections {
            target: componentListCard.exportProgressController
            function onIsExportingChanged() {
                if (componentListCard.exportProgressController && componentListCard.exportProgressController.isExporting && componentListCard.componentListController) {
                    componentListCard.componentListController.dismissAttentionHints();
                }
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
                // 绑定导出状态（由 ComponentListCard 维护的查找表）
                exportStatus: {
                    var id = itemData ? itemData.componentId : "";
                    return id && componentListCard.exportStatusMap[id] ? componentListCard.exportStatusMap[id] : null;
                }
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
                onDescriptionEditRequested: function (componentId, description) {
                    componentListCard.editingDescriptionComponentId = componentId;
                    descriptionDialog.descriptionText = description;
                    descriptionDialog.open();
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
                        // 验证中：仅表示 CAD 验证尚未完成
                        passFilter = (validationPhase === "validating");
                    } else if (filterMode === "valid") {
                        // 有效：验证已完成或正在获取预览图的项目都属于"有效"
                        passFilter = (validationPhase === "completed" || validationPhase === "fetching_preview");
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
        },
        DescriptionEditDialog {
            id: descriptionDialog
            parent: componentListCard.Window.window ? componentListCard.Window.window.contentItem : componentListCard
            onAccepted: function (description) {
                if (componentListCard.componentListController && componentListCard.editingDescriptionComponentId !== "") {
                    componentListCard.componentListController.updateComponentDescription(componentListCard.editingDescriptionComponentId, description);
                }
                componentListCard.editingDescriptionComponentId = "";
            }
            onRejected: componentListCard.editingDescriptionComponentId = ""
        }
    ]
    overlayContent: [
        Rectangle {
            anchors.fill: parent
            anchors.margins: -12
            radius: componentListCard.radius + 12
            color: "transparent"
            border.width: componentListCard.showAttentionHint ? 4 : 0
            border.color: Qt.alpha(componentListCard.hintColor, 0.42)
            opacity: componentListCard.showAttentionHint ? 0.75 : 0.0
            visible: opacity > 0
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: componentListCard.showAttentionHint
                NumberAnimation {
                    from: 0.24
                    to: 0.78
                    duration: 900
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.78
                    to: 0.28
                    duration: 900
                    easing.type: Easing.InOutQuad
                }
            }

            Behavior on border.color {
                ColorAnimation {
                    duration: 180
                }
            }
        },
        Rectangle {
            anchors.fill: parent
            anchors.margins: -22
            radius: componentListCard.radius + 22
            color: "transparent"
            border.width: componentListCard.showAttentionHint ? 10 : 0
            border.color: Qt.alpha(componentListCard.hintColor, 0.14)
            opacity: componentListCard.showAttentionHint ? 0.50 : 0.0
            visible: opacity > 0
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: componentListCard.showAttentionHint
                NumberAnimation {
                    from: 0.05
                    to: 0.34
                    duration: 1250
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    from: 0.34
                    to: 0.03
                    duration: 1250
                    easing.type: Easing.InCubic
                }
            }

            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: componentListCard.showAttentionHint
                NumberAnimation {
                    from: 0.992
                    to: 1.018
                    duration: 1250
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    from: 1.018
                    to: 0.996
                    duration: 1250
                    easing.type: Easing.InCubic
                }
            }
        },
        // === 全局单例预览弹窗 ===
        Popup {
            id: _previewPopup
            parent: componentListCard.Window.window ? componentListCard.Window.window.contentItem : componentListCard
            property var imageSources: []
            property var currentItemData: null
            property int mainIndex: 0
            property real popupScale: 1
            property real scaleOriginX: 0
            property real scaleOriginY: 0
            readonly property bool hasImages: imageSources.length > 0
            readonly property int panelPadding: AppStyle.spacing.md
            readonly property int headerHeight: 34
            readonly property int mainImageSize: Math.min(Math.max(180, parent ? parent.width * 0.22 : 220), 260)
            readonly property int thumbSize: 42
            readonly property int thumbGap: AppStyle.spacing.xs
            readonly property int stripHeight: imageSources.length > 1 ? thumbSize : 0
            readonly property int galleryWidth: Math.max(mainImageSize, imageSources.length * thumbSize + Math.max(0, imageSources.length - 1) * thumbGap) + panelPadding * 2
            readonly property int galleryHeight: hasImages ? headerHeight + mainImageSize + stripHeight + panelPadding * 2 + (stripHeight > 0 ? AppStyle.spacing.sm : 0) : 184
            width: galleryWidth
            height: galleryHeight
            padding: 0
            visible: false
            closePolicy: Popup.NoAutoClose
            modal: false
            focus: false
            dim: false
            opacity: 1
            Timer {
                id: popupHideTimer
                interval: AppStyle.interactions.popupHideDelay
                repeat: false
                onTriggered: {
                    if (popupHoverHandler.hovered)
                        return;
                    _previewPopup.close();
                }
            }

            onVisibleChanged: {
                if (visible) {
                    mainIndex = 0;
                }
            }

            enter: Transition {
                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeOut
                    }
                    NumberAnimation {
                        property: "popupScale"
                        from: 0.38
                        to: 1
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                }
            }

            exit: Transition {
                ParallelAnimation {
                    NumberAnimation {
                        property: "opacity"
                        from: 1
                        to: 0
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeIn
                    }
                    NumberAnimation {
                        property: "popupScale"
                        from: 1
                        to: 0.92
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeIn
                    }
                }
            }

            background: Rectangle {
                color: "transparent"
            }

            contentItem: Item {
                implicitWidth: _previewPopup.width
                implicitHeight: _previewPopup.height
                visible: _previewPopup.currentItemData && _previewPopup.currentItemData.isValid
                transform: Scale {
                    origin.x: _previewPopup.scaleOriginX
                    origin.y: _previewPopup.scaleOriginY
                    xScale: _previewPopup.popupScale
                    yScale: _previewPopup.popupScale
                }

                Rectangle {
                    anchors.fill: parent
                    color: AppStyle.isDarkMode ? Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.9) : Qt.rgba(1, 1, 1, 0.92)
                    border.width: AppStyle.borderWidths.thin
                    border.color: AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08)
                    radius: AppStyle.radius.lg
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowBlur: 0.82
                        shadowColor: AppStyle.isDarkMode ? "#cc000000" : "#2a0f172a"
                        shadowVerticalOffset: 10
                        shadowHorizontalOffset: 0
                    }
                }

                HoverHandler {
                    id: popupHoverHandler
                    onHoveredChanged: {
                        if (hovered) {
                            popupHideTimer.stop();
                        } else {
                            popupHideTimer.restart();
                        }
                    }
                }

                Column {
                    id: galleryColumn
                    anchors.fill: parent
                    anchors.margins: _previewPopup.panelPadding
                    spacing: _previewPopup.stripHeight > 0 ? AppStyle.spacing.sm : 0
                    visible: _previewPopup.hasImages
                    Rectangle {
                        width: parent.width
                        height: _previewPopup.headerHeight
                        color: "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.right: imageCounter.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: AppStyle.spacing.md
                            text: _previewPopup.currentItemData ? _previewPopup.currentItemData.componentId : ""
                            color: AppStyle.colors.textPrimary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.family: "Courier New"
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            id: imageCounter
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: _previewPopup.imageSources.length > 1 ? (_previewPopup.mainIndex + 1) + "/" + _previewPopup.imageSources.length : ""
                            color: AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.xs
                        }
                    }

                    Rectangle {
                        id: mainImageSurface
                        width: parent.width
                        height: _previewPopup.mainImageSize
                        color: AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.025)
                        radius: AppStyle.radius.md
                        clip: true
                        border.width: AppStyle.borderWidths.thin
                        border.color: AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.06)
                        Canvas {
                            anchors.fill: parent
                            opacity: AppStyle.isDarkMode ? 0.28 : 0.42
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                ctx.strokeStyle = AppStyle.isDarkMode ? "rgba(148, 163, 184, 0.24)" : "rgba(100, 116, 139, 0.24)";
                                ctx.lineWidth = 1;
                                var step = 16;
                                for (var xPos = 0; xPos <= width; xPos += step) {
                                    ctx.beginPath();
                                    ctx.moveTo(xPos + 0.5, 0);
                                    ctx.lineTo(xPos + 0.5, height);
                                    ctx.stroke();
                                }
                                for (var yPos = 0; yPos <= height; yPos += step) {
                                    ctx.beginPath();
                                    ctx.moveTo(0, yPos + 0.5);
                                    ctx.lineTo(width, yPos + 0.5);
                                    ctx.stroke();
                                }
                            }
                        }

                        Image {
                            anchors.fill: parent
                            anchors.margins: AppStyle.spacing.sm
                            sourceSize: Qt.size(_previewPopup.mainImageSize * 1.5, _previewPopup.mainImageSize * 1.5)
                            source: _previewPopup.imageSources.length > 0 ? _previewPopup.imageSources[Math.min(_previewPopup.mainIndex, _previewPopup.imageSources.length - 1)] : ""
                            fillMode: Image.PreserveAspectFit
                            cache: true
                            asynchronous: true
                            smooth: true
                            mipmap: true
                        }
                    }

                    Row {
                        id: thumbnailStrip
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: _previewPopup.stripHeight
                        spacing: _previewPopup.thumbGap
                        visible: _previewPopup.imageSources.length > 1
                        Repeater {
                            model: _previewPopup.imageSources.length
                            Rectangle {
                                width: _previewPopup.thumbSize
                                height: _previewPopup.thumbSize
                                color: index === _previewPopup.mainIndex ? (AppStyle.isDarkMode ? Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.18) : Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.1)) : "transparent"
                                radius: AppStyle.radius.xs
                                border.width: AppStyle.borderWidths.thin
                                border.color: index === _previewPopup.mainIndex ? AppStyle.colors.primary : (AppStyle.isDarkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08))
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    sourceSize: Qt.size(_previewPopup.thumbSize, _previewPopup.thumbSize)
                                    source: _previewPopup.imageSources[index]
                                    fillMode: Image.PreserveAspectFit
                                    cache: true
                                    asynchronous: true
                                    smooth: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onEntered: _previewPopup.mainIndex = index
                                    onClicked: _previewPopup.mainIndex = index
                                }
                            }
                        }
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: !_previewPopup.currentItemData || _previewPopup.imageSources.length === 0
                    Item {
                        id: emptyChipShape
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -AppStyle.spacing.lg
                        width: 74
                        height: 54
                        Rectangle {
                            anchors.centerIn: parent
                            width: 38
                            height: 30
                            radius: AppStyle.radius.xs
                            color: "transparent"
                            border.width: AppStyle.borderWidths.thin
                            border.color: AppStyle.colors.textDisabled
                        }
                        Repeater {
                            model: 4
                            Rectangle {
                                x: 8
                                y: 15 + index * 7
                                width: 10
                                height: 1
                                color: AppStyle.colors.textDisabled
                            }
                        }
                        Repeater {
                            model: 4
                            Rectangle {
                                x: 56
                                y: 15 + index * 7
                                width: 10
                                height: 1
                                color: AppStyle.colors.textDisabled
                            }
                        }
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: emptyChipShape.bottom
                        anchors.topMargin: AppStyle.spacing.sm
                        text: "无预览图"
                        font.pixelSize: AppStyle.fontSizes.sm
                        color: AppStyle.colors.textSecondary
                    }
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
                    font.pixelSize: AppStyle.fontSizes.sm
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
                                font.pixelSize: AppStyle.fontSizes.xs
                            }
                            onClicked: componentListCard.componentListController.setFilterMode("all")
                        }

                        // 验证中
                        Button {
                            width: (filterSegmentedControl.width - 8) / 4
                            height: filterSegmentedControl.height - 8
                            flat: true
                            enabled: componentListCard.componentListController ? componentListCard.componentListController.validatingCount > 0 : false
                            opacity: enabled ? 1.0 : 0.4
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: qsTr("验证中 (%1)").arg(componentListCard.componentListController ? componentListCard.componentListController.validatingCount : 0)
                                color: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "validating" ? "#ffffff" : AppStyle.colors.textSecondary
                                font.bold: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "validating"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: AppStyle.fontSizes.xs
                            }
                            onClicked: {
                                if (componentListCard.componentListController && componentListCard.componentListController.validatingCount > 0) {
                                    componentListCard.componentListController.setFilterMode("validating");
                                }
                            }
                        }

                        // 有效
                        Button {
                            width: (filterSegmentedControl.width - 8) / 4
                            height: filterSegmentedControl.height - 8
                            flat: true
                            enabled: componentListCard.componentListController ? componentListCard.componentListController.validCount > 0 : false
                            opacity: enabled ? 1.0 : 0.4
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: qsTr("有效 (%1)").arg(componentListCard.componentListController ? componentListCard.componentListController.validCount : 0)
                                color: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "valid" ? "#ffffff" : AppStyle.colors.textSecondary
                                font.bold: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "valid"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: AppStyle.fontSizes.xs
                            }
                            onClicked: {
                                if (componentListCard.componentListController && componentListCard.componentListController.validCount > 0) {
                                    componentListCard.componentListController.setFilterMode("valid");
                                }
                            }
                        }

                        // 无效
                        Button {
                            width: (filterSegmentedControl.width - 8) / 4
                            height: filterSegmentedControl.height - 8
                            flat: true
                            enabled: componentListCard.componentListController ? componentListCard.componentListController.invalidCount > 0 : false
                            opacity: enabled ? 1.0 : 0.4
                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Text {
                                text: qsTr("无效 (%1)").arg(componentListCard.componentListController ? componentListCard.componentListController.invalidCount : 0)
                                color: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "invalid" ? "#ffffff" : AppStyle.colors.textSecondary
                                font.bold: (componentListCard.componentListController ? componentListCard.componentListController.filterMode : "all") === "invalid"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: AppStyle.fontSizes.xs
                            }
                            onClicked: {
                                if (componentListCard.componentListController && componentListCard.componentListController.invalidCount > 0) {
                                    componentListCard.componentListController.setFilterMode("invalid");
                                }
                            }
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
                        font.pixelSize: AppStyle.fontSizes.xs
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
                        opacity: AppStyle.opacities.medium
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
                        color: AppStyle.colors.textOnPrimary
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
                        color: AppStyle.colors.textOnPrimary
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
    // === 全局单例预览弹窗 ===
    property alias previewPopup: _previewPopup
    function clampPopupCoordinate(value, size, boundary) {
        var margin = AppStyle.spacing.xs;
        var maxValue = Math.max(margin, boundary - size - margin);
        return Math.max(margin, Math.min(value, maxValue));
    }

    function cursorOverPopup(x, y, width, height, cursorX, cursorY, safetyGap) {
        if (cursorX === undefined || cursorY === undefined || isNaN(cursorX) || isNaN(cursorY))
            return false;
        return cursorX >= x - safetyGap && cursorX <= x + width + safetyGap && cursorY >= y - safetyGap && cursorY <= y + height + safetyGap;
    }

    function avoidCursorOverlap(x, y, width, height, boundaryWidth, boundaryHeight, cursorX, cursorY) {
        var safetyGap = AppStyle.interactions.safeCursorGap;
        if (!cursorOverPopup(x, y, width, height, cursorX, cursorY, safetyGap))
            return Qt.point(x, y);
        var candidates = [Qt.point(cursorX + safetyGap, y), Qt.point(cursorX - width - safetyGap, y), Qt.point(x, cursorY + safetyGap), Qt.point(x, cursorY - height - safetyGap)];
        var bestPoint = Qt.point(x, y);
        var bestDistance = Number.MAX_VALUE;
        for (var i = 0; i < candidates.length; ++i) {
            var candidateX = clampPopupCoordinate(candidates[i].x, width, boundaryWidth);
            var candidateY = clampPopupCoordinate(candidates[i].y, height, boundaryHeight);
            if (cursorOverPopup(candidateX, candidateY, width, height, cursorX, cursorY, safetyGap))
                continue;
            var distance = Math.abs(candidateX - x) + Math.abs(candidateY - y);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestPoint = Qt.point(candidateX, candidateY);
            }
        }
        return bestPoint;
    }

    // 更新导出状态查找表
    function updateExportStatusMap() {
        var map = {};
        var pc = componentListCard.exportProgressController;
        if (pc && pc.resultsList) {
            var list = pc.resultsList;
            for (var i = 0; i < list.length; ++i) {
                var item = list[i];
                if (item && item.componentId) {
                    map[item.componentId] = item;
                }
            }
        }
        exportStatusMap = map;
    }

    // 弹窗显示/隐藏辅助函数（供 delegate 调用）
    function showPreviewPopup(imageSources, itemData, thumbX, thumbY, thumbW, thumbH, cursorX, cursorY) {
        popupHideTimer.stop();
        _previewPopup.imageSources = imageSources || [];
        _previewPopup.currentItemData = itemData;
        // 计算位置
        var popupParent = _previewPopup.parent || componentListCard;
        var winW = popupParent.width;
        var winH = popupParent.height;
        var gap = AppStyle.spacing.lg + 8; // 额外偏移避开鼠标
        var popupW = _previewPopup.width;
        var popupH = _previewPopup.height;
        var spaceRight = winW - (thumbX + thumbW);
        var spaceLeft = thumbX;
        var targetX;
        if (spaceRight >= popupW + gap) {
            targetX = thumbX + thumbW + gap;
        } else if (spaceLeft >= popupW + gap) {
            targetX = thumbX - popupW - gap;
        } else {
            targetX = (winW - popupW) / 2;
        }
        targetX = clampPopupCoordinate(targetX, popupW, winW);
        var targetY = thumbY + (thumbH - popupH) / 2;
        targetY = clampPopupCoordinate(targetY, popupH, winH);
        var adjustedPos = avoidCursorOverlap(targetX, targetY, popupW, popupH, winW, winH, cursorX, cursorY);
        _previewPopup.x = adjustedPos.x;
        _previewPopup.y = adjustedPos.y;
        var thumbCenterX = thumbX + thumbW / 2;
        var thumbCenterY = thumbY + thumbH / 2;
        _previewPopup.scaleOriginX = Math.max(0, Math.min(popupW, thumbCenterX - _previewPopup.x));
        _previewPopup.scaleOriginY = Math.max(0, Math.min(popupH, thumbCenterY - _previewPopup.y));
        _previewPopup.open();
    }

    function hidePreviewPopup() {
        popupHideTimer.restart();
    }

    ColumnLayout {
        width: parent.width
        // 元件列表视图（自适应网格）
        GridView {
            id: componentList
            Layout.fillWidth: true
            Layout.preferredHeight: ResponsiveHelper.responsive(240, 300, 360, 400)
            Layout.topMargin: AppStyle.spacing.md
            clip: true
            // 启用虚拟化，缓存上下各一屏的项
            cacheBuffer: 500
            // 启用 Item 回收，减少创建/销毁开销
            reuseItems: true
            cellWidth: {
                var w = width - AppStyle.spacing.md;
                var minCellW = ResponsiveHelper.responsive(180, 220, 230, 250);
                var c = Math.max(1, Math.floor(w / minCellW));
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

            function requestVisiblePreviewImages() {
                if (!componentListCard.componentListController) {
                    return;
                }

                var ids = [];
                var top = componentList.contentY;
                var bottom = top + componentList.height;
                var prefetchMargin = componentList.cellHeight;
                for (var i = 0; i < componentList.count; ++i) {
                    var delegate = componentList.itemAtIndex(i);
                    if (!delegate || !delegate.itemData || !delegate.itemData.componentId) {
                        continue;
                    }

                    var itemTop = delegate.y;
                    var itemBottom = delegate.y + delegate.height;
                    if (itemBottom < top - prefetchMargin || itemTop > bottom + prefetchMargin) {
                        continue;
                    }

                    if (!delegate.itemData.previewImageCount || delegate.itemData.previewImageCount === 0) {
                        ids.push(delegate.itemData.componentId);
                    }
                }

                if (ids.length > 0) {
                    componentListCard.componentListController.fetchPreviewImages(ids);
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
}
