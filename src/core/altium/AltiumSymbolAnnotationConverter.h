#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将符号文本类图元转换为 Altium SchLib 记录。
 * @details 转换器不保存状态，仅负责坐标、字体、颜色、显示属性和部件归属映射。
 */
class AltiumSymbolAnnotationConverter final {
public:
    /** @brief 转换符号文本。 */
    static AltiumSchText convertText(const IR::SymbolTextIR& text);

    /** @brief 转换符号文本框。 */
    static AltiumSchTextFrame convertTextFrame(const IR::SymbolTextFrameIR& frame);

    /** @brief 转换符号图片。 */
    static AltiumSchImage convertImage(const IR::SymbolImageIR& image);
};

}  // namespace EasyKiConverter
