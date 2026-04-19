#include "AsyncNetworkRequest.h"

#include "core/utils/GzipUtils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

namespace EasyKiConverter {

// Static member initialization
const QList<QNetworkReply*> AsyncNetworkRequest::s_emptyReplies;

AsyncNetworkRequest* AsyncNetworkRequest::createFinished(const NetworkResult& result, QObject* parent) {
    auto* request =
        new AsyncNetworkRequest(QUrl(), nullptr, result.diagnostic.resourceType, RetryPolicy(), QByteArray(), parent);
    if (result.wasCancelled) {
        request->m_cancelled.storeRelease(1);
    }
    request->completeWithResult(result);
    return request;
}

AsyncNetworkRequest::AsyncNetworkRequest(const QUrl& url,
                                         QNetworkAccessManager* networkManager,
                                         ResourceType resourceType,
                                         const RetryPolicy& policy,
                                         const QByteArray& body,
                                         QObject* parent)
    : QObject(parent)
    , m_url(url)
    , m_networkManager(networkManager)
    , m_resourceType(resourceType)
    , m_policy(policy)
    , m_postBody(body)
    , m_isPostRequest(!body.isEmpty())
    , m_cancelled(0)
    , m_finished(0)
    , m_currentReply(nullptr)
    , m_timeoutTimer(nullptr)
    , m_currentRetryCount(0)
    , m_timeoutMs(policy.baseTimeoutMs)
    , m_gzipUsed(false)
    , m_currentAttemptTimedOut(0) {
    m_result.diagnostic.url = m_url.toString();
    m_result.diagnostic.host = m_url.host();
    m_result.diagnostic.resourceType = m_resourceType;
    m_result.diagnostic.profileName = RequestProfiles::fromType(m_resourceType).name;
}

AsyncNetworkRequest::~AsyncNetworkRequest() {
    // Cancel any pending operations
    cancel();

    // Clean up any pending replies
    QMutexLocker locker(&m_repliesMutex);
    for (QNetworkReply* reply : m_pendingReplies) {
        if (reply && !reply->isFinished()) {
            reply->abort();
        }
        if (reply) {
            reply->deleteLater();
        }
    }
    m_pendingReplies.clear();

    // Clean up current reply if still active
    if (m_currentReply && !m_currentReply->isFinished()) {
        m_currentReply->abort();
    }
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void AsyncNetworkRequest::cancel() {
    if (!m_cancelled.testAndSetRelaxed(0, 1)) {
        return;
    }

    qDebug() << "AsyncNetworkRequest: Cancelling request to" << m_url;

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    bool shouldCompleteImmediately = false;
    {
        QMutexLocker locker(&m_repliesMutex);
        if (m_currentReply && !m_currentReply->isFinished()) {
            m_currentReply->abort();
        } else {
            shouldCompleteImmediately = true;
        }
    }

    if (shouldCompleteImmediately) {
        completeWithResult(cancelledResult());
    }
}

bool AsyncNetworkRequest::isCancelled() const {
    return m_cancelled.loadRelaxed() != 0;
}

bool AsyncNetworkRequest::isFinished() const {
    return m_finished.loadRelaxed() != 0;
}

NetworkResult AsyncNetworkRequest::result() const {
    QMutexLocker locker(&m_resultMutex);
    return m_result;
}

void AsyncNetworkRequest::setTimeoutMs(int ms) {
    m_timeoutMs = ms;
}

int AsyncNetworkRequest::currentRetryCount() const {
    return m_currentRetryCount;
}

void AsyncNetworkRequest::start() {
    if (isCancelled() || isFinished()) {
        return;
    }

    qDebug() << "AsyncNetworkRequest: Starting request to" << m_url;

    // Call startAttempt directly since:
    // 1. We're using thread-local QNetworkAccessManager (same thread)
    // 2. Signals use Qt::DirectConnection (work in same thread)
    // 3. No event loop needed for QTimer in same thread
    startAttempt(0);
}

void AsyncNetworkRequest::startAttempt(int attemptNumber) {
    if (isCancelled() || isFinished()) {
        return;
    }

    m_currentAttemptTimedOut.storeRelease(0);

    if (!m_networkManager) {
        NetworkResult result = m_result;
        result.error = "Network manager is null";
        result.success = false;
        result.wasCancelled = false;
        result.diagnostic.errorType = NetworkErrorType::Other;
        result.diagnostic.errorMessage = result.error;
        completeWithResult(result);
        return;
    }

    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EasyKiConverter/1.0");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    // Note: We handle gzip manually in processResponse() since Qt doesn't auto-decompress

    // Create the reply - use POST if body is provided
    QNetworkReply* reply;
    if (m_isPostRequest && !m_postBody.isEmpty()) {
        // Auto-detect Content-Type based on body format
        // If body contains = and &, it's likely URL-encoded form data
        if (m_postBody.contains('=') && m_postBody.contains('&')) {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        }
        reply = m_networkManager->post(request, m_postBody);
    } else {
        reply = m_networkManager->get(request);
    }

    {
        QMutexLocker locker(&m_repliesMutex);
        m_currentReply = reply;
        m_pendingReplies.append(reply);
    }

    // Set up timeout (no parent - created/destroyed in same thread pool thread)
    m_timeoutTimer = new QTimer();
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AsyncNetworkRequest::handleTimeout, Qt::DirectConnection);
    if (m_timeoutMs > 0) {
        m_timeoutTimer->start(m_timeoutMs);
    }

    // Connect signals - use DirectConnection since we want immediate handling
    // The slot will run in this object's thread (main thread where AsyncNetworkRequest lives)
    connect(reply, &QNetworkReply::finished, this, &AsyncNetworkRequest::handleAttemptFinished, Qt::DirectConnection);
    connect(
        reply, &QNetworkReply::downloadProgress, this, &AsyncNetworkRequest::downloadProgress, Qt::DirectConnection);

    qDebug() << "AsyncNetworkRequest: Attempt" << attemptNumber << "started for" << m_url;
}

void AsyncNetworkRequest::handleAttemptFinished() {
    if (isCancelled() || isFinished()) {
        return;
    }

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // Stop timeout
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    // Check if this is our current reply
    {
        QMutexLocker locker(&m_repliesMutex);
        if (reply != m_currentReply) {
            // Stale reply, ignore
            return;
        }
    }

    // Process the response
    processResponse(reply);
}

void AsyncNetworkRequest::handleTimeout() {
    if (isCancelled() || isFinished()) {
        return;
    }

    qDebug() << "AsyncNetworkRequest: Timeout for" << m_url;
    emit timeout();
    m_currentAttemptTimedOut.storeRelease(1);

    {
        QMutexLocker locker(&m_repliesMutex);
        if (m_currentReply) {
            m_currentReply->abort();
        }
    }
}

void AsyncNetworkRequest::processResponse(QNetworkReply* reply) {
    NetworkResult result;
    {
        QMutexLocker locker(&m_resultMutex);
        result = m_result;
    }

    result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.diagnostic.statusCode = result.statusCode;
    result.diagnostic.retryCount = m_currentRetryCount;

    // Check for error
    if (reply->error() != QNetworkReply::NoError) {
        QString errorString = reply->errorString();

        // Check if cancelled
        if (isCancelled()) {
            completeWithResult(cancelledResult());
            return;
        }

        if (m_currentAttemptTimedOut.loadAcquire() != 0) {
            if (shouldRetryInternal(result.statusCode, QNetworkReply::TimeoutError, m_currentRetryCount)) {
                scheduleRetry(m_currentRetryCount);
            } else {
                completeWithResult(timeoutResult());
            }
            return;
        }

        // Check if should retry
        if (shouldRetryInternal(result.statusCode, reply->error(), m_currentRetryCount)) {
            scheduleRetry(m_currentRetryCount);
            return;
        }

        // Final error
        result.error = errorString;
        result.success = false;
        result.wasCancelled = false;
        result.diagnostic.errorMessage = result.error;

        switch (reply->error()) {
            case QNetworkReply::TimeoutError:
                result.diagnostic.errorType = NetworkErrorType::Timeout;
                break;
            case QNetworkReply::ConnectionRefusedError:
                result.diagnostic.errorType = NetworkErrorType::ConnectionRefused;
                break;
            case QNetworkReply::HostNotFoundError:
                result.diagnostic.errorType = NetworkErrorType::HostNotFound;
                break;
            case QNetworkReply::SslHandshakeFailedError:
                result.diagnostic.errorType = NetworkErrorType::SSLError;
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
        completeWithResult(result);
        return;
    }

    // Success - read data
    QByteArray data = reply->readAll();

    // Check for gzip encoding and decompress if needed
    QVariant encodingVariant = reply->rawHeader("Content-Encoding");
    QString encoding = encodingVariant.toString().toLower();
    m_gzipUsed = encoding.contains("gzip");

    if (m_gzipUsed || GzipUtils::isGzipped(data)) {
        GzipUtils::DecompressResult decompResult = GzipUtils::decompress(data);
        if (decompResult.success) {
            data = decompResult.data;
        } else {
            result.error = "Gzip decompression failed";
            result.success = false;
            result.wasCancelled = false;
            result.diagnostic.errorType = NetworkErrorType::DecompressionFailed;
            result.diagnostic.errorMessage = result.error;
            completeWithResult(result);
            return;
        }
    }

    result.data = data;
    result.success = true;
    result.retryCount = m_currentRetryCount;
    result.wasCancelled = false;
    result.diagnostic.errorType = NetworkErrorType::None;
    result.diagnostic.errorMessage.clear();

    completeWithResult(result);
}

void AsyncNetworkRequest::scheduleRetry(int retryCount) {
    if (isCancelled() || isFinished()) {
        return;
    }

    int delay = calculateRetryDelay(retryCount);
    m_currentRetryCount = retryCount + 1;

    qDebug() << "AsyncNetworkRequest: Scheduling retry" << m_currentRetryCount << "for" << m_url << "after" << delay
             << "ms";

    emit retrying(m_currentRetryCount, delay);

    // Use a timer to delay retry - this allows cancellation to be checked
    QTimer::singleShot(delay, this, [this]() {
        if (isCancelled() || isFinished()) {
            return;
        }

        // Clean up old reply
        cleanupAttempt();

        // Start new attempt in main thread where QNetworkAccessManager lives
        QMetaObject::invokeMethod(this, [this]() { startAttempt(m_currentRetryCount); }, Qt::QueuedConnection);
    });
}

int AsyncNetworkRequest::calculateRetryDelay(int retryCount) const {
    if (retryCount >= static_cast<int>(m_policy.delays.size())) {
        return m_policy.delays.back();
    }
    int baseDelay = m_policy.delays[retryCount];

    // Add jitter
    int jitterRange = static_cast<int>(baseDelay * m_policy.jitterFactor);
    int jitter = QRandomGenerator::global()->bounded(-jitterRange, jitterRange + 1);
    return baseDelay + jitter;
}

bool AsyncNetworkRequest::shouldRetryInternal(int statusCode, QNetworkReply::NetworkError error, int retryCount) const {
    if (retryCount >= m_policy.maxRetries) {
        return false;
    }

    if (m_policy.retryableStatusCodes.contains(statusCode)) {
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

void AsyncNetworkRequest::cleanupAttempt() {
    QMutexLocker locker(&m_repliesMutex);

    if (m_currentReply) {
        m_pendingReplies.removeAll(m_currentReply);
        if (!m_currentReply->isFinished()) {
            m_currentReply->abort();
        }
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }

    m_currentAttemptTimedOut.storeRelease(0);
}

bool AsyncNetworkRequest::completeWithResult(const NetworkResult& result) {
    if (!m_finished.testAndSetOrdered(0, 1)) {
        return false;
    }

    {
        QMutexLocker locker(&m_resultMutex);
        m_result = result;
    }

    emit finished(result);
    return true;
}

NetworkResult AsyncNetworkRequest::cancelledResult(const QString& errorMessage) const {
    NetworkResult result;
    {
        QMutexLocker locker(&m_resultMutex);
        result = m_result;
    }

    result.success = false;
    result.wasCancelled = true;
    result.error = errorMessage;
    result.diagnostic.wasCanceled = true;
    result.diagnostic.errorType = NetworkErrorType::Canceled;
    result.diagnostic.errorMessage = result.error;
    result.diagnostic.retryCount = m_currentRetryCount;
    return result;
}

NetworkResult AsyncNetworkRequest::timeoutResult() const {
    NetworkResult result;
    {
        QMutexLocker locker(&m_resultMutex);
        result = m_result;
    }

    result.success = false;
    result.wasCancelled = false;
    result.error = "Timeout";
    result.diagnostic.errorType = NetworkErrorType::Timeout;
    result.diagnostic.errorMessage = result.error;
    result.diagnostic.retryCount = m_currentRetryCount;
    return result;
}

bool AsyncNetworkRequest::hasActiveReply() const {
    QMutexLocker locker(&m_repliesMutex);
    return m_currentReply && !m_currentReply->isFinished();
}

void AsyncNetworkRequest::completeWithResultDelayed(const NetworkResult& result, int delayMs) {
    if (delayMs <= 0) {
        QTimer::singleShot(0, this, [this, result]() { completeWithResult(result); });
    } else {
        QTimer::singleShot(delayMs, this, [this, result]() { completeWithResult(result); });
    }
}

}  // namespace EasyKiConverter
