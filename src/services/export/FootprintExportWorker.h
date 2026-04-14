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
 * @brief 封装导出Worker
 *
 * 在线程池中执行单个元器件的封装导出任务。
 * 负责将元器件的封装数据转换为KiCad封装格式(.kicad_mod)并写入磁盘。
 */
class FootprintExportWorker : public QObject, public QRunnable, public IExportWorker {
    Q_OBJECT

public:
    FootprintExportWorker();

    /**
     * @brief 设置导出所需的数据
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     * @param options 导出选项
     */
    void setData(const QString& componentId,
                 const QSharedPointer<ComponentData>& data,
                 const ExportOptions& options) override;

    /**
     * @brief 执行封装导出任务
     *
     * 在线程池中被调用，执行实际的导出逻辑:
     * 1. 检查封装数据是否可用
     * 2. 创建输出目录
     * 3. 检查是否需要覆盖已有文件
     * 4. 执行导出并写入.kicad_mod文件
     * 5. 发射completed信号
     */
    void run() override;

    /**
     * @brief 请求取消导出任务
     */
    void cancel() override;

    /** @brief 获取元器件ID */
    QString componentId() const override {
        return m_componentId;
    }

    /** @brief 设置导出选项 */
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
    QString m_componentId;  ///< 元器件ID
    QSharedPointer<ComponentData> m_data;  ///< 预加载的元器件数据
    ExportOptions m_options;  ///< 导出选项
    std::atomic<bool> m_cancelled{false};  ///< 取消标志
};

}  // namespace EasyKiConverter
