import QtQuick
import QtQuick.Window

Item {
    id: resizeHandles
    anchors.fill: parent
    z: 9999 // 确保在最顶层
    // 外部属性
    property bool isMaximized: false
    visible: !isMaximized
    // 边框拖拽宽度
    property int gripSize: 8
    // 左
    MouseArea {
        width: parent.gripSize
        height: parent.height - 2 * parent.gripSize
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        cursorShape: Qt.SizeHorCursor
        onPressed: Window.window.startSystemResize(Qt.LeftEdge)
    }
    // 右
    MouseArea {
        width: parent.gripSize
        height: parent.height - 2 * parent.gripSize
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        cursorShape: Qt.SizeHorCursor
        onPressed: Window.window.startSystemResize(Qt.RightEdge)
    }
    // 上
    MouseArea {
        width: parent.width - 2 * parent.gripSize
        height: parent.gripSize
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        cursorShape: Qt.SizeVerCursor
        onPressed: Window.window.startSystemResize(Qt.TopEdge)
    }
    // 下
    MouseArea {
        width: parent.width - 2 * parent.gripSize
        height: parent.gripSize
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        cursorShape: Qt.SizeVerCursor
        onPressed: Window.window.startSystemResize(Qt.BottomEdge)
    }
    // 左上
    MouseArea {
        width: parent.gripSize * 2
        height: parent.gripSize * 2
        anchors.left: parent.left
        anchors.top: parent.top
        cursorShape: Qt.SizeFDiagCursor
        onPressed: Window.window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
    }
    // 右上
    MouseArea {
        width: parent.gripSize * 2
        height: parent.gripSize * 2
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeBDiagCursor
        onPressed: Window.window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
    }
    // 左下
    MouseArea {
        width: parent.gripSize * 2
        height: parent.gripSize * 2
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeBDiagCursor
        onPressed: Window.window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }
    // 右下
    MouseArea {
        width: parent.gripSize * 2
        height: parent.gripSize * 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeFDiagCursor
        onPressed: Window.window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }
}
