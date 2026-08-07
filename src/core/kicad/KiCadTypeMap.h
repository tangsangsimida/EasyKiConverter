#pragma once

/**
 * @file KiCadTypeMap.h
 * @brief IR 通用枚举 → KiCad 格式字符串的映射
 *
 * 将 IR 层的 PadShape、PinDirection、PinElectricalType 枚举
 * 转换为 KiCad 文件格式所需的字符串或角度值。
 *
 * @note 此文件属于 KiCad 导出器层，不应被 IR 层引用。
 */

#include "core/ir/IRTypes.h"

namespace EasyKiConverter {

/**
 * @brief IR 枚举 → KiCad 格式映射工具
 */
namespace KiCadTypeMap {

/**
 * @brief PadShape → KiCad 焊盘形状字符串
 * @param shape IR 焊盘形状枚举
 * @return KiCad 格式的形状字符串（如 "rect"、"circle"、"oval"）
 */
inline QString toKicadPadShape(IR::PadShape shape) {
    switch (shape) {
        case IR::PadShape::Rect:
            return QStringLiteral("rect");
        case IR::PadShape::Ellipse:
            return QStringLiteral("circle");
        case IR::PadShape::Oval:
            return QStringLiteral("oval");
        case IR::PadShape::RoundRect:
            return QStringLiteral("roundrect");
        case IR::PadShape::Polygon:
            return QStringLiteral("custom");
        case IR::PadShape::Trapezoid:
            return QStringLiteral("trapezoid");
        default:
            return QStringLiteral("rect");
    }
}

/**
 * @brief PinDirection → KiCad 引脚方向角度
 * @param dir IR 引脚方向枚举
 * @return KiCad 格式的角度（度）
 *
 * @note EasyEDA rotation=0 对应 KiCad angle 180（引脚从左向右绘制）
 */
inline double toKicadAngle(IR::PinDirection dir) {
    switch (dir) {
        case IR::PinDirection::Right:
            return 180.0;
        case IR::PinDirection::Up:
            return 270.0;
        case IR::PinDirection::Left:
            return 0.0;
        case IR::PinDirection::Down:
            return 90.0;
        default:
            return 180.0;
    }
}

/**
 * @brief PinElectricalType → KiCad 引脚电气类型字符串
 * @param type IR 引脚电气类型枚举
 * @return KiCad 格式的电气类型字符串
 */
inline const char* toKicadPinType(IR::PinElectricalType type) {
    switch (type) {
        case IR::PinElectricalType::Unspecified:
            return "unspecified";
        case IR::PinElectricalType::Input:
            return "input";
        case IR::PinElectricalType::Output:
            return "output";
        case IR::PinElectricalType::Bidirectional:
            return "bidirectional";
        case IR::PinElectricalType::Passive:
            return "passive";
        case IR::PinElectricalType::Power:
            return "power_in";
        case IR::PinElectricalType::OpenCollector:
            return "open_collector";
        case IR::PinElectricalType::OpenEmitter:
            return "open_emitter";
        default:
            return "unspecified";
    }
}

}  // namespace KiCadTypeMap
}  // namespace EasyKiConverter
