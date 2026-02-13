#ifndef IFORMATTER_H
#define IFORMATTER_H

#include "LogRecord.h"

#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 日志格式化器接口
 * 
 * 负责将 LogRecord 格式化为可输出的字符串
 */
class IFormatter {
public:
    virtual ~IFormatter() = default;

    /**
     * @brief 格式化日志记录
     * @param record 日志记录
     * @return 格式化后的字符串
     */
    virtual QString format(const LogRecord& record) = 0;

    /**
     * @brief 获取格式化器头部（如文件头信息）
     * @return 头部字符串
     */
    virtual QString header() const {
        return QString();
    }

    /**
     * @brief 克隆格式化器
     * @return 新的格式化器实例
     */
    virtual QSharedPointer<IFormatter> clone() const = 0;
};

}  // namespace EasyKiConverter

#endif  // IFORMATTER_H
