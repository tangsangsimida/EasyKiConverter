#ifndef COMPONENTEXPORTTASK_H
#define COMPONENTEXPORTTASK_H

#include "ExportService.h"
#include "models/ComponentData.h"

#include <QObject>
#include <QRunnable>

namespace EasyKiConverter {

/**
 * @brief 元件导出任务
 *
 * 用于QThreadPool 中并行执行元件导出任
 */
class ComponentExportTask : public QObject, public QRunnable {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param componentData 元件数据
     * @param options 导出选项
     * @param symbolExporter 符号导出
     * @param footprintExporter 封装导出
     * @param modelExporter 3D模型导出
     * @param parent 父对象
     */
    explicit ComponentExportTask(const ComponentData& componentData,
                                 const ExportOptions& options,
                                 class ExporterSymbol* symbolExporter,
                                 class ExporterFootprint* footprintExporter,
                                 class Exporter3DModel* modelExporter,
                                 QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ComponentExportTask() override;

    /**
     * @brief 执行导出任务
     */
    void run() override;

signals:
    /**
     * @brief 导出完成信号
     *
     * @param componentId 元件ID
     * @param success 是否成功
     * @param message 消息
     */
    void exportFinished(const QString& componentId, bool success, const QString& message);

private:
    ComponentData m_componentData;
    ExportOptions m_options;
    class ExporterSymbol* m_symbolExporter;
    class ExporterFootprint* m_footprintExporter;
    class Exporter3DModel* m_modelExporter;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTEXPORTTASK_H
