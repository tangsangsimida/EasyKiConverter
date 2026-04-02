#ifndef LCSCIMAGESERVICE_H
#define LCSCIMAGESERVICE_H

#include "ComponentCacheService.h"

#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

namespace EasyKiConverter {

/**
 * @brief LCSC 图片服务类
 *
 * 专门负责抓取 LCSC 元件预览图和数据手册
 * 下载后直接写磁盘，不在内存中缓存
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

    /**
     * @brief 清空所有缓存数据
     *
     * 当清空元器件列表时调用此方法，以便重新添加相同元器件时能够重新请求
     */
    void clearCache();

    /**
     * @brief 取消所有正在进行的预览图获取操作
     *
     * 当用户点击开始导出时调用，以确保取消所有未完成的预览图获取工作
     */
    void cancelAll();

signals:
    /**
     * @brief 单张图片下载成功信号
     *
     * @param componentId 元件 ID
     * @param imageData 图片数据
     * @param imageIndex 图片索引（0-2）
     */
    void imageReady(const QString& componentId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 所有图片下载完成信号
     *
     * @param componentId 元件 ID
     * @param imagePaths 图片路径列表
     */
    void allImagesReady(const QString& componentId, const QStringList& imagePaths);

    /**
     * @brief LCSC API 数据获取成功信号（包含数据手册和预览图 URL）
     *
     * @param componentId 元件 ID
     * @param manufacturerPart 制造商部件号
     * @param datasheetUrl 数据手册 URL
     * @param imageUrls 预览图 URL 列表
     */
    void lcscDataReady(const QString& componentId,
                       const QString& manufacturerPart,
                       const QString& datasheetUrl,
                       const QStringList& imageUrls);

    /**
     * @brief 数据手册下载成功信号
     *
     * @param componentId 元件 ID
     * @param datasheetData 数据手册数据（从磁盘读取）
     */
    void datasheetReady(const QString& componentId, const QByteArray& datasheetData);

    /**
     * @brief 错误信号
     */
    void error(const QString& componentId, const QString& errorMessage);

private slots:
    void processQueue();
    void handleApiResponse(QSharedPointer<QNetworkReply> reply, const QString& componentId, int retryCount);
    void handleFallbackResponse(QNetworkReply* reply, const QString& componentId);
    void handleDownloadResponse(QSharedPointer<QNetworkReply> reply,
                                const QString& componentId,
                                const QString& imageUrl,
                                int imageIndex,
                                int retryCount);

private:
    void performApiSearch(const QString& componentId, int retryCount);
    void performFallback(const QString& componentId);
    void performDownload(const QString& componentId, const QString& imageUrl, int imageIndex, int retryCount);
    void performDatasheetDownload(const QString& componentId, const QString& datasheetUrl, int retryCount);
    void checkDownloadCompletion(const QString& componentId);
    void checkComponentCompletion(const QString& componentId);
    void emitAllImagesReady(const QString& componentId);
    void addRandomDelay(std::function<void()> callback = nullptr);

    QNetworkAccessManager* m_networkManager;
    QQueue<QString> m_queue;
    QSet<QString> m_requestedComponents;  // 已经请求过的组件（防止重复请求）

    // 跟踪下载状态（只存计数，不存实际数据）
    QMap<QString, int> m_downloadCounts;  // componentId -> downloaded count
    QMap<QString, int> m_expectedCounts;  // componentId -> expected count

    int m_activeRequests;  // 当前活跃的下载数
    static const int MAX_CONCURRENT_REQUESTS = 10;  // 最大并发下载数
    static const int MAX_IMAGES_PER_COMPONENT = 3;  // 每个组件最多下载3张预览图
    static const int MAX_RETRY_COUNT = 3;  // 下载失败时的最大重试次数
};

}  // namespace EasyKiConverter

#endif  // LCSCIMAGESERVICE_H
