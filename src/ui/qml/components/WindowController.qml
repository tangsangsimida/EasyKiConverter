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
    readonly property WindowPersistenceManager persistenceManager: WindowPersistenceManager {
        window: controller.window
        configService: controller.configService
    }
    readonly property WindowStartupManager startupManager: WindowStartupManager {
        window: controller.window
        configService: controller.configService
        persistenceManager: controller.persistenceManager
    }

    function initializeWindow() {
        startupManager.initializeWindow();
    }

    function toggleMaximize() {
        if (!window) {
            return;
        }

        if (window.visibility === Window.Maximized || window.visibility === Window.FullScreen) {
            persistenceManager.requestRestoreNormalWindow();
            return;
        }

        persistenceManager.captureNormalGeometry();
        persistenceManager.persistGeometry();
        persistenceManager.beginStateTransition();
        window.showMaximized();
    }

    function requestMinimize() {
        persistenceManager.captureNormalGeometry();
        persistenceManager.persistGeometry();
        persistenceManager.beginStateTransition();
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
        persistenceManager.persistGeometry();
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
}
