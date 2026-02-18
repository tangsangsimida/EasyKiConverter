#include "LcscImageService.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace EasyKiConverter {

LcscImageService::LcscImageService(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_activeRequests(0) {}

void LcscImageService::fetchPreviewImage(const QString& componentId) {
    if (componentId.isEmpty())
        return;
    m_queue.enqueue(componentId);
    processQueue();
}

void LcscImageService::processQueue() {
    while (m_activeRequests < MAX_CONCURRENT_REQUESTS && !m_queue.isEmpty()) {
        QString componentId = m_queue.dequeue();
        m_activeRequests++;
        performSearch(componentId, 0);
    }
}

void LcscImageService::performSearch(const QString& componentId, int retryCount) {
    QString url =
        QString("https://overseas.szlcsc.com/overseas/global/search?keyword=%1&pageNumber=1&noShowSelf=false&from=%1")
            .arg(componentId);

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/114.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, retryCount]() {
        handleSearchResponse(reply, componentId, retryCount);
    });
}

void LcscImageService::handleSearchResponse(QNetworkReply* reply, const QString& componentId, int retryCount) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < 2) {
            QTimer::singleShot(
                1000, this, [this, componentId, retryCount]() { performSearch(componentId, retryCount + 1); });
        } else {
            performFallback(componentId);
        }
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QString imageUrl;
    bool found = false;

    if (root.contains("result")) {
        QJsonArray resultArray = root["result"].toArray();
        for (const auto& resVal : resultArray) {
            QJsonObject resObj = resVal.toObject();
            if (resObj.contains("products")) {
                QJsonArray products = resObj["products"].toArray();
                if (!products.isEmpty()) {
                    imageUrl = products[0].toObject()["selfBreviaryImageUrl"].toString();
                    if (!imageUrl.isEmpty()) {
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (found) {
        performDownload(componentId, imageUrl, 0);
    } else {
        performFallback(componentId);
    }
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
            performDownload(componentId, match.captured(1), 0);
            return;
        }
    }

    m_activeRequests--;
    emit error(componentId, "Image not found");
    processQueue();
}

void LcscImageService::performDownload(const QString& componentId, const QString& imageUrl, int retryCount) {
    QNetworkRequest request{QUrl(imageUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    request.setTransferTimeout(20000);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, retryCount, imageUrl]() {
        handleDownloadResponse(reply, componentId, retryCount);
    });
}

void LcscImageService::handleDownloadResponse(QNetworkReply* reply, const QString& componentId, int retryCount) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (retryCount < 2) {
            performDownload(componentId, reply->url().toString(), retryCount + 1);
        } else {
            m_activeRequests--;
            emit error(componentId, "Download failed");
            processQueue();
        }
        return;
    }

    // 保存到临时目录
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/lcsc_images";
    QDir().mkpath(cacheDir);
    QString filePath = cacheDir + "/" + componentId + ".jpg";

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        emit imageReady(componentId, filePath);
    } else {
        emit error(componentId, "Disk write error");
    }

    m_activeRequests--;
    processQueue();
}

}  // namespace EasyKiConverter
