pragma Singleton
import QtQuick

QtObject {
    // 窗口宽度 — 由 MainWindow 通过 Binding 写入，singleton 自身无法感知 Window
    property int windowWidth: 1200
    // 断点系统
    enum Breakpoint {
        Compact,
        Medium,
        Wide,
        ExtraWide
    }

    readonly property int breakpoint: {
        if (windowWidth < 900)
            return ResponsiveHelper.Breakpoint.Compact;
        if (windowWidth < 1280)
            return ResponsiveHelper.Breakpoint.Medium;
        if (windowWidth < 1800)
            return ResponsiveHelper.Breakpoint.Wide;
        return ResponsiveHelper.Breakpoint.ExtraWide;
    }

    readonly property bool isCompact: breakpoint === ResponsiveHelper.Breakpoint.Compact
    readonly property bool isMedium: breakpoint === ResponsiveHelper.Breakpoint.Medium
    readonly property bool isWide: breakpoint === ResponsiveHelper.Breakpoint.Wide
    readonly property bool isExtraWide: breakpoint === ResponsiveHelper.Breakpoint.ExtraWide
    readonly property bool isAtLeastMedium: breakpoint >= ResponsiveHelper.Breakpoint.Medium
    readonly property bool isAtLeastWide: breakpoint >= ResponsiveHelper.Breakpoint.Wide
    // 兼容旧接口（移除 DPI 缩放，固定为 1.0）
    readonly property real scaleFactor: 1.0
    // 自适应间距（逻辑像素，根据断点插值）
    readonly property var spacing: QtObject {
        readonly property int xs: responsive(2, 4, 4, 4)
        readonly property int sm: responsive(4, 8, 8, 8)
        readonly property int md: responsive(8, 12, 12, 12)
        readonly property int lg: responsive(12, 16, 16, 16)
        readonly property int xl: responsive(16, 20, 24, 24)
        readonly property int xxl: responsive(20, 24, 30, 30)
        readonly property int huge: responsive(24, 32, 40, 40)
    }

    // 自适应边距
    readonly property int contentMargin: responsive(12, 24, 32, 40)
    readonly property int cardSpacing: responsive(16, 24, 30, 30)
    // 内容最大宽度（4K 下不无限拉宽）
    readonly property int contentMaxWidth: 1320
    // 导出选项布局
    readonly property int exportOptionColumns: isCompact ? 3 : isMedium ? 4 : 6
    // 兼容旧接口
    readonly property int exportOptionMinWidth: 100
    readonly property int exportModeOptionMinWidth: 140
    readonly property int exportOptionPreferredWidth: 120
    readonly property int exportModeOptionPreferredWidth: 140
    // 最小窗口宽度（兼容旧接口）
    readonly property int minimumWindowWidth: 740
    // 兼容旧接口（已修复，使用实际窗口宽度）
    readonly property int baseWindowWidth: windowWidth
    readonly property int minSpacing: 6
    readonly property int maxSpacing: 40
    readonly property int minWidthForMinSpacing: 800
    readonly property int maxWidthForMaxSpacing: 2000
    // 兼容旧接口的 fontSizes（不再缩放，使用逻辑像素）
    readonly property var fontSizes: QtObject {
        readonly property int xs: 11
        readonly property int sm: 13
        readonly property int md: 14
        readonly property int lg: 15
        readonly property int xl: 16
    }

    // 根据断点返回不同值
    function responsive(compact, medium, wide, extraWide) {
        switch (breakpoint) {
        case ResponsiveHelper.Breakpoint.Compact:
            return compact;
        case ResponsiveHelper.Breakpoint.Medium:
            return medium;
        case ResponsiveHelper.Breakpoint.Wide:
            return wide;
        case ResponsiveHelper.Breakpoint.ExtraWide:
            return extraWide;
        }
        return medium;
    }

    // 兼容旧接口
    function isNarrowWindow() {
        return isCompact;
    }

    function isWideWindow() {
        return isAtLeastWide;
    }

    function getSpacingMultiplier() {
        return responsive(0.75, 1.0, 1.0, 1.0);
    }
}
