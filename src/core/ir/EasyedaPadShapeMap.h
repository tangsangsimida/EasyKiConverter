#pragma once

/**
 * @file EasyedaPadShapeMap.h
 * @brief EasyEDA 焊盘形状字符串到 PadShape 枚举的映射
 *
 * EasyEDA 使用字符串表示焊盘形状（"RECT"、"ELLIPSE"、"POLYGON" 等），
 * 大小写不敏感。此映射表将其转换为 IR 的 PadShape 枚举。
 *
 * 映射来源：
 * - AltiumLayerMap::toAltiumPadShape()
 * - FootprintGraphicsGenerator::padShapeToKicad()
 */

#include "IRTypes.h"

#include <QString>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief EasyEDA 焊盘形状映射工具
 */
namespace EasyedaPadShapeMap {

/**
 * @brief EasyEDA 焊盘形状字符串 -> PadShape 枚举
 * @param easyedaShape EasyEDA 焊盘形状字符串（大小写不敏感）
 * @return 对应的 PadShape，默认返回 PadShape::Rect
 */
inline PadShape toPadShape(const QString& easyedaShape) {
    const QString upper = easyedaShape.toUpper();

    if (upper == "ELLIPSE" || upper == "CIRCLE" || upper == "ROUND") {
        return PadShape::Ellipse;
    }
    if (upper == "RECT" || upper == "RECTANGLE") {
        return PadShape::Rect;
    }
    if (upper == "OVAL") {
        return PadShape::Oval;
    }
    if (upper == "ROUNDEDRECT" || upper == "ROUNDEDRECTANGLE") {
        return PadShape::RoundRect;
    }
    if (upper == "POLYGON") {
        return PadShape::Polygon;
    }
    if (upper == "OCTAGON") {
        return PadShape::RoundRect;  // 近似映射
    }
    if (upper == "TRAPEZOID") {
        return PadShape::Trapezoid;
    }

    return PadShape::Rect;  // 默认
}

/**
 * @brief PadShape -> KiCad 焊盘形状字符串
 * @param shape 焊盘形状枚举
 * @return KiCad 格式的形状字符串
 */
inline QString toKicadPadShape(PadShape shape) {
    switch (shape) {
        case PadShape::Rect:
            return QStringLiteral("rect");
        case PadShape::Ellipse:
            return QStringLiteral("circle");
        case PadShape::Oval:
            return QStringLiteral("oval");
        case PadShape::RoundRect:
            return QStringLiteral("roundrect");
        case PadShape::Polygon:
            return QStringLiteral("custom");
        case PadShape::Trapezoid:
            return QStringLiteral("trapezoid");
        default:
            return QStringLiteral("rect");
    }
}

}  // namespace EasyedaPadShapeMap
}  // namespace IR
}  // namespace EasyKiConverter
