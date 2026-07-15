#pragma once

#include <cstdint>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief EasyEDA 层 ID → Altium 层编号映射
 *
 * EasyEDA 和 Altium 共享 Protel/Altium 层编号体系，大部分层 1:1 映射。
 *
 * 层编号对照：
 *   1 = Top Layer, 2-31 = Inner Layers, 32 = Bottom Layer
 *   33 = Top Overlay, 34 = Bottom Overlay
 *   35 = Top Paste, 36 = Bottom Paste
 *   37 = Top Solder Mask, 38 = Bottom Solder Mask
 *   39-54 = Internal Planes 1-16
 *   55 = Drill Guide, 56 = Keepout
 *   57-72 = Mechanical 1-16
 *   73 = Drill Drawing, 74 = Multi Layer
 */
namespace AltiumLayerMap {

/** @brief EasyEDA 层 ID → Altium 层字节（大多数 1:1 映射） */
constexpr uint8_t toAltiumLayer(int easyedaLayerId) {
    return static_cast<uint8_t>(qBound(0, easyedaLayerId, 255));
}

/**
 * @brief Altium 层字节 → V7 Layer ID
 * @details V7 Layer ID 用于 PcbLib 中的 V7_LAYER 参数
 */
uint32_t toV7LayerId(uint8_t layer);

/**
 * @brief Altium 层字节 → 层名称字符串
 * @details 用于 Region/ComponentBody 的 V7_LAYER 参数
 */
QString toLayerName(uint8_t layer);

/**
 * @brief EasyEDA 焊盘形状 → Altium 焊盘形状
 * @details
 *   EasyEDA: "ELLIPSE" → Altium: Round (1)
 *   EasyEDA: "RECT" → Altium: Rectangular (2)
 *   EasyEDA: "POLYGON" → Altium: RoundedRectangle (9)
 */
uint8_t toAltiumPadShape(const QString& easyedaShape);

/**
 * @brief EasyEDA 引脚类型 → Altium 电气类型
 * @details
 *   EasyEDA PinType: Unspecified=0, Input=1, Output=2, Bidirectional=3, Power=4
 *   Altium: Input=0, IO=1, Output=2, OpenCollector=3, Passive=4, HiZ=5, Power=7
 */
uint8_t toAltiumElectricalType(int easyedaPinType);

/**
 * @brief EasyEDA 引脚方向（角度）→ Altium 引脚方向
 * @details
 *   EasyEDA: 0° → Right(0), 90° → Up(1), 180° → Left(2), 270° → Down(3)
 */
uint8_t toAltiumPinOrientation(int easyedaRotation);

}  // namespace AltiumLayerMap
}  // namespace EasyKiConverter
