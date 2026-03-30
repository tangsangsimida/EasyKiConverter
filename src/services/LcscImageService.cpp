#include "LcscImageService.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>

namespace EasyKiConverter {

LcscImageService::LcscImageService(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_activeRequests(0) {
    // 不再使用缓存目录，所有数据保存在内存中
}

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

    // 检查是否正在处理该组件
    if (m_activeRequests > 0 && m_pendingImages.contains(componentId)) {
        qDebug() << "Component" << componentId << "already being processed, skipping duplicate request";
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
    m_manufacturerParts.clear();
    m_pendingDatasheets.clear();
    m_downloadedDatasheets.clear();
    m_pendingImages.clear();
    m_downloadedImages.clear();
    m_downloadCounts.clear();
    m_datasheetDownloadStatus.clear();

    // 清空队列
    m_queue.clear();

    qDebug() << "LcscImageService: Cache cleared, ready for new requests";
}

void LcscImageService::cancelAll() {
    qDebug() << "LcscImageService: Cancelling all pending preview image fetches";

    // 清空队列，不再处理排队的请求
    m_queue.clear();

    // 清空所有待处理的图片下载
    m_pendingImages.clear();
    m_downloadedImages.clear();
    m_downloadCounts.clear();

    // 清空待处理的数据手册下载
    m_pendingDatasheets.clear();
    m_datasheetDownloadStatus.clear();
    m_downloadedDatasheets.clear();

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
    // 避免在回调中访问可能已被删除的对象
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
    // 不再需要手动 deleteLater，QSharedPointer 的删除器会处理

    if (!reply) {
        qWarning() << "Invalid reply pointer in handleApiResponse for component:" << componentId;
        m_activeRequests--;
        processQueue();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < MAX_RETRY_COUNT) {  // 从 2 改为 MAX_RETRY_COUNT (3)
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
                            m_manufacturerParts[componentId] = manufacturerPart;
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
                for (int i = 0; i < imageUrls.size(); ++i) {
                    qDebug() << "  Image" << i << "URL:" << imageUrls[i];
                }

                // 限制最多 3 张图片
                while (imageUrls.size() > MAX_IMAGES_PER_COMPONENT) {
                    imageUrls.removeLast();
                }

                qDebug() << "After limiting to" << MAX_IMAGES_PER_COMPONENT
                         << "images, final count:" << imageUrls.size();

                // 提取数据手册 URL（按照 Python 代码的方式）
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
                qDebug() << "Emitted lcscDataReady for" << componentId << "with manufacturer part:" << manufacturerPart
                         << "images:" << imageUrls.size()
                         << "and datasheet:" << (datasheetUrl.isEmpty() ? "none" : datasheetUrl);

                // 下载图片
                if (!imageUrls.isEmpty()) {
                    m_pendingImages[componentId] = imageUrls;
                    m_downloadedImages[componentId].clear();
                    m_downloadCounts[componentId] = 0;

                    // 预先创建指定数量的空元素，确保索引能够正确对应
                    // 这样当图片按乱序下载时，能够填充到正确的索引位置
                    m_downloadedImages[componentId].resize(imageUrls.size());

                    qDebug() << "Starting download of" << imageUrls.size() << "images for component:" << componentId;
                    qDebug() << "  Pre-allocated container size:" << m_downloadedImages[componentId].size();

                    // 开始下载所有图片
                    for (int i = 0; i < imageUrls.size(); ++i) {
                        qDebug() << "  Scheduling image download" << i << "from:" << imageUrls[i];
                        performDownload(componentId, imageUrls[i], i, 0);
                    }
                }

                // 下载数据手册
                if (!datasheetUrl.isEmpty()) {
                    m_pendingDatasheets[componentId] = datasheetUrl;
                    m_datasheetDownloadStatus[componentId] = 0;  // pending
                    qDebug() << "Starting datasheet download for component:" << componentId << "from:" << datasheetUrl;
                    performDatasheetDownload(componentId, datasheetUrl, 0);
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
    // 添加网络错误处理，确保失败时也能递减计数器，防止队列永久阻塞
    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply, componentId](QNetworkReply::NetworkError) {
        qWarning() << "Fallback request error for component:" << componentId;
        // 递减计数器并处理队列
        m_activeRequests--;
        processQueue();
        reply->deleteLater();
    });
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
            m_pendingImages[componentId] = imageUrls;
            m_downloadedImages[componentId].clear();
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
             << "retry:" << retryCount << "from:" << imageUrl;

    // 随机延迟（0.05-0.15 秒），异步执行避免阻塞主线程
    addRandomDelay([this, componentId, imageUrl, imageIndex, retryCount]() {
        QNetworkRequest request{QUrl(imageUrl)};
        request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
        request.setTransferTimeout(45000);  // 从 20 秒增加到 45 秒

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
    // 不再需要手动 deleteLater，QSharedPointer 的删除器会处理

    if (!reply) {
        qWarning() << "Invalid reply in handleDownloadResponse";
        // 确保计数器递减，防止队列永久阻塞
        if (m_pendingImages.contains(componentId)) {
            m_downloadCounts[componentId]++;
            checkDownloadCompletion(componentId);
        }
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < MAX_RETRY_COUNT) {  // 从 2 改为 MAX_RETRY_COUNT (3)
            qDebug() << "Image download failed for component:" << componentId << "index:" << imageIndex
                     << "error:" << reply->errorString() << "retry:" << retryCount;
            performDownload(componentId, imageUrl, imageIndex, retryCount + 1);
        } else {
            // 下载失败，记录但继续处理其他图片
            qDebug() << "Image download failed after all retries for" << componentId << "index:" << imageIndex
                     << "error:" << reply->errorString();

            // 增加下载计数，确保 checkDownloadCompletion 能正确判断是否所有图片都已尝试下载
            m_downloadCounts[componentId]++;

            // 检查下载完成状态
            checkDownloadCompletion(componentId);
        }
        return;
    }

    // 直接保存到内存中，不写文件
    QByteArray imageData = reply->readAll();
    qDebug() << "Image download response for component:" << componentId << "index:" << imageIndex
             << "size:" << imageData.size() << "bytes" << "URL:" << imageUrl.left(80);

    if (!imageData.isEmpty()) {
        // 使用索引直接赋值，确保图片数据被保存到正确的索引位置
        if (imageIndex >= 0 && imageIndex < m_downloadedImages[componentId].size()) {
            m_downloadedImages[componentId][imageIndex] = imageData;
            qDebug() << "  Saved image data at index:" << imageIndex;
        } else {
            // 如果索引超出范围（异常情况），使用 append
            m_downloadedImages[componentId].append(imageData);
            qDebug() << "  Appended image data (index out of range):" << imageIndex;
        }
        m_downloadCounts[componentId]++;

        // 发送信号，传递图片数据（内存）
        emit imageReady(componentId, imageData, imageIndex);

        // 检查是否所有图片都下载完成
        checkDownloadCompletion(componentId);
    } else {
        qDebug() << "Failed to read image data for" << componentId << "index:" << imageIndex;
        checkDownloadCompletion(componentId);
    }
}

void LcscImageService::checkDownloadCompletion(const QString& componentId) {
    if (!m_pendingImages.contains(componentId)) {
        return;
    }

    int expectedCount = m_pendingImages[componentId].size();
    int downloadedCount = m_downloadCounts.value(componentId, 0);

    qDebug() << "checkDownloadCompletion for component:" << componentId << "expected:" << expectedCount
             << "downloaded:" << downloadedCount;

    // 统计已尝试下载的图片数量（包括成功的和失败的）
    int attemptedCount = 0;
    for (int i = 0; i < m_downloadedImages[componentId].size(); ++i) {
        if (!m_downloadedImages[componentId][i].isEmpty()) {
            attemptedCount++;
        }
    }

    // 只要下载的图片数量达到预期，就认为完成
    if (downloadedCount >= expectedCount) {
        // 所有图片下载完成
        qDebug() << "All images downloaded successfully, emitting allImagesReady";
        emitAllImagesReady(componentId);

        // 检查组件是否所有任务都已完成（图片和数据手册）
        checkComponentCompletion(componentId);
    } else if (attemptedCount == m_downloadedImages[componentId].size()) {
        // 所有位置都已填充（即使有些是空数据），也认为完成
        qDebug() << "All image slots filled (some may be empty), emitting allImagesReady";
        emitAllImagesReady(componentId);

        // 检查组件是否所有任务都已完成（图片和数据手册）
        checkComponentCompletion(componentId);
    }
}

void LcscImageService::emitAllImagesReady(const QString& componentId) {
    QList<QByteArray> imageDataList = m_downloadedImages.value(componentId, QList<QByteArray>());

    qDebug() << "All images ready for component:" << componentId << "count:" << imageDataList.size();

    if (imageDataList.isEmpty()) {
        emit error(componentId, "No images downloaded");
    } else {
        int nonEmptyCount = 0;
        for (int i = 0; i < imageDataList.size(); ++i) {
            qDebug() << "  Image" << i << "size:" << imageDataList[i].size() << "bytes"
                     << (imageDataList[i].isEmpty() ? "(EMPTY)" : "(VALID)");
            if (!imageDataList[i].isEmpty()) {
                nonEmptyCount++;
            }
        }
        qDebug() << "  Total images:" << imageDataList.size() << "Non-empty images:" << nonEmptyCount;
        emit allImagesReady(componentId, imageDataList);
    }

    // 清理临时数据
    m_pendingImages.remove(componentId);
    m_downloadedImages.remove(componentId);
    m_downloadCounts.remove(componentId);
}

void LcscImageService::checkComponentCompletion(const QString& componentId) {
    // 检查组件是否所有任务都已完成
    bool imagesCompleted = !m_pendingImages.contains(componentId);
    bool datasheetCompleted = !m_pendingDatasheets.contains(componentId) ||
                              m_datasheetDownloadStatus.value(componentId, 0) != 0;  // 0=pending, 1=success, 2=failed

    qDebug() << "checkComponentCompletion for component:" << componentId << "images_completed:" << imagesCompleted
             << "datasheet_completed:" << datasheetCompleted;

    if (imagesCompleted && datasheetCompleted) {
        // 所有任务都完成，递减计数器
        qDebug() << "All tasks completed for component:" << componentId << "decrementing active_requests from"
                 << m_activeRequests;
        m_activeRequests--;
        processQueue();
    }
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
        request.setTransferTimeout(45000);  // 45 秒超时

        QNetworkReply* reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, datasheetUrl, retryCount]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                if (retryCount < MAX_RETRY_COUNT) {
                    qDebug() << "Datasheet download failed for" << componentId << "retry:" << retryCount
                             << "error:" << reply->errorString();
                    QTimer::singleShot(1000 * (retryCount + 1), this, [this, componentId, datasheetUrl, retryCount]() {
                        performDatasheetDownload(componentId, datasheetUrl, retryCount + 1);
                    });
                } else {
                    qDebug() << "Datasheet download failed after all retries for" << componentId;
                    m_datasheetDownloadStatus[componentId] = 2;  // failed
                    emit error(componentId, QString("Datasheet download failed: %1").arg(reply->errorString()));

                    // 检查组件是否所有任务都已完成（图片和数据手册）
                    checkComponentCompletion(componentId);
                }
                return;
            }

            // 直接保存到内存中，不写文件
            QByteArray datasheetData = reply->readAll();
            if (!datasheetData.isEmpty()) {
                m_downloadedDatasheets[componentId] = datasheetData;
                m_datasheetDownloadStatus[componentId] = 1;  // success

                qDebug() << "Datasheet downloaded for" << componentId << "size:" << datasheetData.size() << "bytes";
                emit datasheetReady(componentId, datasheetData);

                // 检查组件是否所有任务都已完成（图片和数据手册）
                checkComponentCompletion(componentId);
            } else {
                qDebug() << "Failed to read datasheet data for" << componentId;
                m_datasheetDownloadStatus[componentId] = 2;  // failed
                emit error(componentId, "Failed to read datasheet data");

                // 检查组件是否所有任务都已完成（图片和数据手册）
                checkComponentCompletion(componentId);
            }
        });

        // 添加网络错误处理，确保失败时也能递减计数器
        connect(reply,
                &QNetworkReply::errorOccurred,
                this,
                [this, reply, componentId](QNetworkReply::NetworkError netError) {
                    qWarning() << "Datasheet download network error for component:" << componentId
                               << "error:" << netError;
                    reply->deleteLater();
                    // 标记失败并检查完成状态
                    m_datasheetDownloadStatus[componentId] = 2;
                    emit error(componentId, QString("Network error: %1").arg(netError));
                    checkComponentCompletion(componentId);
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