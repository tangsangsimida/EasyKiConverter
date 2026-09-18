#pragma once

#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

namespace EasyKiConverter {

class AltiumSchLibWriter;

/**
 * @brief 写入 SchLib 元件参数和实现关系记录的协作者。
 * @details 负责 Designator、参数字段以及 RECORD=44-48 实现关系的协议编码。
 */
class AltiumSchComponentRecordWriter final {
public:
    /** @brief 创建绑定到主 SchLib 写入器的元件记录协作者。 */
    explicit AltiumSchComponentRecordWriter(AltiumSchLibWriter& owner);

    /** @brief 写入 Designator、Value 和自定义参数记录。 */
    void writeParameters(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
    /** @brief 写入封装实现、引脚映射和实现参数记录。 */
    void writeImplementations(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
    /** @brief 计算元件参数记录数量。 */
    static int parameterRecordCount(const AltiumSchComponent& component);

private:
    AltiumSchLibWriter& m_owner;
};

}  // namespace EasyKiConverter
