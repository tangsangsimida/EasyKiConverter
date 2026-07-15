#pragma once

#include <QString>

namespace EasyKiConverter {

/**
 * @brief Altium 坐标转换工具
 * @details 处理 EasyKiConverter 内部坐标 → Altium 各种坐标格式的转换。
 *
 * 坐标系统对照：
 * - EasyKiConverter: 1 mil = 10,000 原始单位 (raw)
 * - Altium DXP 单位: 1 DXP = 10 mil = 100,000 raw
 * - Altium PCB 原始单位: 1 mil = 10,000 raw（与 EasyKiConverter 相同）
 * - Schematic Units: 1 mil = 10 单位 (raw / 1000)
 */
namespace AltiumCoord {

/** @brief 原始单位 → Altium PCB 原始单位（1:1 映射，无需转换） */
constexpr int32_t toPcbRaw(int raw) { return static_cast<int32_t>(raw); }

/** @brief 原始单位 → DXP 整数部分（SchLib 参数记录使用） */
constexpr int16_t toDxpInt(int raw) { return static_cast<int16_t>(raw / 100000); }

/** @brief 原始单位 → DXP 小数部分（SchLib 参数记录使用） */
constexpr int32_t toDxpFrac(int raw) { return raw % 100000; }

/** @brief 原始单位 → Schematic Units（多边形/折线顶点使用） */
constexpr int32_t toSchematicUnits(int raw) { return raw / 1000; }

/** @brief 原始单位 → mil 值 */
constexpr double toMils(int raw) { return raw / 10000.0; }

/** @brief 毫米 → Altium mil 字符串（如 "100mil"） */
inline QString mmToMilString(double mm) {
    return QString("%1mil").arg(mm / 0.0254, 0, 'f', 6);
}

/** @brief 原始单位 → mil 字符串 */
inline QString rawToMilString(int raw) {
    return QString("%1mil").arg(raw / 10000.0, 0, 'f', 6);
}

/**
 * @brief 线宽索引转换
 * @details EasyEDA 使用实际线宽，Altium SchLib 使用索引 (0-3)
 *   0 = Small (~1mil), 1 = Medium (~2mil), 2 = Large (~4mil), 3 = Largest (~6mil)
 */
constexpr int lineWidthToIndex(int rawWidth) {
    double mils = rawWidth / 10000.0;
    if (mils >= 5.0) return 3;
    if (mils >= 3.0) return 2;
    if (mils >= 1.5) return 1;
    return 0;
}

}  // namespace AltiumCoord
}  // namespace EasyKiConverter
