#ifndef MEDIAFETCHWORKER_H
#define MEDIAFETCHWORKER_H

#include "models/ComponentExportStatus.h"

#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QRunnable>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 媒体数据获取工作线程
 *
 * 负责从网络下载预览图和手册数据（I/O密集型任务）
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
     */
    void fetchCompleted(const QString& componentId,
                        const QList<QByteArray>& previewImageDataList,
                        const QByteArray& datasheetData);

private:
    /**
     * @brief 执行HTTP GET请求
     * @param url URL
     * @param timeoutMs 超时时间（毫秒）
     * @return QByteArray 响应数据
     */
    QByteArray httpGet(const QString& url, int timeoutMs = 30000);

private:
    QString m_componentId;
    QStringList m_previewImageUrls;
    QString m_datasheetUrl;
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QMutex m_replyMutex;
    bool m_isAborted;

    static const int PREVIEW_IMAGE_TIMEOUT_MS = 15000;  // 预览图超时
    static const int DATASHEET_TIMEOUT_MS = 30000;  // 手册超时
    static const int MAX_RETRIES = 2;  // 最大重试次数
};

}  // namespace EasyKiConverter

#endif  // MEDIAFETCHWORKER_H
