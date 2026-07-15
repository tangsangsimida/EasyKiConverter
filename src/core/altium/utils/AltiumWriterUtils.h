#pragma once

#include <QString>

namespace EasyKiConverter {

/**
 * @brief Altium 写入器共享工具
 * @details 提供 SchLib 和 PcbLib 写入器共用的工具函数。
 */
namespace AltiumWriterUtils {

/**
 * @brief 获取元件的 Section Key（OLE 存储键）
 * @param name 元件名称
 * @return 截断到 31 字符且 '/' 替换为 '_' 的安全键名
 *
 * @details Altium OLE 复合文档中，每个元件存储在以 SectionKey 命名的存储区中。
 *          规则：截断到 31 字符，替换 '/' 为 '_'。
 *          参考：AltiumSharp WriterUtilities.GetSectionKeyFromName()
 *
 * @code
 * QString key = getSectionKey("Resistor/0402");
 * // key = "Resistor_0402"
 * @endcode
 */
inline QString getSectionKey(const QString& name) {
    if (name.isEmpty()) return "_";
    QString key = name.left(31);
    key.replace('/', '_');
    return key;
}

}  // namespace AltiumWriterUtils
}  // namespace EasyKiConverter
