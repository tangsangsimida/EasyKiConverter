#include "NetworkClient.h"

#include "core/utils/GzipUtils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>

#include <memory>

namespace EasyKiConverter {

NetworkClient::NetworkClient() = default;

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
    return executeRequest(url, QByteArray(), ResourceType::Unknown, policy);
}

NetworkResult NetworkClient::get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy) {
    return executeRequest(url, QByteArray(), resourceType, policy);
}

NetworkResult NetworkClient::post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy) {
    return executeRequest(url, body, ResourceType::Unknown, policy);
}

NetworkResult NetworkClient::post(const QUrl& url,
                                  const QByteArray& body,
                                  ResourceType resourceType,
                                  const RetryPolicy& policy) {
    return executeRequest(url, body, resourceType, policy);
}

AsyncNetworkRequest* NetworkClient::getAsync(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy) {
    // Use thread-local QNetworkAccessManager to avoid cross-thread issues
    // This approach is the same as FetchWorker - each thread has its own QNAM
    static thread_local std::unique_ptr<QNetworkAccessManager> threadQNAM = nullptr;
    if (!threadQNAM) {
        threadQNAM = std::make_unique<QNetworkAccessManager>();
    }

    // Create request with thread-local QNAM - no parent, caller manages lifetime
    auto* request = new AsyncNetworkRequest(url, threadQNAM.get(), resourceType, policy, nullptr);

    // Start request synchronously in current thread (same thread as threadQNAM)
    // This avoids the crash issues with moveToThread
    request->start();

    return request;
}

AsyncNetworkRequest* NetworkClient::postAsync(const QUrl& url,
                                              const QByteArray& body,
                                              ResourceType resourceType,
                                              const RetryPolicy& policy) {
    // Use thread-local QNetworkAccessManager (same approach as getAsync)
    static thread_local std::unique_ptr<QNetworkAccessManager> threadQNAM = nullptr;
    if (!threadQNAM) {
        threadQNAM = std::make_unique<QNetworkAccessManager>();
    }

    auto* request = new AsyncNetworkRequest(url, threadQNAM.get(), resourceType, policy, body, nullptr);

    // Start request synchronously in current thread
    request->start();

    return request;
}

void NetworkClient::populateDiagnostic(NetworkDiagnostic& diag, const QUrl& url, ResourceType resourceType) {
    diag.url = url.toString();
    diag.host = url.host();
    diag.resourceType = resourceType;
    diag.profileName = RequestProfiles::fromType(resourceType).name;
}

NetworkResult NetworkClient::executeRequest(const QUrl& url,
                                            const QByteArray& body,
                                            ResourceType resourceType,
                                            const RetryPolicy& policy) {
    // Synchronous requests are used from worker threads during export.
    // Use a thread-local QNAM so reply objects are created in the calling thread.
    static thread_local std::unique_ptr<QNetworkAccessManager> threadQNAM = nullptr;
    if (!threadQNAM) {
        threadQNAM = std::make_unique<QNetworkAccessManager>();
    }

    NetworkResult result;
    QElapsedTimer timer;
    timer.start();

    // Populate diagnostic info
    populateDiagnostic(result.diagnostic, url, resourceType);

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
        result.diagnostic.latencyMs = result.elapsedMs;
        result.diagnostic.retryCount = retryCount;
        result.diagnostic.statusCode = result.statusCode;

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
                    result.diagnostic.errorType = NetworkErrorType::DecompressionFailed;
                    result.diagnostic.errorMessage = result.error;
                    reply->deleteLater();
                    return result;
                }
            }

            result.success = true;
            result.retryCount = retryCount;
            result.diagnostic.totalElapsedMs = result.elapsedMs;
            reply->deleteLater();
            return result;
        }

        // Classify error type for diagnostics
        QNetworkReply::NetworkError error = reply->error();
        switch (error) {
            case QNetworkReply::TimeoutError:
                result.diagnostic.errorType = NetworkErrorType::Timeout;
                break;
            case QNetworkReply::ConnectionRefusedError:
                result.diagnostic.errorType = NetworkErrorType::ConnectionRefused;
                break;
            case QNetworkReply::HostNotFoundError:
                result.diagnostic.errorType = NetworkErrorType::HostNotFound;
                break;
            case QNetworkReply::OperationCanceledError:
                result.diagnostic.errorType = NetworkErrorType::Canceled;
                result.diagnostic.wasCanceled = true;
                break;
            default:
                if (result.statusCode == 429) {
                    result.diagnostic.errorType = NetworkErrorType::RateLimited;
                    result.diagnostic.wasRateLimited = true;
                } else if (result.statusCode >= 500) {
                    result.diagnostic.errorType = NetworkErrorType::ServerError;
                } else if (result.statusCode == 404) {
                    result.diagnostic.errorType = NetworkErrorType::NotFound;
                } else if (result.statusCode == 403) {
                    result.diagnostic.errorType = NetworkErrorType::Forbidden;
                } else {
                    result.diagnostic.errorType = NetworkErrorType::Other;
                }
                break;
        }

        // Check if we should retry
        if (shouldRetry(result.statusCode, reply->error(), retryCount, policy)) {
            int delay = calculateRetryDelay(retryCount, policy);
            qDebug() << QString("NetworkClient: Retry %1/%2 for %3 after %4ms delay")
                            .arg(retryCount + 1)
                            .arg(policy.maxRetries)
                            .arg(url.toString())
                            .arg(delay);
            QThread::msleep(static_cast<unsigned long>(qMax(0, delay)));
            reply->deleteLater();
            continue;
        }

        result.error = reply->errorString();
        result.diagnostic.errorMessage = result.error;
        result.success = false;
        result.diagnostic.totalElapsedMs = result.elapsedMs;
        reply->deleteLater();
        return result;
    }

    result.success = false;
    result.error = "Max retries exceeded";
    result.diagnostic.errorMessage = result.error;
    result.diagnostic.totalElapsedMs = result.elapsedMs;
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

bool NetworkClient::shouldRetry(int statusCode,
                                QNetworkReply::NetworkError error,
                                int retryCount,
                                const RetryPolicy& policy) {
    if (retryCount >= policy.maxRetries) {
        return false;
    }

    if (policy.retryableStatusCodes.contains(statusCode)) {
        return true;
    }

    switch (error) {
        case QNetworkReply::TimeoutError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::NetworkSessionFailedError:
        case QNetworkReply::UnknownNetworkError:
        case QNetworkReply::RemoteHostClosedError:
        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::ProxyTimeoutError:
            return true;
        default:
            return false;
    }
}

}  // namespace EasyKiConverter
