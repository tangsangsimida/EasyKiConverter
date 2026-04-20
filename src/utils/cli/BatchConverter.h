#ifndef BATCHCONVERTER_H
#define BATCHCONVERTER_H

#include "BaseConverter.h"
#include "services/export/ExportProgress.h"

namespace EasyKiConverter {

/**
 * @brief 批量转换器
 *
 * 处理批量元器件的转换逻辑。
 */
class BatchConverter : public BaseConverter {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param context CLI 上下文
     * @param parent 父对象指针
     */
    explicit BatchConverter(CliContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~BatchConverter() override = default;

    /**
     * @brief 执行批量转换
     * @return 成功返回 true，失败返回 false
     */
    bool execute() override;

private slots:
    /**
     * @brief 处理预加载完成
     * @param successCount 成功数量
     * @param failedCount 失败数量
     */
    void onPreloadCompleted(int successCount, int failedCount);

    /**
     * @brief 处理导出进度更新
     * @param progress 导出进度
     */
    void onProgressChanged(const ExportOverallProgress& progress);

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

#endif  // BATCHCONVERTER_H
