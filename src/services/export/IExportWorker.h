#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief 导出Worker接口
 *
 * 所有导出类型的Worker必须实现此接口。
 * Worker在QThreadPool中执行，负责单个元器件的导出任务。
 *
 * 使用示例:
 * @code
 * class SymbolExportWorker : public QObject, public QRunnable, public IExportWorker {
 *     Q_OBJECT
 * public:
 *     void setData(const QString& componentId,
 *                  const QSharedPointer<ComponentData>& data,
 *                  const ExportOptions& options) override {
 *         m_componentId = componentId;
 *         m_data = data;
 *         m_options = options;
 *     }
 *     void cancel() override { m_cancelled = true; }
 *     void run() override {
 *         // 执行导出逻辑
 *         emit completed(m_componentId, true, QString());
 *     }
 * signals:
 *     void completed(const QString& componentId, bool success, const QString& error);
 * };
 * @endcode
 */
class IExportWorker {
public:
    virtual ~IExportWorker() = default;

    /**
     * @brief 设置导出所需的数据
     * @param componentId 元器件ID
     * @param data 预加载的元器件数据
     * @param options 导出选项
     */
    virtual void setData(const QString& componentId,
                         const QSharedPointer<ComponentData>& data,
                         const struct ExportOptions& options) = 0;

    /**
     * @brief 请求取消导出任务
     *
     * 当用户取消导出或某worker失败时调用。
     * Worker应定期检查此标志并尽快退出。
     */
    virtual void cancel() = 0;

    /**
     * @brief 获取关联的元器件ID
     * @return 元器件ID
     */
    virtual QString componentId() const = 0;

    /**
     * @brief 执行导出任务
     *
     * 在线程池中被调用，执行实际的导出逻辑。
     * 完成后必须发射completed信号。
     */
    virtual void run() = 0;
};

}  // namespace EasyKiConverter
