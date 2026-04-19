import QtQuick
import QtQuick.Controls
import QtQuick.Window

QtObject {
    id: startup
    required property ApplicationWindow window
    property var configService
    property var persistenceManager
    property var windowState: configService ? configService.getWindowState() : ({
            x: -9999,
            y: -9999,
            width: -1,
            height: -1,
            maximized: false
        })
    function availableBounds() {
        if (window && window.screen) {
            return window.screen.availableGeometry;
        }

        return Qt.rect(0, 0, window.defaultWidth, window.defaultHeight);
    }

    function resolveStartupGeometry() {
        const bounds = availableBounds();
        const savedWidth = windowState.width;
        const savedHeight = windowState.height;
        const savedX = windowState.x;
        const savedY = windowState.y;
        const startupWidth = savedWidth > 0 ? savedWidth : window.defaultWidth;
        const startupHeight = savedHeight > 0 ? savedHeight : window.defaultHeight;
        const startupX = savedX > 0 ? savedX : bounds.x + Math.round((bounds.width - startupWidth) / 2);
        const startupY = savedY > 0 ? savedY : bounds.y + Math.round((bounds.height - startupHeight) / 2);
        return {
            x: startupX,
            y: startupY,
            width: startupWidth,
            height: startupHeight
        };
    }

    function initializeWindow() {
        if (!window || !configService || !persistenceManager) {
            return;
        }

        const geometry = resolveStartupGeometry();
        window.x = geometry.x;
        window.y = geometry.y;
        window.width = geometry.width;
        window.height = geometry.height;
        persistenceManager.clampGeometry();
        persistenceManager.updateNormalGeometry(window.x, window.y, window.width, window.height);
        window.visible = true;
        window.show();
        window.raise();
        window.requestActivate();
        if (windowState.maximized) {
            persistenceManager.beginStateTransition();
            Qt.callLater(function () {
                if (window) {
                    window.showMaximized();
                }
            });
        } else {
            persistenceManager.persistGeometry();
        }
    }
}
