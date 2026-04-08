#pragma once

#include "ExportTypeStage.h"

namespace EasyKiConverter {

/**
 * @brief 封装导出阶段
 *
 * 管理封装(Footprint)的并行导出任务。
 * 每个元器件的封装导出由FootprintExportWorker执行。
 *
 * 并发配置:
 * - 最大并发数: 3（封装导出是磁盘I/O密集型）
 */
class FootprintExportStage : public ExportTypeStage {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit FootprintExportStage(QObject* parent = nullptr);

protected:
    /**
     * @brief 创建FootprintExportWorker实例
     * @return 新的worker对象
     */
    QObject* createWorker() override;

    /**
     * @brief 启动worker执行封装导出
     * @param worker worker实例
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     */
    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override;

private:
    struct ExportOptions m_options;  ///< 导出选项
};

}  // namespace EasyKiConverter
