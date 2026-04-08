#pragma once

#include "ExportTypeStage.h"

namespace EasyKiConverter {

/**
 * @brief 数据手册导出阶段
 *
 * 管理数据手册(Datasheet)的并行导出任务。
 * 每个元器件的数据手册导出由DatasheetExportWorker执行。
 *
 * 并发配置:
 * - 最大并发数: 2（PDF文件较大，下载+I/O较慢）
 */
class DatasheetExportStage : public ExportTypeStage {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit DatasheetExportStage(QObject* parent = nullptr);

protected:
    /**
     * @brief 创建DatasheetExportWorker实例
     * @return 新的worker对象
     */
    QObject* createWorker() override;

    /**
     * @brief 启动worker执行数据手册导出
     * @param worker worker实例
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     */
    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override;

private:
    struct ExportOptions m_options;  ///< 导出选项
};

}  // namespace EasyKiConverter
