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
    property bool forceExitRequested: false
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

    function processCloseRequest() {
        if (closeConfirmDialog && closeConfirmDialog.visible) {
            return;
        }
        if (exitOptionDialog && exitOptionDialog.visible) {
            return;
        }

        if (exportProgressController && exportProgressController.isExporting) {
            if (closeConfirmDialog)
                closeConfirmDialog.open();
            return;
        }

        if (configService) {
            var exitPreference = configService.getExitPreference();
            if (exitPreference === "minimize") {
                requestMinimize();
                return;
            }
            if (exitPreference === "exit") {
                confirmExit();
                return;
            }
        }

        if (exitOptionDialog)
            exitOptionDialog.open();
    }

    function requestClose() {
        processCloseRequest();
    }

    function resumeExport() {
        forceExitRequested = false;
        if (closeConfirmDialog)
            closeConfirmDialog.close();
        if (window) {
            window.raise();
            window.requestActivate();
        }
    }

    function confirmExit() {
        forceExitRequested = true;
        if (closeConfirmDialog)
            closeConfirmDialog.close();
        if (exitOptionDialog)
            exitOptionDialog.close();
        if (exportProgressController)
            exportProgressController.handleCloseRequest();
        persistenceManager.persistGeometry();
        if (window)
            window.close();
        Qt.callLater(Qt.quit);
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
        if (forceExitRequested) {
            close.accepted = true;
            return;
        }
        close.accepted = false;
        processCloseRequest();
    }
}
