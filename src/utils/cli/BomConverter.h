#ifndef BOMCONVERTER_H
#define BOMCONVERTER_H

#include "BaseConverter.h"
#include "services/export/ExportProgress.h"

namespace EasyKiConverter {

/**
 * @brief BOM 表转换器
 *
 * 处理 BOM 表文件的转换逻辑。
 */
class BomConverter : public BaseConverter {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param context CLI 上下文
     * @param parent 父对象指针
     */
    explicit BomConverter(CliContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~BomConverter() override = default;

    /**
     * @brief 执行 BOM 表转换
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

#endif  // BOMCONVERTER_H
