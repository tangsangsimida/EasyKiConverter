#include "LcscImageService.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QtConcurrent>

namespace EasyKiConverter {

namespace {

QString normalizePreviewImageUrl(const QString& imageUrl) {
    QString normalizedUrl = imageUrl.trimmed();
    if (normalizedUrl.isEmpty()) {
        return QString();
    }

    if (normalizedUrl.startsWith(QStringLiteral("//"))) {
        normalizedUrl.prepend(QStringLiteral("https:"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/image.lceda.cn/")) ||
               normalizedUrl.startsWith(QStringLiteral("/file.elecfans.com/")) ||
               normalizedUrl.startsWith(QStringLiteral("/www.lcsc.com/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/web1/")) ||
               normalizedUrl.startsWith(QStringLiteral("/M00/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://file.elecfans.com/"));
    } else if (normalizedUrl.startsWith('/')) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://image.lceda.cn/"));
    } else if (normalizedUrl.contains("alimg.szlcsc.com")) {
        if (!normalizedUrl.startsWith("http")) {
            normalizedUrl = "https://" + normalizedUrl;
        }
    } else if (!normalizedUrl.contains("://")) {
        normalizedUrl = "https://image.lceda.cn/" + normalizedUrl;
    }

    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("http://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn/image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));

    return normalizedUrl;
}

QStringList deduplicateAndNormalizeUrls(const QStringList& imageUrls) {
    QStringList normalizedUrls;
    for (const QString& imageUrl : imageUrls) {
        const QString normalizedUrl = normalizePreviewImageUrl(imageUrl);
        if (!normalizedUrl.isEmpty() && !normalizedUrls.contains(normalizedUrl)) {
            normalizedUrls.append(normalizedUrl);
        }
    }
    return normalizedUrls;
}

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

    return deduplicateAndNormalizeUrls(imageUrls);
}

}  // namespace

LcscImageService::LcscImageService(QObject* parent)
    : QObject(parent), m_cacheThreadPool(new QThreadPool(this)), m_activeRequests(0), m_isCancelled(0) {
    m_cacheThreadPool->setMaxThreadCount(MAX_CONCURRENT_REQUESTS);
}

void LcscImageService::fetchPreviewImages(const QString& componentId) {
    if (componentId.isEmpty()) {
        return;
    }

    if (m_requestedComponents.contains(componentId) || m_queue.contains(componentId)) {
        qDebug() << "Component" << componentId << "already requested, skipping duplicate request";
        return;
    }

    if (tryLoadCachedPreviewImages(componentId)) {
        m_requestedComponents.insert(componentId);
        return;
    }

    if (m_requestedComponents.contains(componentId)) {
        qDebug() << "Component" << componentId << "already requested, skipping duplicate request";
        return;
    }

    if (m_queue.contains(componentId)) {
        qDebug() << "Component" << componentId << "already in queue, skipping duplicate request";
        return;
    }

    qDebug() << "Adding component" << componentId << "to fetch queue (first time)";
    m_requestedComponents.insert(componentId);
    m_queue.enqueue(componentId);
    processQueue();
}

void LcscImageService::fetchBatchPreviewImages(const QStringList& componentIds) {
    m_isCancelled = 0;

    for (const QString& componentId : componentIds) {
        if (componentId.isEmpty()) {
            continue;
        }

        if (m_requestedComponents.contains(componentId) || m_queue.contains(componentId)) {
            continue;
        }

        if (tryLoadCachedPreviewImages(componentId)) {
            m_requestedComponents.insert(componentId);
            continue;
        }

        if (m_queue.contains(componentId)) {
            continue;
        }

        m_queue.enqueue(componentId);
    }
    processQueue();
}

void LcscImageService::clearCache() {
    qDebug() << "LcscImageService: Clearing all cache data";

    m_requestedComponents.clear();
    m_queue.clear();
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    qDebug() << "LcscImageService: Cache cleared, ready for new requests";
}

void LcscImageService::cancelAll() {
    qDebug() << "LcscImageService: Cancelling all pending preview image fetches";

    m_isCancelled = 1;
    m_requestedComponents.clear();
    m_queue.clear();
    m_downloadCounts.clear();
    m_expectedCounts.clear();
    m_activeRequests = 0;

    for (const auto& request : m_activeAsyncRequests) {
        if (request && !request->isFinished()) {
            request->cancel();
        }
    }
    m_activeAsyncRequests.clear();

    qDebug() << "LcscImageService: All pending preview image fetches cancelled";
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
                        for (auto it = componentImageData->constBegin(); it != componentImageData->constEnd(); ++it) {
                            if (!it.value().isEmpty()) {
                                emit imageReady(componentId, it.value(), it.key());
                            }
                        }

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
    const QStringList normalizedUrls = deduplicateAndNormalizeUrls(imageUrls);
    if (normalizedUrls.isEmpty()) {
        emit error(componentId, "No preview image URLs available");
        return;
    }

    m_expectedCounts[componentId] = normalizedUrls.size();
    m_downloadCounts[componentId] = 0;

    for (int i = 0; i < normalizedUrls.size(); ++i) {
        performDownload(componentId, normalizedUrls[i], i);
    }
}

void LcscImageService::processQueue() {
    while (m_activeRequests < MAX_NETWORK_CONCURRENT_REQUESTS && !m_queue.isEmpty()) {
        const QString componentId = m_queue.dequeue();
        m_activeRequests++;
        performApiSearch(componentId);
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
                                            RetryPolicy::fromProfile(profile));
    trackAsyncRequest(request);

    QObject::connect(
        request, &AsyncNetworkRequest::finished, this, [this, request, componentId](const NetworkResult& result) {
            untrackAsyncRequest(request);

            if (m_isCancelled) {
                request->deleteLater();
                return;
            }

            if (!result.success) {
                qWarning() << "LcscImageService: API search failed for component:" << componentId
                           << "error:" << result.error;
                request->deleteLater();
                performFallback(componentId);
                return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(result.data);
            if (doc.isNull()) {
                qWarning() << "LcscImageService: Failed to parse JSON response for component:" << componentId;
                request->deleteLater();
                performFallback(componentId);
                return;
            }

            const QJsonObject root = doc.object();

            if (root.contains("result")) {
                const QJsonObject resultObject = root["result"].toObject();
                if (resultObject.contains("productList")) {
                    const QJsonArray productList = resultObject["productList"].toArray();
                    if (!productList.isEmpty()) {
                        const QJsonObject product = productList[0].toObject();
                        QStringList imageUrls = extractPreviewImageUrlsFromProduct(product);

                        if (imageUrls.isEmpty()) {
                            for (const QJsonValue& value : productList) {
                                imageUrls = extractPreviewImageUrlsFromProduct(value.toObject());
                                if (!imageUrls.isEmpty()) {
                                    break;
                                }
                            }
                        }

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
                            m_activeRequests = qMax(0, m_activeRequests - 1);
                            startPreviewImageDownloads(componentId, imageUrls);
                            processQueue();
                        } else {
                            m_activeRequests = qMax(0, m_activeRequests - 1);
                            emit error(componentId, "No images available");
                            checkComponentCompletion(componentId);
                            processQueue();
                        }

                        request->deleteLater();
                        return;
                    }
                }
            }

            request->deleteLater();
            performFallback(componentId);
        });
}

void LcscImageService::performFallback(const QString& componentId) {
    if (m_isCancelled) {
        return;
    }

    RequestProfile profile = RequestProfiles::previewImage();
    profile.name = "LcscFallbackSearch";
    profile.connectTimeoutMs = 15000;
    profile.readTimeoutMs = 15000;
    profile.maxRetries = 1;

    AsyncNetworkRequest* request =
        NetworkClient::instance().getAsync(QUrl(QString("https://www.lcsc.com/search?q=%1").arg(componentId)),
                                           ResourceType::PreviewImage,
                                           RetryPolicy::fromProfile(profile));
    trackAsyncRequest(request);

    QObject::connect(
        request, &AsyncNetworkRequest::finished, this, [this, request, componentId](const NetworkResult& result) {
            untrackAsyncRequest(request);

            if (m_isCancelled) {
                request->deleteLater();
                return;
            }

            if (!result.success) {
                m_activeRequests = qMax(0, m_activeRequests - 1);
                emit error(componentId, QString("Fallback search failed: %1").arg(result.error));
                processQueue();
                request->deleteLater();
                return;
            }

            const QString html = QString::fromUtf8(result.data);
            QRegularExpression re("src=\"(https://file\\.elecfans\\.com/web1/M00/[^\"]+\\.jpg)\"");
            QRegularExpressionMatch match = re.match(html);

            if (match.hasMatch()) {
                QStringList imageUrls = {match.captured(1)};
                m_activeRequests = qMax(0, m_activeRequests - 1);
                startPreviewImageDownloads(componentId, imageUrls);
                processQueue();
            } else {
                m_activeRequests = qMax(0, m_activeRequests - 1);
                emit error(componentId, "Image not found");
                processQueue();
            }

            request->deleteLater();
        });
}

void LcscImageService::performDownload(const QString& componentId, const QString& imageUrl, int imageIndex) {
    addRandomDelay([this, componentId, imageUrl, imageIndex]() {
        if (m_isCancelled) {
            return;
        }

        AsyncNetworkRequest* request = NetworkClient::instance().getAsync(
            QUrl(imageUrl), ResourceType::PreviewImage, RetryPolicy::fromProfile(RequestProfiles::previewImage()));
        trackAsyncRequest(request);

        connect(
            request,
            &AsyncNetworkRequest::finished,
            this,
            [this, request, componentId, imageIndex](const NetworkResult& result) {
                untrackAsyncRequest(request);

                if (m_isCancelled) {
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
                // Validate that the downloaded data is actually an image (not HTML error page)
                if (!imageData.isEmpty() && !QString::fromUtf8(imageData).contains("<!DOCTYPE", Qt::CaseInsensitive)) {
                    ComponentCacheService::instance()->savePreviewImage(componentId, imageData, imageIndex);
                    m_downloadCounts[componentId]++;
                    emit imageReady(componentId, imageData, imageIndex);
                    checkDownloadCompletion(componentId);
                } else {
                    qWarning() << "LcscImageService: Downloaded invalid data for" << componentId << "index"
                               << imageIndex << "- may be blocked (403), falling back to API";
                    // Invalid image (403 blocked), try API instead
                    m_downloadCounts[componentId]++;
                    checkDownloadCompletion(componentId);
                    // TODO: Implement fallback to API here - for now just emit error
                    emit error(componentId, "Image blocked (403), please retry");
                }

                request->deleteLater();
            });
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
        checkComponentCompletion(componentId);
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
}

void LcscImageService::checkComponentCompletion(const QString& componentId) {
    m_activeRequests = qMax(0, m_activeRequests - 1);
    processQueue();
}

void LcscImageService::performDatasheetDownload(const QString& componentId, const QString& datasheetUrl) {
    addRandomDelay([this, componentId, datasheetUrl]() {
        if (m_isCancelled) {
            return;
        }

        AsyncNetworkRequest* request = NetworkClient::instance().getAsync(
            QUrl(datasheetUrl), ResourceType::Datasheet, RetryPolicy::fromProfile(RequestProfiles::datasheet()));
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
    });
}

void LcscImageService::addRandomDelay(std::function<void()> callback) {
    int delay = QRandomGenerator::global()->bounded(50, 150);
    QTimer::singleShot(delay, this, [callback]() {
        if (callback) {
            callback();
        }
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
