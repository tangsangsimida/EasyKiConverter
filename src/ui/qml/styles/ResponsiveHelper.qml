pragma Singleton
import QtQuick
import QtQuick.Window

QtObject {
    // DPI 缩放因子（基于设备像素比，使用保守的缩放策略）
    readonly property real scaleFactor: Math.min(Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1.0, 1.5)
    // 当前窗口宽度
    readonly property int currentWindowWidth: Window.window ? Window.window.width : 1200
    // 基准窗口宽度（用于计算间距比例）
    readonly property int baseWindowWidth: 1200
    // 最小和最大间距
    readonly property int minSpacing: 6
    readonly property int maxSpacing: 40
    // 窗口宽度范围（用于计算间距比例）
    readonly property int minWidthForMinSpacing: 800
    readonly property int maxWidthForMaxSpacing: 2000
    // 计算动态间距（基于窗口宽度，不换行）
    readonly property var spacing: QtObject {
        // 动态间距因子（0.0 - 1.0），根据窗口宽度计算
        readonly property real widthRatio: {
            if (currentWindowWidth <= minWidthForMinSpacing)
                return 0.0;
            if (currentWindowWidth >= maxWidthForMaxSpacing)
                return 1.0;
            return (currentWindowWidth - minWidthForMinSpacing) / (maxWidthForMaxSpacing - minWidthForMinSpacing);
        }

        // xs 间距（极小间距，用于紧凑布局）
        readonly property int xs: Math.round(minSpacing + widthRatio * (minSpacing * 0.5))
        // sm 间距（小间距，用于一般间距）
        readonly property int sm: Math.round(minSpacing * 1.5 + widthRatio * (minSpacing * 0.8))
        // md 间距（中等间距，用于主要间距）
        readonly property int md: Math.round(minSpacing * 2.0 + widthRatio * (minSpacing * 1.0))
        // lg 间距（大间距，用于主要分组）
        readonly property int lg: Math.round(minSpacing * 2.5 + widthRatio * (minSpacing * 1.2))
        // xl 间距（超大间距，用于大分组）
        readonly property int xl: Math.round(minSpacing * 3.0 + widthRatio * (minSpacing * 1.4))
    }

    // 保守的字体大小（基于 DPI 缩放，但有上限）
    readonly property var fontSizes: QtObject {
        readonly property int xs: Math.round(11 * scaleFactor)
        readonly property int sm: Math.round(13 * scaleFactor)
        readonly property int md: Math.round(14 * scaleFactor)
        readonly property int lg: Math.round(15 * scaleFactor)
        readonly property int xl: Math.round(16 * scaleFactor)
    }

    // 导出选项的固定列数（始终6列，不换行）
    readonly property int exportOptionColumns: 6
    // 导出选项的最小宽度（紧凑模式）
    readonly property int exportOptionMinWidth: 100
    // 导出模式选项的最小宽度（需要包含说明文字，但在窄窗口下允许压缩）
    readonly property int exportModeOptionMinWidth: {
        if (currentWindowWidth <= 0)
            return 140;
        // 在窄窗口下，允许压缩到更小的宽度
        if (currentWindowWidth < 1000)
            return 120;
        return 140;
    }

    // 导出选项的推荐宽度（根据窗口宽度动态调整）
    readonly property int exportOptionPreferredWidth: {
        if (currentWindowWidth <= 0)
            return 120;
        var availableWidth = currentWindowWidth - spacing.xl * 2 - spacing.lg * (exportOptionColumns - 1);
        var preferredWidth = Math.floor(availableWidth / exportOptionColumns);
        return Math.max(exportOptionMinWidth, Math.min(preferredWidth, 180));
    }

    // 导出模式选项的推荐宽度（根据窗口宽度动态调整）
    readonly property int exportModeOptionPreferredWidth: {
        if (currentWindowWidth <= 0)
            return 140;
        var availableWidth = currentWindowWidth - spacing.xl * 2 - spacing.lg * (exportOptionColumns - 1);
        var preferredWidth = Math.floor(availableWidth / exportOptionColumns);
        return Math.max(exportModeOptionMinWidth, Math.min(preferredWidth, 220));
    }

    // 计算最小窗口宽度（基于间距达到最小值时）
    readonly property int minimumWindowWidth: {
        // 6个选项的最小宽度总和
        var optionsMinWidth = 80 * 5 + 280; // 5个普通选项80px + 导出模式280px
        // 5个间距的最小值
        var spacingMinWidth = minSpacing * 5;
        // 卡片的左右内边距（AppStyle.spacing.xl ≈ 24px）
        var cardPadding = 24 * 2;
        // 卡片的边框（1px * 2）
        var cardBorder = 2;
        // 最小窗口宽度 = 选项宽度 + 间距 + 内边距 + 边框
        return optionsMinWidth + spacingMinWidth + cardPadding + cardBorder;
    }

    // 检查是否为窄窗口（可能需要压缩间距）
    function isNarrowWindow() {
        return currentWindowWidth < 1000;
    }

    // 检查是否为宽窗口（可以增加间距）
    function isWideWindow() {
        return currentWindowWidth > 1600;
    }

    // 获取适合当前窗口的间距系数
    function getSpacingMultiplier() {
        if (currentWindowWidth <= 0)
            return 1.0;
        var ratio = currentWindowWidth / baseWindowWidth;
        // 限制在 0.5 - 1.5 之间
        return Math.max(0.5, Math.min(ratio, 1.5));
    }
}
