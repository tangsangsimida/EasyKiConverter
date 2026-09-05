/**
 * @file SymbolIR.cpp
 * @brief 符号 IR 辅助函数实现
 */

#include "SymbolIR.h"

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 将颜色十六进制字符串解析为 QColor
 * @param colorStr 颜色字符串（如 "#FF0000" 或 "rgb(255,0,0)"）
 * @param fallback 解析失败时的默认颜色
 * @return 解析后的 QColor
 */
QColor parseColor(const QString& colorStr, const QColor& fallback) {
    if (colorStr.isEmpty()) {
        return fallback;
    }

    // 尝试直接解析十六进制格式
    QColor color(colorStr);
    if (color.isValid()) {
        return color;
    }

    // 尝试添加 "#" 前缀
    if (!colorStr.startsWith('#')) {
        color = QColor('#' + colorStr);
        if (color.isValid()) {
            return color;
        }
    }

    return fallback;
}

}  // namespace IR
}  // namespace EasyKiConverter
