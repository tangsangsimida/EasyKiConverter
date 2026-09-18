#pragma once

#include "models/AltiumPcbComponent.h"

namespace EasyKiConverter {

/**
 * @brief 描述 Altium PcbLib 封装图元的整数包围盒。
 * @details 包围盒同时服务于原点归一化和三维元件体轮廓生成。
 */
struct AltiumFootprintBounds {
    qint64 minX = 0;
    qint64 minY = 0;
    qint64 maxX = 0;
    qint64 maxY = 0;
    bool valid = false;
};

/**
 * @brief 统一处理 PcbLib 封装图元的包围盒和原点平移。
 * @details 该类不负责 IR 转换或文件写入，只复用导出器中的几何边界规则。
 */
class AltiumFootprintGeometryNormalizer final {
public:
    /**
     * @brief 计算封装所有二维图元的包围盒。
     * @param component 待计算的 PcbLib 封装。
     * @param clampRegionCoordinates 是否将区域顶点有限值安全转换为整数。
     * @return 图元存在时返回有效包围盒。
     */
    static AltiumFootprintBounds computeBounds(const AltiumPcbComponent& component, bool clampRegionCoordinates);

    /**
     * @brief 将封装所有相关图元按同一偏移量平移。
     * @param component 待平移的 PcbLib 封装。
     * @param offsetX X 方向偏移量。
     * @param offsetY Y 方向偏移量。
     */
    static void translate(AltiumPcbComponent& component, int offsetX, int offsetY);
};

}  // namespace EasyKiConverter
