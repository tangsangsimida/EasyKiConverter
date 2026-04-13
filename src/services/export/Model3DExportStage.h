#pragma once

#include "ExportTypeStage.h"
#include "ExportProgress.h"
#include "TempFileManager.h"

namespace EasyKiConverter {

/**
 * @brief 3D模型导出阶段
 *
 * 管理3D模型(Model3D)的并行导出任务。
 * 每个元器件的3D模型导出由Model3DExportWorker执行。
 *
 * 目录结构:
 *   <outputPath>/
 *   └── 3dmodels/               ← 3D模型目录
 *       ├── component1.wrl
 *       ├── component2.wrl
 *       └── ...
 *
 * 使用TempFileManager确保:
 * - 临时文件存放在.outputPath/.tmp/目录
 * - 导出完成时提交临时文件到最终位置
 * - 取消或失败时回滚所有临时文件
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

    /**
     * @brief 析构函数
     */
    ~Model3DExportStage() override;

    /**
     * @brief 设置导出选项
     * @param options 导出选项配置
     */
    void setOptions(const ExportOptions& options) {
        m_options = options;
    }

    /**
     * @brief 开始3D模型导出
     * @param componentIds 需要导出的元器件ID列表
     * @param cachedData 预加载的元器件数据（key: componentId, value: ComponentData）
     */
    void start(const QStringList& componentIds,
               const QMap<QString, QSharedPointer<ComponentData>>& cachedData) override;

    /**
     * @brief 取消导出
     */
    void cancel() override;

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
    struct TempFilePaths {
        QString wrlTempPath;
        QString wrlFinalPath;
        QString stepTempPath;
        QString stepFinalPath;
    };

    /**
     * @brief 提交单个临时文件到最终位置
     * @param tempPath 临时文件路径
     * @param finalPath 最终文件路径
     */
    bool commitTempFile(const QString& tempPath, const QString& finalPath);

    struct ExportOptions m_options;  ///< 导出选项
    TempFileManager m_tempManager;  ///< 临时文件管理器
    QMap<QString, TempFilePaths> m_componentPaths;  ///< componentId -> temp/final paths
    std::atomic<bool> m_isExporting{false};  ///< 是否正在导出
};

}  // namespace EasyKiConverter
