import QtQuick
import QtQuick.Controls
import QtQuick.Window

QtObject {
    id: controller

    required property ApplicationWindow window
    property var configService
    property var exportProgressController
    property var closeConfirmDialog
    property var exitOptionDialog

    property real normalWidth: window.width
    property real normalHeight: window.height

    function clampGeometry() {
        if (!window) {
            return;
        }

        const screen = window.screen;
        if (!screen) {
            return;
        }

        const bounds = screen.availableGeometry;
        if (window.width > bounds.width) {
            window.width = bounds.width;
        }
        if (window.height > bounds.height) {
            window.height = bounds.height;
        }
        if (window.x < bounds.x) {
            window.x = bounds.x;
        }
        if (window.y < bounds.y) {
            window.y = bounds.y;
        }
        if (window.x + window.width > bounds.x + bounds.width) {
            window.x = bounds.x + bounds.width - window.width;
        }
        if (window.y + window.height > bounds.y + bounds.height) {
            window.y = bounds.y + bounds.height - window.height;
        }
    }

    function persistGeometry() {
        if (!configService || !window)
            return;

        if (window.visibility === Window.Maximized) {
            configService.setWindowWidth(Math.round(normalWidth));
            configService.setWindowHeight(Math.round(normalHeight));
        } else {
            configService.setWindowWidth(Math.round(window.width));
            configService.setWindowHeight(Math.round(window.height));
        }

        if (window.visibility !== Window.Maximized && window.visibility !== Window.FullScreen && window.visibility !== Window.Minimized) {
            configService.setWindowX(Math.round(window.x));
            configService.setWindowY(Math.round(window.y));
        }

        configService.setWindowMaximized(window.visibility === Window.Maximized);
    }

    function restoreWindowState() {
        if (!window || !configService)
            return;

        clampGeometry();

        if (configService.getWindowMaximized()) {
            Qt.callLater(function () {
                if (window)
                    window.showMaximized();
            });
        }
    }

    function toggleMaximize() {
        if (!window) {
            return;
        }

        if (window.visibility === Window.Maximized || window.visibility === Window.FullScreen) {
            window.showNormal();
            clampGeometry();
            return;
        }

        persistGeometry();
        window.showMaximized();
    }

    function requestMinimize() {
        persistGeometry();
        if (window)
            window.showMinimized();
    }

    function requestClose() {
        if (window)
            window.close();
    }

    function confirmExit() {
        if (exportProgressController)
            exportProgressController.handleCloseRequest();
        persistGeometry();
        Qt.quit();
    }

    function handleEsc() {
        if (closeConfirmDialog && closeConfirmDialog.visible) {
            closeConfirmDialog.close();
            return;
        }
        if (exitOptionDialog && exitOptionDialog.visible) {
            exitOptionDialog.close();
            return;
        }

        if (exportProgressController && exportProgressController.isExporting) {
            if (closeConfirmDialog)
                closeConfirmDialog.open();
        } else if (exitOptionDialog) {
            exitOptionDialog.open();
        }
    }

    function handleClosing(close) {
        if (closeConfirmDialog && closeConfirmDialog.visible) {
            close.accepted = false;
            return;
        }
        if (exitOptionDialog && exitOptionDialog.visible) {
            close.accepted = false;
            return;
        }

        if (exportProgressController && exportProgressController.isExporting) {
            close.accepted = false;
            if (closeConfirmDialog)
                closeConfirmDialog.open();
            return;
        }

        if (configService) {
            var exitPreference = configService.getExitPreference();
            if (exitPreference === "minimize") {
                close.accepted = false;
                requestMinimize();
                return;
            }
            if (exitPreference === "exit") {
                close.accepted = false;
                confirmExit();
                return;
            }
        }

        close.accepted = false;
        if (exitOptionDialog)
            exitOptionDialog.open();
    }

    property Connections windowConnections: Connections {
        target: controller.window

        function onWidthChanged() {
            if (controller.window.visibility === Window.Windowed) {
                controller.normalWidth = controller.window.width;
            }
        }

        function onHeightChanged() {
            if (controller.window.visibility === Window.Windowed) {
                controller.normalHeight = controller.window.height;
            }
        }

        function onScreenChanged() {
            controller.clampGeometry();
        }
    }
}
