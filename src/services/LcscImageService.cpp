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

namespace EasyKiConverter {

LcscImageService::LcscImageService(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_activeRequests(0) {}

void LcscImageService::fetchPreviewImages(const QString& componentId) {
    if (componentId.isEmpty())
        return;

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
    for (const QString& componentId : componentIds) {
        if (!componentId.isEmpty()) {
            m_queue.enqueue(componentId);
        }
    }
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

    // 清空队列，不再处理排队的请求
    m_queue.clear();

    // 清空下载计数
    m_downloadCounts.clear();
    m_expectedCounts.clear();

    // 重置活跃请求计数
    m_activeRequests = 0;

    qDebug() << "LcscImageService: All pending preview image fetches cancelled";
}

void LcscImageService::processQueue() {
    while (m_activeRequests < MAX_CONCURRENT_REQUESTS && !m_queue.isEmpty()) {
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
    request.setTransferTimeout(30000);  // 从 15 秒增加到 30 秒

    QNetworkReply* reply = m_networkManager->post(request, postData);

    // 使用 QSharedPointer 确保 QNetworkReply 的安全生命周期
    QSharedPointer<QNetworkReply> replyPtr(reply, [](QNetworkReply* r) {
        if (r) {
            r->deleteLater();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, replyPtr, componentId, retryCount]() {
        handleApiResponse(replyPtr, componentId, retryCount);
    });
}

void LcscImageService::handleApiResponse(QSharedPointer<QNetworkReply> reply,
                                         const QString& componentId,
                                         int retryCount) {
    if (!reply) {
        qWarning() << "Invalid reply pointer in handleApiResponse for component:" << componentId;
        m_activeRequests--;
        processQueue();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < MAX_RETRY_COUNT) {
            QTimer::singleShot(1000 * (retryCount + 1), this, [this, componentId, retryCount]() {
                performApiSearch(componentId, retryCount + 1);
            });
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
                    m_expectedCounts[componentId] = imageUrls.size();
                    m_downloadCounts[componentId] = 0;

                    qDebug() << "Starting download of" << imageUrls.size() << "images for component:" << componentId;

                    // 开始下载所有图片
                    for (int i = 0; i < imageUrls.size(); ++i) {
                        performDownload(componentId, imageUrls[i], i, 0);
                    }
                } else {
                    // 没有图片，直接完成
                    checkComponentCompletion(componentId);
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
    request.setTransferTimeout(15000);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId]() {
        handleFallbackResponse(reply, componentId);
    });
    // 注意：不要单独连接 errorOccurred，因为 finished 信号在错误时也会触发
    // error 处理统一在 handleFallbackResponse 中进行
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
            m_expectedCounts[componentId] = imageUrls.size();
            m_downloadCounts[componentId] = 0;

            performDownload(componentId, imageUrls[0], 0, 0);
            return;
        }
    }

    m_activeRequests--;
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
        request.setTransferTimeout(45000);

        QNetworkReply* reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, imageUrl, imageIndex, retryCount]() {
            handleDownloadResponse(QSharedPointer<QNetworkReply>(reply, [](QNetworkReply* r) { r->deleteLater(); }),
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
    qDebug() << "[LcscImage] Download complete for" << componentId << "index" << imageIndex
             << "size" << imageData.size() << "bytes";

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
    qDebug() << "checkComponentCompletion for component:" << componentId;
    m_activeRequests--;
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
                    QTimer::singleShot(1000 * (retryCount + 1), this, [this, componentId, datasheetUrl, retryCount]() {
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
