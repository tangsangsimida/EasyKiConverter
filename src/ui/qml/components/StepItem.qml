import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Item {
    id: root
    property string label: ""
    property real progress: 0
    property int index: 1
    property color activeColor: AppStyle.colors.primary
    // 状态判断
    property bool isCompleted: progress >= 100
    property bool isActive: progress > 0 && progress < 100
    property bool isPending: progress <= 0
    implicitWidth: 80
    implicitHeight: column.implicitHeight
    ColumnLayout {
        id: column
        anchors.centerIn: parent
        spacing: 4
        // 圆圈图标
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 32
            height: 32
            radius: 16
            // 背景色：完成(绿) / 进行中(白+边框) / 等待(灰)
            color: root.isCompleted ? AppStyle.colors.success : (root.isActive ? "transparent" : AppStyle.colors.border)
            // 边框：进行中(亮色) / 其他(无)
            border.width: root.isActive ? 2 : 0
            border.color: root.isActive ? root.activeColor : "transparent"
            // 动画：进行中时呼吸效果
            SequentialAnimation on scale {
                running: root.isActive
                loops: Animation.Infinite
                NumberAnimation {
                    to: 1.1
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    to: 1.0
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
            }

            // 图标/数字
            Text {
                anchors.centerIn: parent
                text: root.isCompleted ? "✓" : root.index.toString()
                font.bold: true
                font.pixelSize: 14
                // 颜色：进行中(亮色) / 其他(白色)
                color: root.isActive ? root.activeColor : "#ffffff"
            }
        }

        // 步骤标签
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.label
            font.pixelSize: 12
            font.bold: root.isActive
            color: root.isActive || root.isCompleted ? AppStyle.colors.textPrimary : AppStyle.colors.textSecondary
        }

        // 进度百分比 (仅开始后显示)
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: Math.round(root.progress) + "%"
            font.pixelSize: 10
            color: root.isCompleted ? AppStyle.colors.success : (root.isActive ? root.activeColor : "transparent")
            visible: root.progress > 0
        }
    }
}
