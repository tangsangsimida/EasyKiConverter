#pragma once

#include "ExportTypeStage.h"

namespace EasyKiConverter {

/**
 * @brief 预览图导出阶段
 *
 * 管理预览图(Preview Images)的并行导出任务。
 * 每个元器件的预览图导出由PreviewImagesExportWorker执行。
 *
 * 并发配置:
 * - 最大并发数: 4（预览图导出是图像处理+I/O密集型）
 */
class PreviewImagesExportStage : public ExportTypeStage {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit PreviewImagesExportStage(QObject* parent = nullptr);

protected:
    /**
     * @brief 创建PreviewImagesExportWorker实例
     * @return 新的worker对象
     */
    QObject* createWorker() override;

    /**
     * @brief 启动worker执行预览图导出
     * @param worker worker实例
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     */
    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override;

private:
    struct ExportOptions m_options;  ///< 导出选项
};

}  // namespace EasyKiConverter
