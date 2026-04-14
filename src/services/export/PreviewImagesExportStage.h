#pragma once

#include "ExportProgress.h"
#include "ExportTypeStage.h"
#include "TempFileManager.h"

namespace EasyKiConverter {

/**
 * @brief 预览图导出阶段
 *
 * 管理预览图(Preview Images)的并行导出任务。
 * 每个元器件的预览图导出由PreviewImagesExportWorker执行。
 *
 * 目录结构:
 *   <outputPath>/
 *   └── previews/               ← 预览图目录
 *       ├── component1_preview_1.png
 *       ├── component1_preview_2.png
 *       └── ...
 *
 * 使用TempFileManager确保:
 * - 临时文件存放在outputPath/.tmp/目录
 * - 导出完成时提交临时文件到最终位置
 * - 取消或失败时回滚所有临时文件
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

    /**
     * @brief 析构函数
     */
    ~PreviewImagesExportStage() override;

    /**
     * @brief 设置导出选项
     * @param options 导出选项配置
     */
    void setOptions(const ExportOptions& options) {
        m_options = options;
    }

    /**
     * @brief 开始预览图导出
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
    /**
     * @brief 为指定组件创建临时路径映射
     * @param componentId 元器件ID
     * @param previewCount 预览图数量
     * @return 文件名到临时路径的映射
     */
    QMap<QString, QString> createTempPathsForComponent(const QString& componentId, int previewCount);

    struct ExportOptions m_options;  ///< 导出选项
    TempFileManager m_tempManager;  ///< 临时文件管理器
    QMap<QString, QMap<QString, QString>> m_componentTempPaths;  ///< componentId -> (filename -> tempPath)
    QMap<QString, QString> m_outputDirs;  ///< componentId -> outputDir
    std::atomic<bool> m_isExporting{false};  ///< 是否正在导出
};

}  // namespace EasyKiConverter
