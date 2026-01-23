pragma Singleton
import QtQuick
QtObject {
    // 深色模式状态
    property bool isDarkMode: false
    // 颜色主题
    readonly property var colors: QtObject {
        readonly property color primary: "#3b82f6"
        readonly property color primaryHover: "#2563eb"
        readonly property color primaryPressed: "#1e40af"
        readonly property color success: "#22c55e"
        readonly property color successLight: "#dcfce7"
        readonly property color successDark: "#166534"
        readonly property color warning: "#f59e0b"
        readonly property color warningLight: "#fef3c7"
        readonly property color warningDark: "#92400e"
        readonly property color danger: "#ef4444"
        readonly property color dangerLight: "#fee2e2"
        readonly property color dangerDark: "#991b1b"
        readonly property color background: isDarkMode ? "#0f172a" : "#f8fafc"
        readonly property color surface: isDarkMode ? "#1e293b" : "#ffffff"
        readonly property color textPrimary: isDarkMode ? "#f1f5f9" : "#1e293b"
        readonly property color textSecondary: isDarkMode ? "#94a3b8" : "#64748b"
        readonly property color textDisabled: isDarkMode ? "#64748b" : "#cbd5e1"
        readonly property color border: isDarkMode ? "#334155" : "#e2e8f0"
        readonly property color borderHover: isDarkMode ? "#475569" : "#cbd5e1"
        readonly property color borderFocus: "#3b82f6"
    }
    // 字体大小
    readonly property var fontSizes: QtObject {
        readonly property int xs: 12
        readonly property int sm: 14
        readonly property int md: 16
        readonly property int lg: 18
        readonly property int xl: 20
        readonly property int xxl: 24
        readonly property int xxxl: 32
        readonly property int display: 48
    }
    // 间距
    readonly property var spacing: QtObject {
        readonly property int xs: 4
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int lg: 16
        readonly property int xl: 20
        readonly property int xxl: 24
        readonly property int xxxl: 30
        readonly property int huge: 40
    }
    // 圆角
    readonly property var radius: QtObject {
        readonly property int sm: 6
        readonly property int md: 8
        readonly property int lg: 12
        readonly property int xl: 16
    }
    // 阴影
    readonly property var shadows: QtObject {
        readonly property var sm: QtObject {
            readonly property int horizontalOffset: 0
            readonly property int verticalOffset: 1
            readonly property int radius: 4
            readonly property int samples: 9
            readonly property color color: isDarkMode ? "#0000004d" : "#0000000d"
        }
        readonly property var md: QtObject {
            readonly property int horizontalOffset: 0
            readonly property int verticalOffset: 2
            readonly property int radius: 8
            readonly property int samples: 17
            readonly property color color: isDarkMode ? "#00000066" : "#0000001a"
        }
        readonly property var lg: QtObject {
            readonly property int horizontalOffset: 0
            readonly property int verticalOffset: 4
            readonly property int radius: 12
            readonly property int samples: 25
            readonly property color color: isDarkMode ? "#00000080" : "#00000026"
        }
    }
    // 动画时长
    readonly property var durations: QtObject {
        readonly property int fast: 150
        readonly property int normal: 300
        readonly property int slow: 500
        readonly property int themeSwitch: 600  // 主题切换动画时长
    }
    // 缓动函数
    readonly property var easings: QtObject {
        readonly property int easeOut: Easing.OutCubic
        readonly property int easeIn: Easing.InCubic
        readonly property int easeInOut: Easing.InOutCubic
    }
}
