import QtQuick
import QtQuick.Controls
import QtQuick.Window

QtObject {
    id: persistence
    required property ApplicationWindow window
    property var configService
    property real normalX: window.x
    property real normalY: window.y
    property real normalWidth: window.width
    property real normalHeight: window.height
    property bool stateTransitionInProgress: false
    property bool pendingNormalRestore: false
    function updateNormalGeometry(x, y, width, height) {
        normalX = x;
        normalY = y;
        normalWidth = width;
        normalHeight = height;
    }

    function captureNormalGeometry() {
        if (!window) {
            return;
        }

        updateNormalGeometry(window.x, window.y, window.width, window.height);
    }

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
        if (!configService || !window) {
            return;
        }

        const isSpecialState = window.visibility === Window.Maximized || window.visibility === Window.FullScreen || window.visibility === Window.Minimized;
        const widthToSave = isSpecialState ? normalWidth : window.width;
        const heightToSave = isSpecialState ? normalHeight : window.height;
        const xToSave = isSpecialState ? normalX : window.x;
        const yToSave = isSpecialState ? normalY : window.y;
        configService.setWindowState({
            x: Math.round(xToSave),
            y: Math.round(yToSave),
            width: Math.round(widthToSave),
            height: Math.round(heightToSave),
            maximized: window.visibility === Window.Maximized
        });
    }

    function beginStateTransition() {
        stateTransitionInProgress = true;
    }

    function requestRestoreNormalWindow() {
        if (!window) {
            return;
        }

        pendingNormalRestore = true;
        beginStateTransition();
        window.showNormal();
    }

    property Connections windowConnections: Connections {
        target: persistence.window
        function onXChanged() {
            if (persistence.window.visibility === Window.Windowed && !persistence.stateTransitionInProgress) {
                persistence.normalX = persistence.window.x;
            }
        }

        function onYChanged() {
            if (persistence.window.visibility === Window.Windowed && !persistence.stateTransitionInProgress) {
                persistence.normalY = persistence.window.y;
            }
        }

        function onWidthChanged() {
            if (persistence.window.visibility === Window.Windowed && !persistence.stateTransitionInProgress) {
                persistence.normalWidth = persistence.window.width;
            }
        }

        function onHeightChanged() {
            if (persistence.window.visibility === Window.Windowed && !persistence.stateTransitionInProgress) {
                persistence.normalHeight = persistence.window.height;
            }
        }

        function onVisibilityChanged() {
            if (persistence.window.visibility === Window.Windowed && persistence.pendingNormalRestore) {
                persistence.window.x = Math.round(persistence.normalX);
                persistence.window.y = Math.round(persistence.normalY);
                persistence.window.width = Math.round(persistence.normalWidth);
                persistence.window.height = Math.round(persistence.normalHeight);
                persistence.clampGeometry();
                persistence.pendingNormalRestore = false;
            } else if (persistence.window.visibility !== Window.Windowed) {
                persistence.pendingNormalRestore = false;
            }

            persistence.stateTransitionInProgress = false;
            persistence.persistGeometry();
        }

        function onScreenChanged() {
            persistence.clampGeometry();
        }
    }
}
