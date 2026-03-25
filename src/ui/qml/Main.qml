import QtQuick
import QtQuick.Controls
import "components"
import "styles"

ApplicationWindow {
    id: appWindow
    // 默认窗口大小为屏幕的 65%（如果配置中没有保存的值）
    property int defaultWidth: Math.max(800, Screen.desktopAvailableWidth * 0.65)
    property int defaultHeight: Math.max(600, Screen.desktopAvailableHeight * 0.65)
    property int windowRadius: 10
    // 状态锁：防止重复触发关闭/最小化动画
    property bool isClosing: false
    property bool isMinimizing: false
    width: configService ? (configService.getWindowWidth() > 0 ? configService.getWindowWidth() : defaultWidth) : defaultWidth
    height: configService ? (configService.getWindowHeight() > 0 ? configService.getWindowHeight() : defaultHeight) : defaultHeight
    // 默认窗口位置居中显示（如果配置中没有保存的值）
    x: configService ? (configService.getWindowX() > 0 ? configService.getWindowX() : (Screen.desktopAvailableWidth - width) / 2) : (Screen.desktopAvailableWidth - width) / 2
    y: configService ? (configService.getWindowY() > 0 ? configService.getWindowY() : (Screen.desktopAvailableHeight - height) / 2) : (Screen.desktopAvailableHeight - height) / 2
    // 最小窗口宽度计算：
    // - 动态计算基于文本宽度和布局
    // - 由 MainWindow.qml 中的 calculatedMinimumWindowWidth 提供
    property int dynamicMinimumWidth: 855  // 默认值，会被覆盖
    minimumWidth: dynamicMinimumWidth
    minimumHeight: 600
    visible: true
    title: "EasyKiConverter - 元器件转换工具"
    color: "transparent"
    // 使用自定义标题栏，添加任务栏按钮支持
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.CustomizeWindowHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint
    // 监听窗口可见性变化
    onVisibleChanged: {
        if (visible) {
            // 窗口显示时，重置状态锁
            isClosing = false;
            isMinimizing = false;
        }
    }

    // 保存窗口大小和位置的函数
    function saveWindowPosition() {
        if (configService) {
            console.log("准备保存窗口位置 - 当前值:");
            console.log("  width:", width, "height:", height);
            console.log("  x:", x, "y:", y);
            console.log("  visibility:", visibility);
            configService.setWindowWidth(width);
            configService.setWindowHeight(height);
            // 只在窗口不是最大化、全屏或最小化时保存位置
            if (visibility !== Window.Maximized && visibility !== Window.FullScreen && visibility !== Window.Minimized) {
                configService.setWindowX(x);
                configService.setWindowY(y);
                console.log("已保存窗口位置: (" + x + ", " + y + ") 大小: (" + width + ", " + height + ")");
            }

            // 验证保存的值
            console.log("验证保存的值:");
            console.log("  保存的 windowX:", configService.getWindowX());
            console.log("  保存的 windowY:", configService.getWindowY());
            console.log("  保存的 windowWidth:", configService.getWindowWidth());
            console.log("  保存的 windowHeight:", configService.getWindowHeight());
        }
    }

    // 美化的确认关闭对话框
    ConfirmDialog {
        id: closeConfirmDialog
        title: qsTr("确认退出")
        message: qsTr("转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？")
        confirmText: qsTr("强制退出")
        cancelText: qsTr("继续转换")
        confirmColor: AppStyle.colors.danger
        onAccepted: {
            if (exportProgressViewModel.handleCloseRequest()) {
                saveWindowPosition();
                Qt.quit();
            }
        }
    }

    // 退出动画层 - 向下滑动缩小（与最小化效果一致）

    Rectangle {
        id: exitAnimationLayer
        width: appWindow.width
        height: appWindow.height
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        opacity: 0
        z: 1000
        visible: opacity > 0
        color: themeSettingsViewModel && themeSettingsViewModel.isDarkMode ? "#1a1a1a" : "#f5f5f5"
        radius: appWindow.windowRadius
        // 添加尺寸绑定，确保窗口大小改变时动画层同步更新
        Binding on width {
            when: exitAnimationLayer.opacity > 0
            value: appWindow.width
            restoreMode: Binding.RestoreBinding
        }

        Binding on height {
            when: exitAnimationLayer.opacity > 0
            value: appWindow.height
            restoreMode: Binding.RestoreBinding
        }

        transform: Scale {
            id: exitScale
            origin.x: (exitAnimationLayer.width || 800) / 2
            origin.y: exitAnimationLayer.height || 600
            xScale: 1.0
            yScale: 1.0
        }

        // 显式动画

        ParallelAnimation {
            id: exitWindowAnim
            running: false
            NumberAnimation {
                target: exitAnimationLayer
                property: "y"
                to: appWindow.height
                duration: 400
                easing.type: Easing.InOutQuad
            }

            NumberAnimation {
                target: exitScale
                property: "xScale"
                to: 0.1
                duration: 400
                easing.type: Easing.InOutQuad
            }

            NumberAnimation {
                target: exitScale
                property: "yScale"
                to: 0.1
                duration: 400
                easing.type: Easing.InOutQuad
            }
        }

        // 退出动画计时器

        Timer {
            id: exitAnimTimer
            interval: 450
            onTriggered: {
                // 清理动画层

                exitAnimationLayer.opacity = 0;
                exitAnimationLayer.y = 0;
                exitScale.xScale = 1.0;
                exitScale.yScale = 1.0;
                // 执行退出

                saveWindowPosition();
                exportProgressViewModel.handleCloseRequest();
                // 重置状态锁
                isClosing = false;
                Qt.quit();
            }
        }
    }

    // 最小化动画层 - 向下滑动缩小

    Rectangle {
        id: minimizeAnimationLayer
        width: appWindow.width
        height: appWindow.height
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        opacity: 0
        z: 1000
        visible: opacity > 0
        color: themeSettingsViewModel && themeSettingsViewModel.isDarkMode ? "#1a1a1a" : "#f5f5f5"
        radius: appWindow.windowRadius
        // 添加尺寸绑定，确保窗口大小改变时动画层同步更新
        Binding on width {
            when: minimizeAnimationLayer.opacity > 0
            value: appWindow.width
            restoreMode: Binding.RestoreBinding
        }

        Binding on height {
            when: minimizeAnimationLayer.opacity > 0
            value: appWindow.height
            restoreMode: Binding.RestoreBinding
        }

        transform: Scale {
            id: minimizeScale
            origin.x: (minimizeAnimationLayer.width || 800) / 2
            origin.y: minimizeAnimationLayer.height || 600
            xScale: 1.0
            yScale: 1.0
        }

        // 显式动画

        ParallelAnimation {
            id: minimizeWindowAnim
            running: false
            NumberAnimation {
                target: minimizeAnimationLayer
                property: "y"
                to: appWindow.height
                duration: 400
                easing.type: Easing.InOutQuad
            }

            NumberAnimation {
                target: minimizeScale
                property: "xScale"
                to: 0.1
                duration: 400
                easing.type: Easing.InOutQuad
            }

            NumberAnimation {
                target: minimizeScale
                property: "yScale"
                to: 0.1
                duration: 400
                easing.type: Easing.InOutQuad
            }
        }

        // 最小化动画计时器

        Timer {
            id: minimizeAnimTimer
            interval: 450
            onTriggered: {
                // 清理动画层

                minimizeAnimationLayer.opacity = 0;
                minimizeAnimationLayer.y = 0;
                minimizeScale.xScale = 1.0;
                minimizeScale.yScale = 1.0;
                // 执行最小化

                saveWindowPosition();
                appWindow.showMinimized();
                // 重置状态锁
                isMinimizing = false;
            }
        }
    }

    // 退出选项对话框
    ExitDialog {
        id: exitOptionDialog
        onMinimizeToTray: function (remember) {
            // 如果选择了记住，保存退出偏好
            if (remember && configService) {
                configService.setExitPreference("minimize");
            }

            // 立即隐藏对话框，直接最小化
            exitOptionDialog.visible = false;
            playMinimizeAnimation();
        }
        onExitApp: function (remember) {
            // 如果选择了记住，保存退出偏好
            if (remember && configService) {
                configService.setExitPreference("exit");
            }

            // 立即隐藏对话框，直接退出
            exitOptionDialog.visible = false;
            playExitAnimation();
        }
        onCanceled: {
            // 用户点击取消
        }
    }

    // 播放退出动画 - 直接退出
    function playExitAnimation() {
        saveWindowPosition();
        exportProgressViewModel.handleCloseRequest();
        Qt.quit();
    }

    // 播放最小化动画 - 直接最小化
    function playMinimizeAnimation() {
        saveWindowPosition();
        appWindow.showMinimized();
    }

    onClosing: close => {
        // 如果对话框正在显示，阻止关闭
        if (closeConfirmDialog.visible || exitOptionDialog.visible) {
            close.accepted = false;
            return;
        }

        if (exportProgressViewModel.isExporting) {
            close.accepted = false;
            closeConfirmDialog.open();
        } else {
            // 检查是否有记住的退出偏好
            if (configService) {
                var exitPreference = configService.getExitPreference();
                if (exitPreference === "minimize") {
                    // 直接最小化
                    close.accepted = false;
                    playMinimizeAnimation();
                } else if (exitPreference === "exit") {
                    // 直接退出
                    close.accepted = false;
                    playExitAnimation();
                } else {
                    // 没有记住的偏好，显示退出选项对话框
                    close.accepted = false;
                    exitOptionDialog.open();
                }
            } else {
                // configService 不可用，显示退出选项对话框
                close.accepted = false;
                exitOptionDialog.open();
            }
        }
    }

    // ESC 键快捷键处理
    Shortcut {
        sequence: "Esc"
        onActivated: {
            // ESC 键：打开或关闭对话框
            if (closeConfirmDialog.visible) {
                // 如果 ConfirmDialog 可见，关闭它
                closeConfirmDialog.close();
            } else if (exitOptionDialog.visible) {
                // 如果 ExitDialog 可见，关闭它
                exitOptionDialog.close();
            } else {
                // 如果没有对话框可见，打开相应的对话框
                if (exportProgressViewModel.isExporting) {
                    closeConfirmDialog.open();
                } else {
                    exitOptionDialog.open();
                }
            }
        }
    }

    // 在启动时设置窗口位置
    Component.onCompleted: {
        // 窗口位置由 QML 属性直接控制
    }

    // 加载主窗口内容
    Item {
        id: windowContent
        anchors.fill: parent
        // 加载主窗口内容
        Loader {
            id: mainWindowLoader
            anchors.fill: parent
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/MainWindow.qml"
            // 动态更新最小窗口宽度
            onLoaded: {
                if (item && item.calculatedMinimumWindowWidth) {
                    appWindow.dynamicMinimumWidth = item.calculatedMinimumWindowWidth;
                    console.log("动态计算的最小窗口宽度:", appWindow.dynamicMinimumWidth);
                }
            }
        }

        // 绑定最小窗口宽度
        Binding {
            target: appWindow
            property: "dynamicMinimumWidth"
            value: mainWindowLoader.item ? mainWindowLoader.item.calculatedMinimumWindowWidth : 855
            when: mainWindowLoader.status === Loader.Ready
        }
    }
}
