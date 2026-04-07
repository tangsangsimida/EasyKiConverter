#include "NetworkClient.h"

#include "core/utils/GzipUtils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>

namespace EasyKiConverter {

NetworkClient::NetworkClient()
    : m_networkManager(new QNetworkAccessManager(this)) {
}

NetworkClient::~NetworkClient() = default;


// static
bool NetworkClient::isGzipCompressed(const QByteArray& data) {
    return GzipUtils::isGzipped(data);
}

// static
QByteArray NetworkClient::decompressGzip(const QByteArray& data) {
    GzipUtils::DecompressResult decompResult = GzipUtils::decompress(data);
    if (!decompResult.success) {
        qWarning() << "Gzip decompression failed";
        return QByteArray();
    }
    return decompResult.data;
}

NetworkResult NetworkClient::get(const QUrl& url, const RetryPolicy& policy) {
    return executeRequest(url, QByteArray(), policy);
}

NetworkResult NetworkClient::post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy) {
    return executeRequest(url, body, policy);
}

NetworkResult NetworkClient::executeRequest(const QUrl& url,
                                           const QByteArray& body,
                                           const RetryPolicy& policy) {
    NetworkResult result;
    QElapsedTimer timer;
    timer.start();

    for (int retryCount = 0; retryCount <= policy.maxRetries; ++retryCount) {
        QEventLoop loop;
        QNetworkRequest request(url);

        // Set headers
        request.setHeader(QNetworkRequest::UserAgentHeader, "EasyKiConverter/1.0");
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::AlwaysNetwork);

        QNetworkReply* reply = nullptr;
        if (body.isEmpty()) {
            reply = m_networkManager->get(request);
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            reply = m_networkManager->post(request, body);
        }

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(policy.baseTimeoutMs);

        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            reply->abort();
            loop.quit();
        });

        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        loop.exec();

        result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.elapsedMs = timer.elapsed();

        if (reply->error() == QNetworkReply::NoError) {
            result.data = reply->readAll();

            // Decompress if gzip
            if (GzipUtils::isGzipped(result.data)) {
                GzipUtils::DecompressResult decompResult = GzipUtils::decompress(result.data);
                if (decompResult.success) {
                    result.data = decompResult.data;
                } else {
                    qWarning() << "Gzip decompression failed for URL:" << url;
                    result.error = "Gzip decompression failed";
                    reply->deleteLater();
                    return result;
                }
            }

            result.success = true;
            result.retryCount = retryCount;
            reply->deleteLater();
            return result;
        }

        // Check if we should retry
        if (shouldRetry(result.statusCode, retryCount, policy)) {
            int delay = calculateRetryDelay(retryCount, policy);
            QThread::sleep(delay / 1000);
            reply->deleteLater();
            continue;
        }

        result.error = reply->errorString();
        result.success = false;
        reply->deleteLater();
        return result;
    }

    result.success = false;
    result.error = "Max retries exceeded";
    return result;
}

int NetworkClient::calculateRetryDelay(int retryCount, const RetryPolicy& policy) {
    if (retryCount >= static_cast<int>(policy.delays.size())) {
        return policy.delays.back();
    }
    int baseDelay = policy.delays[retryCount];

    // Add jitter using QRandomGenerator
    int jitterRange = static_cast<int>(baseDelay * policy.jitterFactor);
    int jitter = QRandomGenerator::global()->bounded(-jitterRange, jitterRange + 1);
    return baseDelay + jitter;
}

bool NetworkClient::shouldRetry(int statusCode, int retryCount, const RetryPolicy& policy) {
    if (retryCount >= policy.maxRetries) {
        return false;
    }

    // Retry on rate limit
    if (statusCode == 429) {
        return true;
    }

    // Retry on server errors
    if (statusCode >= 500 && statusCode < 600) {
        return true;
    }

    return false;
}

}  // namespace EasyKiConverter
