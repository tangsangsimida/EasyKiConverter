#ifndef LCSCIMAGESERVICE_H
#define LCSCIMAGESERVICE_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QString>


namespace EasyKiConverter {

/**
 * @brief LCSC 图片服务类
 *
 * 专门负责抓取 LCSC 元件预览图，包含爬虫和重试逻辑
 */
class LcscImageService : public QObject {
    Q_OBJECT

public:
    explicit LcscImageService(QObject* parent = nullptr);

    /**
     * @brief 请求获取元件预览图
     *
     * @param componentId 元件 ID
     */
    void fetchPreviewImage(const QString& componentId);

signals:
    /**
     * @brief 图片下载成功信号
     *
     * @param componentId 元件 ID
     * @param imagePath 本地缓存路径
     */
    void imageReady(const QString& componentId, const QString& imagePath);

    /**
     * @brief 错误信号
     */
    void error(const QString& componentId, const QString& errorMessage);

private slots:
    void processQueue();
    void handleSearchResponse(QNetworkReply* reply, const QString& componentId, int retryCount);
    void handleFallbackResponse(QNetworkReply* reply, const QString& componentId);
    void handleDownloadResponse(QNetworkReply* reply, const QString& componentId, int retryCount);

private:
    void performSearch(const QString& componentId, int retryCount);
    void performFallback(const QString& componentId);
    void performDownload(const QString& componentId, const QString& imageUrl, int retryCount);

    QNetworkAccessManager* m_networkManager;
    QQueue<QString> m_queue;
    int m_activeRequests;
    static const int MAX_CONCURRENT_REQUESTS = 5;
};

}  // namespace EasyKiConverter

#endif  // LCSCIMAGESERVICE_H
