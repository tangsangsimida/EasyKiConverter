#pragma once

/**
 * @file EasyedaPinTypeMap.h
 * @brief EasyEDA 引脚类型和方向到 IR 枚举的映射
 *
 * EasyEDA 使用整数编码引脚类型（0-4）和旋转角度（0/90/180/270）。
 * 此映射表将其转换为 IR 的 PinElectricalType 和 PinDirection 枚举。
 *
 * 映射来源：
 * - SymbolData.h 中的 PinType 枚举（0=Unspecified, 1=Input, 2=Output, 3=Bidirectional, 4=Power）
 * - 导出器中的旋转角度处理逻辑
 */

#include "IRTypes.h"

namespace EasyKiConverter {
namespace IR {

/**
 * @brief EasyEDA 引脚类型映射工具
 */
namespace EasyedaPinTypeMap {

/**
 * @brief EasyEDA 整数引脚类型 -> PinElectricalType 枚举
 * @param easyedaPinType EasyEDA 引脚类型整数（0-4）
 * @return 对应的 PinElectricalType
 *
 * EasyEDA 引脚类型编码：
 * - 0 = Unspecified（未指定）
 * - 1 = Input（输入）
 * - 2 = Output（输出）
 * - 3 = Bidirectional（双向）
 * - 4 = Passive（无源）
 *
 * @note 与现有 PinType 枚举值一一对应，但类型安全。
 */
inline PinElectricalType toPinElectricalType(int easyedaPinType) {
    switch (easyedaPinType) {
        case 0:
            return PinElectricalType::Unspecified;
        case 1:
            return PinElectricalType::Input;
        case 2:
            return PinElectricalType::Output;
        case 3:
            return PinElectricalType::Bidirectional;
        case 4:
            return PinElectricalType::Passive;
        default:
            return PinElectricalType::Unspecified;
    }
}

/**
 * @brief EasyEDA 旋转角度 -> PinDirection 枚举
 * @param easyedaRotation EasyEDA 旋转角度（0/90/180/270）
 * @return 对应的 PinDirection
 *
 * EasyEDA 旋转约定：
 * - 0   = 向右
 * - 90  = 向上（Y 轴翻转后）
 * - 180 = 向左
 * - 270 = 向下（Y 轴翻转后）
 */
inline PinDirection toPinDirection(int easyedaRotation) {
    switch (easyedaRotation % 360) {
        case 0:
            return PinDirection::Right;
        case 90:
            return PinDirection::Up;
        case 180:
            return PinDirection::Left;
        case 270:
            return PinDirection::Down;
        default:
            return PinDirection::Right;
    }
}

/**
 * @brief PinDirection -> KiCad 引脚方向角度
 * @param dir 引脚方向枚举
 * @return KiCad 格式的角度（度）
 */
inline double toKicadAngle(PinDirection dir) {
    switch (dir) {
        case PinDirection::Right:
            return 0.0;
        case PinDirection::Left:
            return 180.0;
        case PinDirection::Up:
            return 90.0;
        case PinDirection::Down:
            return 270.0;
        default:
            return 0.0;
    }
}

/**
 * @brief PinElectricalType -> KiCad 引脚电气类型字符串
 * @param type 引脚电气类型枚举
 * @return KiCad 格式的电气类型字符串
 */
inline const char* toKicadPinType(PinElectricalType type) {
    switch (type) {
        case PinElectricalType::Input:
            return "input";
        case PinElectricalType::Output:
            return "output";
        case PinElectricalType::Bidirectional:
            return "bidirectional";
        case PinElectricalType::Passive:
            return "passive";
        case PinElectricalType::Power:
            return "power_in";
        case PinElectricalType::OpenCollector:
            return "open_collector";
        case PinElectricalType::OpenEmitter:
            return "open_emitter";
        default:
            return "passive";
    }
}

}  // namespace EasyedaPinTypeMap
}  // namespace IR
}  // namespace EasyKiConverter
