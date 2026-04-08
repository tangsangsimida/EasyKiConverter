#pragma once

#include "ExportTypeStage.h"

namespace EasyKiConverter {

/**
 * @brief 符号导出阶段
 *
 * 管理符号(Symbol)的并行导出任务。
 * 每个元器件的符号导出由SymbolExportWorker执行。
 *
 * 并发配置:
 * - 最大并发数: 3（符号导出是磁盘I/O密集型）
 */
class SymbolExportStage : public ExportTypeStage {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit SymbolExportStage(QObject* parent = nullptr);

    /**
     * @brief 设置导出选项
     * @param options 导出选项配置
     */
    void setOptions(const ExportOptions& options) {
        m_options = options;
    }

protected:
    /**
     * @brief 创建SymbolExportWorker实例
     * @return 新的worker对象
     */
    QObject* createWorker() override;

    /**
     * @brief 启动worker执行符号导出
     * @param worker worker实例
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     */
    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override;

private:
    struct ExportOptions m_options;  ///< 导出选项
};

}  // namespace EasyKiConverter
