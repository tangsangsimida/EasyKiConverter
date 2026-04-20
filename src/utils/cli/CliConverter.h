#ifndef CLICONVERTER_H
#define CLICONVERTER_H

#include "utils/CommandLineParser.h"

#include <QObject>
#include <QString>

namespace EasyKiConverter {

class CliContext;
class BaseConverter;

/**
 * @brief CLI 转换器主协调器
 *
 * 协调 CLI 模式下的转换流程。
 */
class CliConverter : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parser 命令行解析器
     * @param parent 父对象指针
     */
    explicit CliConverter(const CommandLineParser& parser, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~CliConverter() override;

    /**
     * @brief 执行转换
     * @return 成功返回 true，失败返回 false
     */
    bool execute();

    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    QString errorMessage() const;

private:
    /**
     * @brief 创建转换器
     * @param mode CLI 模式
     * @return 转换器指针
     */
    BaseConverter* createConverter(CommandLineParser::CliMode mode);

    CliContext* m_context{nullptr};
    BaseConverter* m_converter{nullptr};
};

}  // namespace EasyKiConverter

#endif  // CLICONVERTER_H
