#include "LcscImageService.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>

namespace EasyKiConverter {

LcscImageService::LcscImageService(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_activeRequests(0) {
    // 设置缓存目录
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/lcsc_images";
    ensureCacheDir();
}

void LcscImageService::fetchPreviewImages(const QString& componentId) {
    if (componentId.isEmpty())
        return;
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

void LcscImageService::processQueue() {
    while (m_activeRequests < MAX_CONCURRENT_REQUESTS && !m_queue.isEmpty()) {
        QString componentId = m_queue.dequeue();
        m_activeRequests++;
        performApiSearch(componentId, 0);
    }
}

void LcscImageService::performApiSearch(const QString& componentId, int retryCount) {
    // 使用官方 API 搜索产品
    QString apiUrl = "https://pro.lceda.cn/api/v2/eda/product/search";

    // 构建表单数据
    QByteArray postData;
    QUrlQuery query;
    query.addQueryItem("keyword", componentId);
    query.addQueryItem("currPage", "1");
    query.addQueryItem("pageSize", "1");
    query.addQueryItem("needAggs", "true");
    postData = query.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest request{QUrl(apiUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded; charset=UTF-8");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/145.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setTransferTimeout(15000);

    QNetworkReply* reply = m_networkManager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, retryCount]() {
        handleApiResponse(reply, componentId, retryCount);
    });
}

void LcscImageService::handleApiResponse(QNetworkReply* reply, const QString& componentId, int retryCount) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < 2) {
            QTimer::singleShot(
                1000 * (retryCount + 1), this, [this, componentId, retryCount]() { performApiSearch(componentId, retryCount + 1); });
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

                // 提取图片列表
                QStringList imageUrls;
                if (product.contains("image")) {
                    QString imagesStr = product["image"].toString();
                    if (!imagesStr.isEmpty()) {
                        // EasyEDA 使用 <$> 分隔多张图片
                        imageUrls = imagesStr.split("<$>", Qt::SkipEmptyParts);
                    }
                }

                // 限制最多 3 张图片
                while (imageUrls.size() > MAX_IMAGES_PER_COMPONENT) {
                    imageUrls.removeLast();
                }

                if (!imageUrls.isEmpty()) {
                    m_pendingImages[componentId] = imageUrls;
                    m_downloadedImages[componentId].clear();
                    m_downloadCounts[componentId] = 0;

                    // 开始下载所有图片
                    for (int i = 0; i < imageUrls.size(); ++i) {
                        performDownload(componentId, imageUrls[i], i, 0);
                    }
                    return;
                }
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

void LcscImageService::performDownload(const QString& componentId, const QString& imageUrl, int imageIndex, int retryCount) {
    QNetworkRequest request{QUrl(imageUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    request.setTransferTimeout(20000);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, imageUrl, imageIndex, retryCount]() {
        handleDownloadResponse(reply, componentId, imageUrl, imageIndex, retryCount);
    });
}

void LcscImageService::handleDownloadResponse(QNetworkReply* reply, const QString& componentId, const QString& imageUrl, int imageIndex, int retryCount) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < 2) {
            performDownload(componentId, imageUrl, imageIndex, retryCount + 1);
        } else {
            // 下载失败，记录但继续处理其他图片
            qDebug() << "Failed to download image for" << componentId << "index:" << imageIndex << "error:" << reply->errorString();
            checkDownloadCompletion(componentId);
        }
        return;
    }

    // 保存到缓存目录
    QString cachePath = getCachePath(componentId, imageIndex);

    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();

        m_downloadedImages[componentId].append(cachePath);
        m_downloadCounts[componentId]++;

        emit imageReady(componentId, cachePath, imageIndex);

        // 检查是否所有图片都下载完成
        checkDownloadCompletion(componentId);
    } else {
        qDebug() << "Failed to save image for" << componentId << "index:" << imageIndex << "error:" << file.errorString();
        checkDownloadCompletion(componentId);
    }
}

void LcscImageService::checkDownloadCompletion(const QString& componentId) {
    if (!m_pendingImages.contains(componentId)) {
        return;
    }

    int expectedCount = m_pendingImages[componentId].size();
    int downloadedCount = m_downloadCounts.value(componentId, 0);

    if (downloadedCount >= expectedCount) {
        // 所有图片下载完成
        emitAllImagesReady(componentId);

        m_activeRequests--;
        processQueue();
    }
}

void LcscImageService::emitAllImagesReady(const QString& componentId) {
    QStringList imagePaths = m_downloadedImages.value(componentId, QStringList());

    if (imagePaths.isEmpty()) {
        emit error(componentId, "No images downloaded");
    } else {
        emit allImagesReady(componentId, imagePaths);
    }

    // 清理临时数据
    m_pendingImages.remove(componentId);
    m_downloadedImages.remove(componentId);
    m_downloadCounts.remove(componentId);
}

QString LcscImageService::getCachePath(const QString& componentId, int imageIndex) {
    QString filename = QString("%1_%2.jpg").arg(componentId).arg(imageIndex);
    return m_cacheDir + "/" + filename;
}

bool LcscImageService::ensureCacheDir() {
    QDir dir(m_cacheDir);
    if (!dir.exists()) {
        return dir.mkpath(m_cacheDir);
    }
    return true;
}

}  // namespace EasyKiConverter