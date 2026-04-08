#include "LcscImageService.h"

#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QtConcurrent>

namespace EasyKiConverter {

LcscImageService::LcscImageService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_cacheThreadPool(new QThreadPool(this))
    , m_activeRequests(0) {
    // 设置缓存加载线程池的最大线程数
    // 使用较高的并发数以充分利用磁盘 I/O 和缓存加载速度
    m_cacheThreadPool->setMaxThreadCount(MAX_CONCURRENT_REQUESTS);
}

void LcscImageService::fetchPreviewImages(const QString& componentId) {
    if (componentId.isEmpty())
        return;

    // 先检查缓存中是否已有所有预览图，如果有则直接加载缓存，不发起网络请求
    // 注意：即使组件已在 m_requestedComponents 中，也需要检查缓存（支持第二次导出时从缓存加载）
    if (tryLoadCachedPreviewImages(componentId)) {
        // 缓存加载成功，标记为已请求并直接返回
        m_requestedComponents.insert(componentId);
        return;
    }

    // 检查是否已经请求过该组件（防止重复请求）
    if (m_requestedComponents.contains(componentId)) {
        qDebug() << "Component" << componentId << "already requested, skipping duplicate request";
        return;
    }

    // 检查是否已经在队列中，避免重复请求
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
    qDebug() << "LcscImageService: fetchBatchPreviewImages called for" << componentIds.size() << "components";
    qDebug() << "LcscImageService: Current queue size:" << m_queue.size() << "active requests:" << m_activeRequests;
    for (const QString& componentId : componentIds) {
        if (!componentId.isEmpty()) {
            // 先检查缓存，缓存中有则直接加载，不加入队列
            if (tryLoadCachedPreviewImages(componentId)) {
                m_requestedComponents.insert(componentId);
                qDebug() << "LcscImageService: Cached images loaded for" << componentId;
                continue;
            }
            // 缓存没有或加载失败，检查是否已在队列中（防止重复请求）
            if (m_queue.contains(componentId)) {
                qDebug() << "LcscImageService: Component" << componentId << "already in queue, skipping";
                continue;
            }
            // 缓存没有且不在队列中，将组件加入队列等待网络请求
            m_queue.enqueue(componentId);
            qDebug() << "LcscImageService: Queued for network fetch:" << componentId;
        }
    }
    qDebug() << "LcscImageService: Queue size after adding:" << m_queue.size();
    processQueue();
}

void LcscImageService::clearCache() {
    qDebug() << "LcscImageService: Clearing all cache data";

    // 清空所有缓存数据
    m_requestedComponents.clear();

    // 清空队列
    m_queue.clear();

    // 清空下载计数
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    qDebug() << "LcscImageService: Cache cleared, ready for new requests";
}

void LcscImageService::cancelAll() {
    qDebug() << "LcscImageService: Cancelling all pending preview image fetches";

    // 清空已请求组件记录，防止重复请求跳过
    m_requestedComponents.clear();

    // 清空队列，不再处理排队的请求
    m_queue.clear();

    // 清空下载计数
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    // 重置活跃请求计数
    m_activeRequests = 0;

    qDebug() << "LcscImageService: All pending preview image fetches cancelled";
}

bool LcscImageService::tryLoadCachedPreviewImages(const QString& componentId) {
    ComponentCacheService* cache = ComponentCacheService::instance();
    if (!cache) {
        return false;
    }

    // 检查是否有任何缓存的预览图
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

    // 注意：缓存加载不需要设置 m_expectedCounts，因为缓存加载不会增加 m_activeRequests
    // m_expectedCounts 只用于追踪网络请求的并发数

    // 使用 QTimer 将缓存加载调度到下一个事件循环，避免阻塞主线程
    // 这样可以确保 UI 不会冻结
    QTimer::singleShot(0, this, [this, componentId, cache]() { loadCachedPreviewImagesAsync(componentId, cache); });

    return true;
}

void LcscImageService::loadCachedPreviewImagesAsync(const QString& componentId, ComponentCacheService* cache) {
    // 收集所有需要加载的预览图路径
    QList<QPair<int, QString>> pathsToLoad;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        QString path = cache->previewImagePath(componentId, i);
        qDebug() << "LcscImageService: Checking cache path:" << path << "exists:" << QFileInfo::exists(path);
        if (QFileInfo::exists(path)) {
            pathsToLoad.append({i, path});
        }
    }

    if (pathsToLoad.isEmpty()) {
        qDebug() << "LcscImageService: No cached images found for" << componentId;
        checkDownloadCompletion(componentId);
        return;
    }

    qDebug() << "LcscImageService: Found" << pathsToLoad.size() << "cached images for" << componentId;

    // 设置预期的缓存图片数量，用于完成检查
    m_expectedCounts[componentId] = pathsToLoad.size();
    m_downloadCounts[componentId] = 0;

    // 使用专用的缓存线程池实现真正的异步非阻塞加载
    // 改进：收集所有图片数据，一次性发射批量信号，减少 UI 更新次数
    // 为每个组件创建一个临时的图片数据容器
    QMap<int, QByteArray>* componentImageData = new QMap<int, QByteArray>();
    int* loadedCount = new int(0);
    int totalImages = pathsToLoad.size();

    for (const auto& [imageIndex, path] : pathsToLoad) {
        QFuture<QByteArray> future = QtConcurrent::run(m_cacheThreadPool, [path]() {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                return file.readAll();
            }
            return QByteArray();
        });

        QFutureWatcher<QByteArray>* watcher = new QFutureWatcher<QByteArray>(this);
        watcher->setProperty("componentId", componentId);
        watcher->setProperty("imageIndex", imageIndex);
        watcher->setProperty("componentImageData", QVariant::fromValue(componentImageData));
        watcher->setProperty("loadedCount", QVariant::fromValue(loadedCount));
        watcher->setProperty("totalImages", totalImages);

        connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher]() {
            // 获取当前完成的 watcher 的组件信息
            QString compId = watcher->property("componentId").toString();
            int imgIndex = watcher->property("imageIndex").toInt();
            QByteArray imageData = watcher->result();

            QMap<int, QByteArray>* compImageData =
                watcher->property("componentImageData").value<QMap<int, QByteArray>*>();
            int* loadedCountPtr = watcher->property("loadedCount").value<int*>();
            int totalImagesValue = watcher->property("totalImages").toInt();

            if (!imageData.isEmpty() && compImageData) {
                (*compImageData)[imgIndex] = imageData;
            }

            (*loadedCountPtr)++;

            // 检查该组件是否所有图片都已加载完成
            if (*loadedCountPtr >= totalImagesValue) {
                qDebug() << "LcscImageService: All cached images loaded for" << compId
                         << ", emitting batch imageReady signals";

                // 批量发射所有图片的 imageReady 信号
                if (compImageData) {
                    for (auto it = compImageData->constBegin(); it != compImageData->constEnd(); ++it) {
                        if (!it.value().isEmpty()) {
                            emit imageReady(compId, it.value(), it.key());
                        }
                    }
                }

                // 更新计数
                m_downloadCounts[compId] = totalImagesValue;

                // 清理临时对象
                delete compImageData;
                delete loadedCountPtr;

                // 检查下载完成
                checkDownloadCompletion(compId);
            }

            // 清理 watcher
            m_pendingImageWatchers.removeOne(watcher);
            watcher->deleteLater();
        });

        m_pendingImageWatchers.append(watcher);
        watcher->setFuture(future);
    }
}

void LcscImageService::processQueue() {
    // 使用更保守的网络并发策略（5个）来照顾弱网用户
    // 缓存加载不受此限制，可以高并发进行
    while (m_activeRequests < MAX_NETWORK_CONCURRENT_REQUESTS && !m_queue.isEmpty()) {
        QString componentId = m_queue.dequeue();
        m_activeRequests++;
        performApiSearch(componentId, 0);
    }
}

void LcscImageService::performApiSearch(const QString& componentId, int retryCount) {
    qDebug() << "LcscImageService: Starting API search for component:" << componentId << "retry:" << retryCount
             << "active_requests:" << m_activeRequests;

    // 使用官方 API 搜索产品
    QString apiUrl = "https://pro.lceda.cn/api/v2/eda/product/search";

    // 构建表单数据（与 Python 版本一致）
    QByteArray postData;
    QUrlQuery query;
    query.addQueryItem("keyword", componentId);
    query.addQueryItem("currPage", "1");
    query.addQueryItem("pageSize", "50");  // 从 1 改为 50，以获取完整信息
    query.addQueryItem("needAggs", "true");
    postData = query.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest request{QUrl(apiUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded; charset=UTF-8");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/145.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");

    // 使用 QTimer 精确控制超时，而不是使用 setTransferTimeout
    // 这样可以确保计时从请求发出时才开始
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setProperty("componentId", componentId);
    timeoutTimer->setProperty("retryCount", retryCount);

    QNetworkReply* reply = m_networkManager->post(request, postData);

    // 使用 QSharedPointer 确保 QNetworkReply 的安全生命周期
    QSharedPointer<QNetworkReply> replyPtr(reply, [](QNetworkReply* r) {
        if (r) {
            r->deleteLater();
        }
    });

    // 在 reply 和 timeoutTimer 之间建立互相清理的关系
    reply->setProperty("timeoutTimer", QVariant::fromValue(timeoutTimer));
    timeoutTimer->setProperty("reply", QVariant::fromValue(reply));

    // 超时处理
    connect(timeoutTimer, &QTimer::timeout, this, [this, componentId, retryCount, reply, timeoutTimer]() {
        qWarning() << "LcscImageService: API search timeout for component:" << componentId;
        reply->abort();
        reply->deleteLater();
        timeoutTimer->deleteLater();

        // 减少活跃请求数并处理重试
        m_activeRequests--;
        if (retryCount < MAX_RETRY_COUNT) {
            qDebug() << "LcscImageService: Retrying API search for component:" << componentId
                     << "retry:" << (retryCount + 1);
            m_queue.enqueue(componentId);
            processQueue();
        } else {
            qWarning() << "LcscImageService: API search failed after max retries for component:" << componentId;
            emit error(componentId, "Timeout after " + QString::number(MAX_RETRY_COUNT) + " retries");
        }
    });

    // 请求完成时取消超时定时器
    connect(reply, &QNetworkReply::finished, this, [this, replyPtr, componentId, retryCount, timeoutTimer]() {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
        handleApiResponse(replyPtr, componentId, retryCount);
    });

    // 在请求发出后才开始超时计时
    timeoutTimer->start(30000);  // 30 秒超时
}

void LcscImageService::handleApiResponse(QSharedPointer<QNetworkReply> reply,
                                         const QString& componentId,
                                         int retryCount) {
    if (!reply) {
        qWarning() << "Invalid reply pointer in handleApiResponse for component:" << componentId;
        m_activeRequests = qMax(0, m_activeRequests - 1);
        processQueue();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < MAX_RETRY_COUNT) {
            QTimer::singleShot(
                0, this, [this, componentId, retryCount]() { performApiSearch(componentId, retryCount + 1); });
        } else {
            // API 失败，尝试回退到爬虫方式
            performFallback(componentId);
        }
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();

    // 解析 API 响应
    if (root.contains("result")) {
        QJsonObject result = root["result"].toObject();

        if (result.contains("productList")) {
            QJsonArray productList = result["productList"].toArray();

            if (!productList.isEmpty()) {
                QJsonObject product = productList[0].toObject();

                // 提取制造商部件号
                QString manufacturerPart;
                if (product.contains("device_info")) {
                    QJsonObject deviceInfo = product["device_info"].toObject();
                    if (deviceInfo.contains("attributes")) {
                        QJsonObject attributes = deviceInfo["attributes"].toObject();
                        if (attributes.contains("Manufacturer Part")) {
                            manufacturerPart = attributes["Manufacturer Part"].toString();
                            qDebug() << "Extracted manufacturer part from LCSC API for" << componentId << ":"
                                     << manufacturerPart;
                        }
                    }
                }

                // 提取图片列表
                QStringList imageUrls;
                if (product.contains("image")) {
                    QString imagesStr = product["image"].toString();
                    if (!imagesStr.isEmpty()) {
                        // EasyEDA 使用 <$> 分隔多张图片
                        imageUrls = imagesStr.split("<$>", Qt::SkipEmptyParts);
                    }
                }

                qDebug() << "Extracted" << imageUrls.size() << "image URLs for component:" << componentId;

                // 限制最多 3 张图片
                while (imageUrls.size() > MAX_IMAGES_PER_COMPONENT) {
                    imageUrls.removeLast();
                }

                // 提取数据手册 URL
                QString datasheetUrl;
                if (product.contains("device_info")) {
                    QJsonObject deviceInfo = product["device_info"].toObject();
                    if (deviceInfo.contains("attributes")) {
                        QJsonObject attributes = deviceInfo["attributes"].toObject();
                        if (attributes.contains("Datasheet")) {
                            datasheetUrl = attributes["Datasheet"].toString();
                            qDebug() << "Extracted datasheet URL from LCSC API for" << componentId << ":"
                                     << datasheetUrl;
                        }
                    }
                }

                // 发送 LCSC 数据就绪信号（包含制造商部件号、数据手册 URL 和预览图 URL 列表）
                emit lcscDataReady(componentId, manufacturerPart, datasheetUrl, imageUrls);
                qDebug() << "Emitted lcscDataReady for" << componentId;

                // 下载图片
                if (!imageUrls.isEmpty()) {
                    // API 搜索成功，减少活跃请求计数，允许队列继续处理
                    m_activeRequests--;
                    qDebug() << "LcscImageService: API search success for" << componentId
                             << ", active requests decreased to:" << m_activeRequests;

                    m_expectedCounts[componentId] = imageUrls.size();
                    m_downloadCounts[componentId] = 0;

                    qDebug() << "Starting download of" << imageUrls.size() << "images for component:" << componentId;

                    // 开始下载所有图片
                    for (int i = 0; i < imageUrls.size(); ++i) {
                        performDownload(componentId, imageUrls[i], i, 0);
                    }

                    // 继续处理队列
                    processQueue();
                } else {
                    // 没有图片，减少活跃请求计数并发出错误信号
                    m_activeRequests--;
                    qDebug() << "LcscImageService: API search success but no images for" << componentId
                             << ", active requests decreased to:" << m_activeRequests;
                    emit error(componentId, "No images available");
                    checkComponentCompletion(componentId);
                    processQueue();
                }

                return;
            }
        }
    }

    // API 没有找到图片，尝试回退
    performFallback(componentId);
}

void LcscImageService::performFallback(const QString& componentId) {
    QString url = QString("https://www.lcsc.com/search?q=%1").arg(componentId);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/114.0.0.0 Safari/537.36");

    // 使用 QTimer 精确控制超时
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setProperty("componentId", componentId);

    QNetworkReply* reply = m_networkManager->get(request);

    // 在 reply 和 timeoutTimer 之间建立互相清理的关系
    reply->setProperty("timeoutTimer", QVariant::fromValue(timeoutTimer));
    timeoutTimer->setProperty("reply", QVariant::fromValue(reply));

    // 超时处理
    connect(timeoutTimer, &QTimer::timeout, this, [this, componentId, reply, timeoutTimer]() {
        qWarning() << "LcscImageService: Fallback search timeout for component:" << componentId;
        reply->abort();
        reply->deleteLater();
        timeoutTimer->deleteLater();

        m_activeRequests = qMax(0, m_activeRequests - 1);
        emit error(componentId, "Fallback search timeout");
        processQueue();
    });

    // 请求完成时取消超时定时器
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, timeoutTimer]() {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
        handleFallbackResponse(reply, componentId);
    });

    // 在请求发出后才开始超时计时
    timeoutTimer->start(15000);  // 15 秒超时
}

void LcscImageService::handleFallbackResponse(QNetworkReply* reply, const QString& componentId) {
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
        QString html = QString::fromUtf8(reply->readAll());
        // 改进的正则，匹配产品列表中图片的 src
        QRegularExpression re("src=\"(https://file\\.elecfans\\.com/web1/M00/[^\"]+\\.jpg)\"");
        QRegularExpressionMatch match = re.match(html);

        if (match.hasMatch()) {
            QStringList imageUrls = {match.captured(1)};
            // 回退搜索成功，减少活跃请求计数，允许队列继续处理
            m_activeRequests--;
            qDebug() << "LcscImageService: Fallback search success for" << componentId
                     << ", active requests decreased to:" << m_activeRequests;

            m_expectedCounts[componentId] = imageUrls.size();
            m_downloadCounts[componentId] = 0;

            performDownload(componentId, imageUrls[0], 0, 0);
            processQueue();
            return;
        }
    }

    m_activeRequests = qMax(0, m_activeRequests - 1);
    emit error(componentId, "Image not found");
    processQueue();
}

void LcscImageService::performDownload(const QString& componentId,
                                       const QString& imageUrl,
                                       int imageIndex,
                                       int retryCount) {
    qDebug() << "Performing download for component:" << componentId << "image index:" << imageIndex
             << "retry:" << retryCount;

    // 随机延迟（0.05-0.15 秒），异步执行避免阻塞主线程
    addRandomDelay([this, componentId, imageUrl, imageIndex, retryCount]() {
        QNetworkRequest request{QUrl(imageUrl)};
        request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");

        // 使用 QTimer 精确控制超时，而不是使用 setTransferTimeout
        QTimer* timeoutTimer = new QTimer(this);
        timeoutTimer->setSingleShot(true);
        timeoutTimer->setProperty("componentId", componentId);
        timeoutTimer->setProperty("imageIndex", imageIndex);
        timeoutTimer->setProperty("retryCount", retryCount);

        QNetworkReply* reply = m_networkManager->get(request);

        // 在 reply 和 timeoutTimer 之间建立互相清理的关系
        reply->setProperty("timeoutTimer", QVariant::fromValue(timeoutTimer));
        timeoutTimer->setProperty("reply", QVariant::fromValue(reply));

        // 超时处理
        connect(timeoutTimer,
                &QTimer::timeout,
                this,
                [this, componentId, imageUrl, imageIndex, retryCount, reply, timeoutTimer]() {
                    qWarning() << "LcscImageService: Download timeout for component:" << componentId
                               << "image:" << imageIndex;
                    reply->abort();
                    reply->deleteLater();
                    timeoutTimer->deleteLater();

                    // 重试逻辑
                    if (retryCount < MAX_RETRY_COUNT) {
                        qDebug() << "LcscImageService: Retrying download for component:" << componentId
                                 << "image:" << imageIndex << "retry:" << (retryCount + 1);
                        performDownload(componentId, imageUrl, imageIndex, retryCount + 1);
                    } else {
                        qWarning() << "LcscImageService: Download failed after max retries for component:"
                                   << componentId << "image:" << imageIndex;
                        checkDownloadCompletion(componentId);
                    }
                });

        // 请求完成时取消超时定时器
        connect(reply,
                &QNetworkReply::finished,
                this,
                [this, reply, componentId, imageUrl, imageIndex, retryCount, timeoutTimer]() {
                    timeoutTimer->stop();
                    timeoutTimer->deleteLater();
                    handleDownloadResponse(
                        QSharedPointer<QNetworkReply>(reply, [](QNetworkReply* r) { r->deleteLater(); }),
                        componentId,
                        imageUrl,
                        imageIndex,
                        retryCount);
                });
    });
}

void LcscImageService::handleDownloadResponse(QSharedPointer<QNetworkReply> reply,
                                              const QString& componentId,
                                              const QString& imageUrl,
                                              int imageIndex,
                                              int retryCount) {
    if (!reply) {
        qWarning() << "Invalid reply in handleDownloadResponse";
        if (m_expectedCounts.contains(componentId)) {
            m_downloadCounts[componentId]++;
            checkDownloadCompletion(componentId);
        }
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < MAX_RETRY_COUNT) {
            qDebug() << "Image download failed for component:" << componentId << "index:" << imageIndex
                     << "retry:" << retryCount;
            performDownload(componentId, imageUrl, imageIndex, retryCount + 1);
        } else {
            // 下载失败，记录但继续处理其他图片
            qDebug() << "Image download failed after all retries for" << componentId << "index:" << imageIndex;
            m_downloadCounts[componentId]++;
            checkDownloadCompletion(componentId);
        }
        return;
    }

    // 读取数据
    QByteArray imageData = reply->readAll();
    qDebug() << "[LcscImage] Download complete for" << componentId << "index" << imageIndex << "size"
             << imageData.size() << "bytes";

    if (!imageData.isEmpty()) {
        qDebug() << "[LcscImage] Step 1: Saving to disk...";
        // 直接保存到磁盘（用于持久化）
        ComponentCacheService::instance()->savePreviewImage(componentId, imageData, imageIndex);

        qDebug() << "[LcscImage] Step 2: Incrementing download count...";
        // 增加下载计数
        m_downloadCounts[componentId]++;

        qDebug() << "[LcscImage] Step 3: Emitting imageReady signal...";
        // 直接发送信号（使用原始数据，不需要再从磁盘读取）
        emit imageReady(componentId, imageData, imageIndex);

        qDebug() << "[LcscImage] Step 4: Checking download completion...";
        // 检查下载完成状态
        checkDownloadCompletion(componentId);
        qDebug() << "[LcscImage] Step 5: Done for this image";
    } else {
        qDebug() << "Failed to read image data for" << componentId << "index:" << imageIndex;
        m_downloadCounts[componentId]++;
        checkDownloadCompletion(componentId);
    }
}

void LcscImageService::checkDownloadCompletion(const QString& componentId) {
    if (!m_expectedCounts.contains(componentId)) {
        return;
    }

    int expectedCount = m_expectedCounts[componentId];
    int downloadedCount = m_downloadCounts.value(componentId, 0);

    qDebug() << "checkDownloadCompletion for component:" << componentId << "expected:" << expectedCount
             << "downloaded:" << downloadedCount;

    // 只要下载的图片数量达到预期，就认为完成
    if (downloadedCount >= expectedCount) {
        // 所有图片下载完成
        qDebug() << "All images downloaded successfully, emitting allImagesReady";
        emitAllImagesReady(componentId);

        // 检查组件是否所有任务都已完成
        checkComponentCompletion(componentId);
    }
}

void LcscImageService::emitAllImagesReady(const QString& componentId) {
    // 只收集路径，不加载数据到内存
    QStringList imagePaths;
    for (int i = 0; i < MAX_IMAGES_PER_COMPONENT; ++i) {
        QString path = ComponentCacheService::instance()->previewImagePath(componentId, i);
        if (QFileInfo::exists(path)) {
            imagePaths.append(path);
        }
    }

    qDebug() << "All images ready for component:" << componentId << "paths:" << imagePaths.size();

    if (imagePaths.isEmpty()) {
        emit error(componentId, "No images downloaded");
    } else {
        emit allImagesReady(componentId, imagePaths);
    }

    // 清理临时数据
    m_downloadCounts.remove(componentId);
    m_expectedCounts.remove(componentId);
}

void LcscImageService::checkComponentCompletion(const QString& componentId) {
    // 图片下载已经完成检查，这里只需递减计数器
    // 使用 qMax 确保计数器不会变成负数
    qDebug() << "checkComponentCompletion for component:" << componentId << "before decrement:" << m_activeRequests;
    m_activeRequests = qMax(0, m_activeRequests - 1);
    processQueue();
}

void LcscImageService::performDatasheetDownload(const QString& componentId,
                                                const QString& datasheetUrl,
                                                int retryCount) {
    // 随机延迟（0.05-0.15 秒），异步执行避免阻塞主线程
    addRandomDelay([this, componentId, datasheetUrl, retryCount]() {
        QNetworkRequest request{QUrl(datasheetUrl)};
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                          "Chrome/145.0.0.0 Safari/537.36");
        request.setTransferTimeout(45000);

        QNetworkReply* reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, datasheetUrl, retryCount]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                if (retryCount < MAX_RETRY_COUNT) {
                    qDebug() << "Datasheet download failed for" << componentId << "retry:" << retryCount;
                    QTimer::singleShot(0, this, [this, componentId, datasheetUrl, retryCount]() {
                        performDatasheetDownload(componentId, datasheetUrl, retryCount + 1);
                    });
                } else {
                    qDebug() << "Datasheet download failed after all retries for" << componentId;
                    emit error(componentId, QString("Datasheet download failed: %1").arg(reply->errorString()));
                    m_activeRequests--;
                    processQueue();
                }
                return;
            }

            // 读取数据
            QByteArray datasheetData = reply->readAll();
            if (!datasheetData.isEmpty()) {
                // 直接保存到磁盘
                QString format = datasheetUrl.toLower().contains(".html") ? "html" : "pdf";
                ComponentCacheService::instance()->saveDatasheet(componentId, datasheetData, format);

                qDebug() << "Datasheet downloaded for" << componentId << "size:" << datasheetData.size() << "bytes";

                // 从磁盘读取并发送信号
                QByteArray savedData = ComponentCacheService::instance()->loadDatasheet(componentId);
                emit datasheetReady(componentId, savedData);
            } else {
                qDebug() << "Failed to read datasheet data for" << componentId;
                emit error(componentId, "Failed to read datasheet data");
            }

            m_activeRequests--;
            processQueue();
        });

        // 添加网络错误处理
        connect(reply,
                &QNetworkReply::errorOccurred,
                this,
                [this, reply, componentId](QNetworkReply::NetworkError netError) {
                    qWarning() << "Datasheet download network error for component:" << componentId
                               << "error:" << netError;
                    reply->deleteLater();
                    emit error(componentId, QString("Network error: %1").arg(netError));
                    m_activeRequests--;
                    processQueue();
                });
    });
}

void LcscImageService::addRandomDelay(std::function<void()> callback) {
    // 随机延迟 0.05-0.15 秒（异步，不阻塞主线程）
    int delay = QRandomGenerator::global()->bounded(50, 150);
    QTimer::singleShot(delay, this, [callback]() {
        if (callback) {
            callback();
        }
    });
}

}  // namespace EasyKiConverter
