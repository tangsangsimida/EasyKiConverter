#pragma once

#include "ExportProgress.h"
#include "IExportWorker.h"

#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>

#include <atomic>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief 符号导出Worker
 *
 * 在线程池中执行单个元器件的符号导出任务。
 * 负责将元器件的符号数据转换为KiCad符号格式(.kicad_sym)并写入磁盘。
 *
 * 使用QThreadPool时，Worker通过QRunnable接口执行。
 */
class SymbolExportWorker : public QObject, public QRunnable, public IExportWorker {
    Q_OBJECT

public:
    SymbolExportWorker();

    void setData(const QString& componentId,
                 const QSharedPointer<ComponentData>& data,
                 const ExportOptions& options) override;
    void run() override;
    void cancel() override;

    QString componentId() const override {
        return m_componentId;
    }

    /** @brief 设置导出选项（用于配置输出路径等） */
    void setOptions(const ExportOptions& options);

signals:
    /**
     * @brief 导出完成信号
     * @param componentId 元器件ID
     * @param success 是否成功
     * @param error 错误信息（成功时为空）
     */
    void completed(const QString& componentId, bool success, const QString& error);

private:
    QString m_componentId;
    QSharedPointer<ComponentData> m_data;
    ExportOptions m_options;
    std::atomic<bool> m_cancelled{false};
};

}  // namespace EasyKiConverter
