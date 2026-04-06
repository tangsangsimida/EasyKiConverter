#ifndef MEDIAFETCHWORKER_H
#define MEDIAFETCHWORKER_H

#include "models/ComponentExportStatus.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QRunnable>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 媒体数据获取工作线程
 *
 * 负责从缓存或网络下载预览图和手册数据（I/O密集型任务）
 * 优先使用缓存，缓存没有则下载并自动缓存
 */
class MediaFetchWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param componentId 元件ID
     * @param previewImageUrls 预览图URL列表
     * @param datasheetUrl 手册URL
     * @param parent 父对象
     */
    explicit MediaFetchWorker(const QString& componentId,
                              const QStringList& previewImageUrls,
                              const QString& datasheetUrl,
                              QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MediaFetchWorker() override;

    /**
     * @brief 执行获取任务
     */
    void run() override;

    /**
     * @brief 中断当前正在执行的网络请求
     */
    void abort();

signals:
    /**
     * @brief 获取完成信号
     * @param componentId 元件ID
     * @param previewImageDataList 预览图数据列表
     * @param datasheetData 手册数据
     * @param diagnostics 网络诊断信息列表
     */
    void fetchCompleted(const QString& componentId,
                        const QList<QByteArray>& previewImageDataList,
                        const QByteArray& datasheetData,
                        const QList<ComponentExportStatus::NetworkDiagnostics>& diagnostics);

private:
    QString m_componentId;
    QStringList m_previewImageUrls;
    QString m_datasheetUrl;
    bool m_isAborted;
    QList<ComponentExportStatus::NetworkDiagnostics> m_diagnostics;
};

}  // namespace EasyKiConverter

#endif  // MEDIAFETCHWORKER_H
