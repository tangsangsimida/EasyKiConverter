#ifndef LCSCIMAGESERVICE_H
#define LCSCIMAGESERVICE_H

#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief LCSC 图片服务类
 *
 * 专门负责抓取 LCSC 元件预览图，使用官方 API 获取多张预览图
 */
class LcscImageService : public QObject {
    Q_OBJECT

public:
    explicit LcscImageService(QObject* parent = nullptr);

    /**
     * @brief 请求获取元件预览图（支持多张）
     *
     * @param componentId 元件 ID
     */
    void fetchPreviewImages(const QString& componentId);

    /**
     * @brief 批量获取多个元件的预览图
     *
     * @param componentIds 元件 ID 列表
     */
    void fetchBatchPreviewImages(const QStringList& componentIds);

signals:
    /**
     * @brief 单张图片下载成功信号
     *
     * @param componentId 元件 ID
     * @param imagePath 本地缓存路径
     * @param imageIndex 图片索引（0-2）
     */
    void imageReady(const QString& componentId, const QString& imagePath, int imageIndex);

    /**
     * @brief 所有图片下载完成信号
     *
     * @param componentId 元件 ID
     * @param imagePaths 本地缓存路径列表
     */
    void allImagesReady(const QString& componentId, const QStringList& imagePaths);

    /**
     * @brief LCSC API 数据获取成功信号（包含数据手册和预览图 URL）
     *
     * @param componentId 元件 ID
     * @param datasheetUrl 数据手册 URL
     * @param imageUrls 预览图 URL 列表
     */
    void lcscDataReady(const QString& componentId, const QString& datasheetUrl, const QStringList& imageUrls);

    /**
     * @brief 错误信号
     */
    void error(const QString& componentId, const QString& errorMessage);

private slots:
    void processQueue();
    void handleApiResponse(QNetworkReply* reply, const QString& componentId, int retryCount);
    void handleFallbackResponse(QNetworkReply* reply, const QString& componentId);
    void handleDownloadResponse(QNetworkReply* reply,
                                const QString& componentId,
                                const QString& imageUrl,
                                int imageIndex,
                                int retryCount);

private:
    void performApiSearch(const QString& componentId, int retryCount);
    void performFallback(const QString& componentId);
    void performDownload(const QString& componentId, const QString& imageUrl, int imageIndex, int retryCount);
    void checkDownloadCompletion(const QString& componentId);
    void emitAllImagesReady(const QString& componentId);
    QString getCachePath(const QString& componentId, int imageIndex);
    bool ensureCacheDir();

    QNetworkAccessManager* m_networkManager;
    QQueue<QString> m_queue;
    QMap<QString, QStringList> m_pendingImages;     // componentId -> imageUrls
    QMap<QString, QStringList> m_downloadedImages;  // componentId -> localPaths
    QMap<QString, int> m_downloadCounts;            // componentId -> downloaded count
    int m_activeRequests;
    static const int MAX_CONCURRENT_REQUESTS = 5;
    static const int MAX_IMAGES_PER_COMPONENT = 3;
    QString m_cacheDir;
};

}  // namespace EasyKiConverter

#endif  // LCSCIMAGESERVICE_H
