#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将符号基础图元转换为 Altium SchLib 记录。
 * @details 转换器不保存状态，仅负责基础几何、样式、颜色和部件归属映射。
 */
class AltiumSymbolPrimitiveConverter final {
public:
    /** @brief 转换矩形。 */
    static AltiumSchRectangle convertRectangle(const IR::SymbolRectangleIR& rect);

    /** @brief 转换圆角矩形。 */
    static AltiumSchRoundRectangle convertRoundRectangle(const IR::SymbolRectangleIR& rect);

    /** @brief 转换圆形。 */
    static AltiumSchEllipse convertCircle(const IR::SymbolCircleIR& circle);

    /** @brief 转换 IEEE 图元。 */
    static AltiumSchIeee convertIeee(const IR::SymbolIeeeIR& ieee);
};

}  // namespace EasyKiConverter
