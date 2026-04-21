#include "LcscImageService.h"

#include "ConfigService.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QTimer>
#include <QUrlQuery>
#include <QtConcurrent>

namespace EasyKiConverter {

namespace {

QStringList extractPreviewImageUrlsFromProduct(const QJsonObject& product) {
    QStringList imageUrls;
    if (product.contains(QStringLiteral("image"))) {
        const QString imagesStr = product.value(QStringLiteral("image")).toString();
        if (!imagesStr.isEmpty()) {
            imageUrls = imagesStr.split(QStringLiteral("<$>"), Qt::SkipEmptyParts);
        }
    }

    while (imageUrls.size() > 3) {
        imageUrls.removeLast();
    }

    return UrlUtils::deduplicateAndNormalizeUrls(imageUrls);
}

QString extractComponentCode(const QJsonObject& product) {
    static const QStringList directKeys = {QStringLiteral("component_code"),
                                           QStringLiteral("productCode"),
                                           QStringLiteral("product_code"),
                                           QStringLiteral("productNo"),
                                           QStringLiteral("product_no"),
                                           QStringLiteral("number"),
                                           QStringLiteral("code"),
                                           QStringLiteral("lcscPart"),
                                           QStringLiteral("lcscPartNumber")};

    for (const QString& key : directKeys) {
        const QString value = product.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value.toUpper();
        }
    }

    const QJsonObject deviceInfo = product.value(QStringLiteral("device_info")).toObject();
    if (!deviceInfo.isEmpty()) {
        for (const QString& key : directKeys) {
            const QString value = deviceInfo.value(key).toString().trimmed();
            if (!value.isEmpty()) {
                return value.toUpper();
            }
        }

        const QJsonObject attributes = deviceInfo.value(QStringLiteral("attributes")).toObject();
        static const QStringList attributeKeys = {QStringLiteral("LCSC Part"),
                                                  QStringLiteral("LCSC Part #"),
                                                  QStringLiteral("LCSC Part Number"),
                                                  QStringLiteral("Part Number")};
        for (const QString& key : attributeKeys) {
            const QString value = attributes.value(key).toString().trimmed();
            if (!value.isEmpty()) {
                return value.toUpper();
            }
        }
    }

    return QString();
}

QJsonObject selectBestProductForComponent(const QString& componentId, const QJsonArray& productList) {
    const QString normalizedId = componentId.trimmed().toUpper();
    for (const QJsonValue& value : productList) {
        const QJsonObject product = value.toObject();
        if (extractComponentCode(product) == normalizedId) {
            return product;
        }
    }

    // 不再 fallback 到第一个产品，如果没有精确匹配则返回空对象
    // 原因：之前的 fallback 逻辑会导致显示错误元器件的预览图，造成用户混淆
    return QJsonObject();
}

bool isWeakNetworkEnabled() {
    return ConfigService::instance()->getWeakNetworkSupport();
}

}  // namespace

LcscImageService::LcscImageService(QObject* parent)
    : QObject(parent), m_cacheThreadPool(new QThreadPool(this)), m_isCancelled(0) {
    m_cacheThreadPool->setMaxThreadCount(MAX_CONCURRENT_REQUESTS);
}

void LcscImageService::fetchPreviewImages(const QString& componentId) {
    if (componentId.isEmpty()) {
        return;
    }

    if (m_requestedComponents.contains(componentId)) {
        qDebug() << "Component" << componentId << "already requested, skipping duplicate request";
        return;
    }

    if (tryLoadCachedPreviewImages(componentId)) {
        m_requestedComponents.insert(componentId);
        return;
    }

    m_requestedComponents.insert(componentId);
    performApiSearch(componentId);
}

void LcscImageService::fetchBatchPreviewImages(const QStringList& componentIds) {
    m_isCancelled = 0;

    for (const QString& componentId : componentIds) {
        if (componentId.isEmpty()) {
            continue;
        }

        if (m_requestedComponents.contains(componentId)) {
            continue;
        }

        if (tryLoadCachedPreviewImages(componentId)) {
            m_requestedComponents.insert(componentId);
            continue;
        }

        m_requestedComponents.insert(componentId);
        performApiSearch(componentId);
    }
}

void LcscImageService::clearCache() {
    qDebug() << "LcscImageService: Clearing all cache data";

    m_requestedComponents.clear();
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    qDebug() << "LcscImageService: Cache cleared, ready for new requests";
}

void LcscImageService::cancelAll() {
    qDebug() << "LcscImageService: Cancelling all pending preview image fetches";

    m_isCancelled = 1;
    m_requestedComponents.clear();
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    for (const auto& request : m_activeAsyncRequests) {
        if (request && !request->isFinished()) {
            request->cancel();
        }
    }
    m_activeAsyncRequests.clear();

    qDebug() << "LcscImageService: All pending preview image fetches cancelled";
}

void LcscImageService::cancelRequestForComponent(const QString& componentId) {
    qDebug() << "LcscImageService: Cancelling request for component" << componentId;

    // 从请求组件列表中移除
    m_requestedComponents.remove(componentId);
    m_downloadCounts.remove(componentId);
    m_expectedCounts.remove(componentId);

    // 取消与该组件相关的异步请求
    // 注意：由于AsyncNetworkRequest没有暴露componentId信息，我们无法精确取消特定组件的请求
    // 但通过从m_requestedComponents中移除，响应到达时会被忽略

    qDebug() << "LcscImageService: Request cancelled for component" << componentId;
}

void LcscImageService::fetchDatasheet(const QString& componentId, const QString& datasheetUrl) {
    if (componentId.isEmpty() || datasheetUrl.isEmpty()) {
        qWarning() << "LcscImageService::fetchDatasheet called with empty componentId or datasheetUrl";
        return;
    }

    m_isCancelled = 0;

    qDebug() << "LcscImageService: fetchDatasheet called for" << componentId;
    performDatasheetDownload(componentId, datasheetUrl);
}

bool LcscImageService::tryLoadCachedPreviewImages(const QString& componentId) {
    ComponentCacheService* cache = ComponentCacheService::instance();
    if (!cache) {
        return false;
    }

    int cachedCount = 0;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        QString path = cache->previewImagePath(componentId, i);
        if (QFileInfo::exists(path)) {
            cachedCount++;
        }
    }

    if (cachedCount == 0) {
        qDebug() << "LcscImageService: No cached preview images for" << componentId;
        return false;
    }

    qDebug() << "LcscImageService: Found" << cachedCount << "cached preview images for" << componentId
             << ", scheduling async cache load...";

    QTimer::singleShot(0, this, [this, componentId, cache]() { loadCachedPreviewImagesAsync(componentId, cache); });
    return true;
}

void LcscImageService::loadCachedPreviewImagesAsync(const QString& componentId, ComponentCacheService* cache) {
    if (m_isCancelled) {
        return;
    }

    QList<QPair<int, QString>> pathsToLoad;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        QString path = cache->previewImagePath(componentId, i);
        if (QFileInfo::exists(path)) {
            pathsToLoad.append({i, path});
        }
    }

    if (pathsToLoad.isEmpty()) {
        qDebug() << "LcscImageService: No cached images found for" << componentId;
        m_requestedComponents.remove(componentId);
        checkDownloadCompletion(componentId);
        return;
    }

    m_expectedCounts[componentId] = pathsToLoad.size();
    m_downloadCounts[componentId] = 0;

    auto componentImageData = QSharedPointer<QMap<int, QByteArray>>::create();
    auto loadedCount = QSharedPointer<int>::create(0);
    const int totalImages = pathsToLoad.size();

    for (const auto& [imageIndex, path] : pathsToLoad) {
        QFuture<QByteArray> future = QtConcurrent::run(m_cacheThreadPool, [path]() {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                return file.readAll();
            }
            return QByteArray();
        });

        auto* watcher = new QFutureWatcher<QByteArray>(this);
        connect(watcher,
                &QFutureWatcher<QByteArray>::finished,
                this,
                [this, watcher, componentId, imageIndex, componentImageData, loadedCount, totalImages]() {
                    QByteArray imageData = watcher->result();
                    if (!imageData.isEmpty()) {
                        (*componentImageData)[imageIndex] = imageData;
                    }

                    ++(*loadedCount);
                    if (*loadedCount >= totalImages) {
                        m_downloadCounts[componentId] = totalImages;
                        checkDownloadCompletion(componentId);
                    }

                    m_pendingImageWatchers.removeOne(watcher);
                    watcher->deleteLater();
                });

        m_pendingImageWatchers.append(watcher);
        watcher->setFuture(future);
    }
}

void LcscImageService::startPreviewImageDownloads(const QString& componentId, const QStringList& imageUrls) {
    const QStringList normalizedUrls = UrlUtils::deduplicateAndNormalizeUrls(imageUrls);
    if (normalizedUrls.isEmpty()) {
        m_requestedComponents.remove(componentId);
        emit error(componentId, "No preview image URLs available");
        return;
    }

    m_expectedCounts[componentId] = normalizedUrls.size();
    m_downloadCounts[componentId] = 0;

    for (int i = 0; i < normalizedUrls.size(); ++i) {
        performDownload(componentId, normalizedUrls[i], i);
    }
}

void LcscImageService::performApiSearch(const QString& componentId) {
    if (m_isCancelled) {
        return;
    }

    QByteArray postData;
    QUrlQuery query;
    query.addQueryItem("keyword", componentId);
    query.addQueryItem("currPage", "1");
    query.addQueryItem("pageSize", "50");
    query.addQueryItem("needAggs", "true");
    postData = query.toString(QUrl::FullyEncoded).toUtf8();

    RequestProfile profile = RequestProfiles::productSearch();
    profile.connectTimeoutMs = 30000;
    profile.readTimeoutMs = 30000;
    profile.maxRetries = MAX_RETRY_COUNT;

    AsyncNetworkRequest* request =
        NetworkClient::instance().postAsync(QUrl("https://pro.lceda.cn/api/v2/eda/product/search"),
                                            postData,
                                            ResourceType::ProductSearch,
                                            RetryPolicy::fromProfile(profile, isWeakNetworkEnabled()));
    trackAsyncRequest(request);

    QObject::connect(
        request, &AsyncNetworkRequest::finished, this, [this, request, componentId](const NetworkResult& result) {
            untrackAsyncRequest(request);

            if (m_isCancelled) {
                request->deleteLater();
                return;
            }

            // 检查组件是否已被取消
            if (!m_requestedComponents.contains(componentId)) {
                request->deleteLater();
                return;
            }

            if (!result.success) {
                qWarning() << "LcscImageService: API search failed for component:" << componentId
                           << "error:" << result.error;
                request->deleteLater();
                m_requestedComponents.remove(componentId);
                // 不再触发fallback，直接报告无预览图
                emit error(componentId, "No preview image available from API");
                return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(result.data);
            if (doc.isNull()) {
                qWarning() << "LcscImageService: Failed to parse JSON response for component:" << componentId;
                request->deleteLater();
                m_requestedComponents.remove(componentId);
                // 不再触发fallback，直接报告无预览图
                emit error(componentId, "No preview image available from API");
                return;
            }

            const QJsonObject root = doc.object();

            if (root.contains("result")) {
                const QJsonObject resultObject = root["result"].toObject();
                if (resultObject.contains("productList")) {
                    const QJsonArray productList = resultObject["productList"].toArray();
                    if (!productList.isEmpty()) {
                        const QJsonObject product = selectBestProductForComponent(componentId, productList);
                        const QString matchedComponentCode = extractComponentCode(product);

                        // 如果没有找到匹配的元器件，直接报告错误，不再使用 fallback
                        if (matchedComponentCode.isEmpty()) {
                            qWarning() << "LcscImageService: No matching product found for component:" << componentId;
                            request->deleteLater();
                            m_requestedComponents.remove(componentId);
                            emit error(componentId, "No preview image available from API");
                            return;
                        }

                        QStringList imageUrls = extractPreviewImageUrlsFromProduct(product);

                        QString manufacturerPart;
                        QString datasheetUrl;
                        if (product.contains("device_info")) {
                            const QJsonObject deviceInfo = product["device_info"].toObject();
                            if (deviceInfo.contains("attributes")) {
                                const QJsonObject attributes = deviceInfo["attributes"].toObject();
                                manufacturerPart = attributes.value("Manufacturer Part").toString();
                                datasheetUrl = attributes.value("Datasheet").toString();
                            }
                        }

                        while (imageUrls.size() > MAX_IMAGES_PER_COMPONENT) {
                            imageUrls.removeLast();
                        }

                        emit lcscDataReady(componentId, manufacturerPart, datasheetUrl, imageUrls);

                        if (!imageUrls.isEmpty()) {
                            startPreviewImageDownloads(componentId, imageUrls);
                        } else {
                            emit error(componentId, "No images available");
                        }

                        request->deleteLater();
                        return;
                    }
                }
            }

            request->deleteLater();
            m_requestedComponents.remove(componentId);
            // 不再触发fallback，直接报告无预览图
            emit error(componentId, "No preview image available from API");
        });
}

void LcscImageService::performDownload(const QString& componentId, const QString& imageUrl, int imageIndex) {
    if (m_isCancelled) {
        return;
    }

    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(
        QUrl(imageUrl),
        ResourceType::PreviewImage,
        RetryPolicy::fromProfile(RequestProfiles::previewImage(), isWeakNetworkEnabled()));
    trackAsyncRequest(request);

    connect(request,
            &AsyncNetworkRequest::finished,
            this,
            [this, request, componentId, imageIndex](const NetworkResult& result) {
                untrackAsyncRequest(request);

                if (m_isCancelled) {
                    request->deleteLater();
                    return;
                }

                // 检查组件是否已被取消
                if (!m_requestedComponents.contains(componentId)) {
                    request->deleteLater();
                    return;
                }

                if (!result.success) {
                    m_downloadCounts[componentId]++;
                    checkDownloadCompletion(componentId);
                    request->deleteLater();
                    return;
                }

                const QByteArray imageData = result.data;
                if (!imageData.isEmpty() && !QString::fromUtf8(imageData).contains("<!DOCTYPE", Qt::CaseInsensitive)) {
                    ComponentCacheService::instance()->savePreviewImage(componentId, imageData, imageIndex);
                    m_downloadCounts[componentId]++;
                    emit imageReady(componentId, imageData, imageIndex);
                    checkDownloadCompletion(componentId);
                } else {
                    qWarning() << "LcscImageService: Downloaded invalid data for" << componentId << "index"
                               << imageIndex << "- may be blocked (403), falling back to API";
                    m_downloadCounts[componentId]++;
                    checkDownloadCompletion(componentId);
                    emit error(componentId, "Image blocked (403), please retry");
                }

                request->deleteLater();
            });
}

void LcscImageService::checkDownloadCompletion(const QString& componentId) {
    if (!m_expectedCounts.contains(componentId)) {
        return;
    }

    int expectedCount = m_expectedCounts[componentId];
    int downloadedCount = m_downloadCounts.value(componentId, 0);

    if (downloadedCount >= expectedCount) {
        emitAllImagesReady(componentId);
    }
}

void LcscImageService::emitAllImagesReady(const QString& componentId) {
    QStringList imagePaths;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        QString path = ComponentCacheService::instance()->previewImagePath(componentId, i);
        if (QFileInfo::exists(path)) {
            imagePaths.append(path);
        }
    }

    if (imagePaths.isEmpty()) {
        emit error(componentId, "No images downloaded");
    } else {
        emit allImagesReady(componentId, imagePaths);
    }

    m_downloadCounts.remove(componentId);
    m_expectedCounts.remove(componentId);
    m_requestedComponents.remove(componentId);
}

void LcscImageService::performDatasheetDownload(const QString& componentId, const QString& datasheetUrl) {
    if (m_isCancelled) {
        return;
    }

    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(
        QUrl(datasheetUrl),
        ResourceType::Datasheet,
        RetryPolicy::fromProfile(RequestProfiles::datasheet(), isWeakNetworkEnabled()));
    trackAsyncRequest(request);

    connect(request,
            &AsyncNetworkRequest::finished,
            this,
            [this, request, componentId, datasheetUrl](const NetworkResult& result) {
                untrackAsyncRequest(request);

                if (m_isCancelled) {
                    request->deleteLater();
                    return;
                }

                if (!result.success) {
                    emit error(componentId, QString("Datasheet download failed: %1").arg(result.error));
                    request->deleteLater();
                    return;
                }

                const QByteArray datasheetData = result.data;
                if (!datasheetData.isEmpty()) {
                    QString format = datasheetUrl.toLower().contains(".html") ? "html" : "pdf";
                    ComponentCacheService::instance()->saveDatasheet(componentId, datasheetData, format);
                    QByteArray savedData = ComponentCacheService::instance()->loadDatasheet(componentId);
                    emit datasheetReady(componentId, savedData);
                } else {
                    emit error(componentId, "Failed to read datasheet data");
                }

                request->deleteLater();
            });
}

void LcscImageService::trackAsyncRequest(AsyncNetworkRequest* request) {
    if (!request) {
        return;
    }
    m_activeAsyncRequests.append(QPointer<AsyncNetworkRequest>(request));
}

void LcscImageService::untrackAsyncRequest(AsyncNetworkRequest* request) {
    m_activeAsyncRequests.removeOne(QPointer<AsyncNetworkRequest>(request));
}

}  // namespace EasyKiConverter
