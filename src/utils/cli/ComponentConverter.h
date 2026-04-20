#ifndef COMPONENTCONVERTER_H
#define COMPONENTCONVERTER_H

#include "BaseConverter.h"

namespace EasyKiConverter {

/**
 * @brief 单个元器件转换器
 *
 * 处理单个元器件的转换逻辑。
 */
class ComponentConverter : public BaseConverter {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param context CLI 上下文
     * @param parent 父对象指针
     */
    explicit ComponentConverter(CliContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ComponentConverter() override = default;

    /**
     * @brief 执行单个元器件转换
     * @return 成功返回 true，失败返回 false
     */
    bool execute() override;

private slots:
    /**
     * @brief 处理导出完成
     * @param successCount 成功数量
     * @param failedCount 失败数量
     */
    void onExportCompleted(int successCount, int failedCount);

    /**
     * @brief 处理导出失败
     * @param error 错误信息
     */
    void onExportFailed(const QString& error);

private:
    bool m_exportFinished{false};
    bool m_exportSuccess{false};
    int m_successCount{0};
    int m_failedCount{0};
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCONVERTER_H
