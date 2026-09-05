#pragma once

/**
 * @file EasyedaLayerMap.h
 * @brief EasyEDA 数字层 ID 到通用 LayerType 的映射
 *
 * EasyEDA 使用整数层 ID（1=Top, 2=Bottom, 99=KeepOut 等），
 * 与 Altium/Protel 的层编号体系大部分兼容。
 * 此映射表将 EasyEDA 层 ID 转换为 IR 的 LayerType 枚举。
 *
 * 映射来源：
 * - KiCad 导出器 FootprintGraphicsGenerator::layerIdToKicad()
 * - Altium 导出器中直接使用 layerId 的逻辑
 */

#include "IRTypes.h"

namespace EasyKiConverter {
namespace IR {

/**
 * @brief EasyEDA 层 ID 映射工具
 */
namespace EasyedaLayerMap {

/**
 * @brief EasyEDA 数字层 ID -> 通用 LayerType
 * @param easyedaLayerId EasyEDA 层 ID
 * @return 对应的 LayerType，无法映射时返回 LayerType::Unknown
 */
inline LayerType toLayerType(int easyedaLayerId) {
    switch (easyedaLayerId) {
        case 0:
            return LayerType::TopSilk;  // EasyEDA Body/封装体层 → 顶层丝印
        case 1:
            return LayerType::TopCopper;
        case 2:
            return LayerType::BottomCopper;
        case 3:
            return LayerType::TopSilk;
        case 4:
            return LayerType::BottomSilk;
        case 5:
            return LayerType::TopPaste;
        case 6:
            return LayerType::BottomPaste;
        case 7:
            return LayerType::TopMask;
        case 8:
            return LayerType::BottomMask;
        case 9:
            return LayerType::TopCopper;  // EasyEDA 内层 1 映射到顶层（近似）
        case 10:
            return LayerType::EdgeCuts;
        case 11:
            return LayerType::EdgeCuts;
        case 12:
            return LayerType::UserDefined;  // Dwgs.User
        case 13:
            return LayerType::TopAssembly;  // F.Fab
        case 14:
            return LayerType::BottomAssembly;  // B.Fab
        case 15:
            return LayerType::UserDefined;
        case 20:
            return LayerType::UserDefined;  // Cmts.User
        case 21:
            return LayerType::UserDefined;  // Eco1.User
        case 22:
            return LayerType::UserDefined;  // Eco2.User
        case 99:
            return LayerType::KeepOut;  // 禁止区
        case 100:
            return LayerType::TopAssembly;  // F.Fab (EasyEDA 扩展)
        case 101:
            return LayerType::TopSilk;  // F.SilkS (EasyEDA 扩展)
        case 102:
            return LayerType::BottomAssembly;  // B.Fab (EasyEDA 扩展)
        case 103:
            return LayerType::BottomSilk;  // B.SilkS (EasyEDA 扩展)
        default:
            return LayerType::Unknown;
    }
}

}  // namespace EasyedaLayerMap
}  // namespace IR
}  // namespace EasyKiConverter
