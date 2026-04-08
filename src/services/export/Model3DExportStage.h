#pragma once

#include "ExportTypeStage.h"

namespace EasyKiConverter {

/**
 * @brief 3D模型导出阶段
 *
 * 管理3D模型(Model3D)的并行导出任务。
 * 每个元器件的3D模型导出由Model3DExportWorker执行。
 *
 * 并发配置:
 * - 最大并发数: 2（3D模型文件较大，I/O较慢）
 */
class Model3DExportStage : public ExportTypeStage {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit Model3DExportStage(QObject* parent = nullptr);

protected:
    /**
     * @brief 创建Model3DExportWorker实例
     * @return 新的worker对象
     */
    QObject* createWorker() override;

    /**
     * @brief 启动worker执行3D模型导出
     * @param worker worker实例
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     */
    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override;

private:
    struct ExportOptions m_options;  ///< 导出选项
};

}  // namespace EasyKiConverter
