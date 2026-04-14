#include "NetworkClient.h"

#include "core/utils/GzipUtils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>

#include <memory>

namespace EasyKiConverter {

NetworkClient::NetworkClient() : m_networkManager(new QNetworkAccessManager(this)) {}

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

AsyncNetworkRequest* NetworkClient::getAsync(const QUrl& url, const RetryPolicy& policy) {
    // Use thread-local QNetworkAccessManager to avoid cross-thread issues
    // This approach is the same as FetchWorker - each thread has its own QNAM
    static thread_local std::unique_ptr<QNetworkAccessManager> threadQNAM = nullptr;
    if (!threadQNAM) {
        threadQNAM = std::make_unique<QNetworkAccessManager>();
    }

    // Create request with thread-local QNAM - no parent, caller manages lifetime
    auto* request = new AsyncNetworkRequest(url, threadQNAM.get(), policy, nullptr);

    // Start request synchronously in current thread (same thread as threadQNAM)
    // This avoids the crash issues with moveToThread
    request->start();

    return request;
}

AsyncNetworkRequest* NetworkClient::postAsync(const QUrl& url, const QByteArray& body, const RetryPolicy& policy) {
    // Use thread-local QNetworkAccessManager (same approach as getAsync)
    static thread_local std::unique_ptr<QNetworkAccessManager> threadQNAM = nullptr;
    if (!threadQNAM) {
        threadQNAM = std::make_unique<QNetworkAccessManager>();
    }

    auto* request = new AsyncNetworkRequest(url, threadQNAM.get(), policy, body, nullptr);

    // Start request synchronously in current thread
    request->start();

    return request;
}

NetworkResult NetworkClient::executeRequest(const QUrl& url, const QByteArray& body, const RetryPolicy& policy) {
    // Synchronous requests are used from worker threads during export.
    // Use a thread-local QNAM so reply objects are created in the calling thread.
    static thread_local std::unique_ptr<QNetworkAccessManager> threadQNAM = nullptr;
    if (!threadQNAM) {
        threadQNAM = std::make_unique<QNetworkAccessManager>();
    }

    NetworkResult result;
    QElapsedTimer timer;
    timer.start();

    for (int retryCount = 0; retryCount <= policy.maxRetries; ++retryCount) {
        QEventLoop loop;
        QNetworkRequest request(url);

        // Set headers
        request.setHeader(QNetworkRequest::UserAgentHeader, "EasyKiConverter/1.0");
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

        QNetworkReply* reply = nullptr;
        if (body.isEmpty()) {
            reply = threadQNAM->get(request);
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            reply = threadQNAM->post(request, body);
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
