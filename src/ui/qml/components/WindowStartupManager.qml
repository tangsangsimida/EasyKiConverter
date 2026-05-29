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
        if (window && window.screen && window.screen.availableGeometry) {
            return window.screen.availableGeometry;
        }

        var w = window ? window.defaultWidth : 900;
        var h = window ? window.defaultHeight : 680;
        return Qt.rect(0, 0, w, h);
    }

    function resolveStartupGeometry() {
        var bounds = availableBounds();
        if (!bounds) {
            return {
                x: 0,
                y: 0,
                width: 900,
                height: 680
            };
        }
        var savedWidth = windowState.width;
        var savedHeight = windowState.height;
        var savedX = windowState.x;
        var savedY = windowState.y;
        var startupWidth = savedWidth > 0 ? savedWidth : (window ? window.defaultWidth : 900);
        var startupHeight = savedHeight > 0 ? savedHeight : (window ? window.defaultHeight : 680);
        var startupX = savedX > 0 ? savedX : bounds.x + Math.round((bounds.width - startupWidth) / 2);
        var startupY = savedY > 0 ? savedY : bounds.y + Math.round((bounds.height - startupHeight) / 2);
        return {
            x: startupX,
            y: startupY,
            width: startupWidth,
            height: startupHeight
        };
    }

    property int _retryCount: 0
    function initializeWindow() {
        if (!window || !configService || !persistenceManager) {
            if (_retryCount < 20) {
                _retryCount++;
                Qt.callLater(initializeWindow);
            } else {
                console.error("WindowStartupManager: 依赖在 20 次重试后仍未就绪, window=", !!window, "configService=", !!configService, "persistenceManager=", !!persistenceManager);
            }
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
