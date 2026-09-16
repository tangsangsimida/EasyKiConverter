#include "AsyncNetworkRequest.h"

#include "core/utils/GzipUtils.h"
#include "core/utils/UrlUtils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>

namespace EasyKiConverter {

// Static member initialization
const QList<QNetworkReply*> AsyncNetworkRequest::s_emptyReplies;

namespace {

constexpr int kResponseSummaryLimit = 512;

// 读取并限制响应头长度，避免把控制字符或异常长值带入诊断日志。
QString boundedHeaderValue(QNetworkReply* reply, const QByteArray& name) {
    if (!reply) {
        return QString();
    }
    QString value = QString::fromLatin1(reply->rawHeader(name)).trimmed();
    value.replace(QRegularExpression(QStringLiteral("[\\r\\n]")), QString());
    return value.left(256);
}

// 从错误响应中提取可用于判断访问策略和限流状态的非敏感摘要。
void captureErrorResponseDiagnostics(QNetworkReply* reply, NetworkResult& result) {
    if (!reply) {
        return;
    }

    result.diagnostic.responseContentType = boundedHeaderValue(reply, "Content-Type");
    result.diagnostic.retryAfter = boundedHeaderValue(reply, "Retry-After");
    result.diagnostic.rateLimitRemaining = boundedHeaderValue(reply, "X-RateLimit-Remaining");
    result.diagnostic.rateLimitReset = boundedHeaderValue(reply, "X-RateLimit-Reset");
    result.diagnostic.hasRateLimitHint = !result.diagnostic.retryAfter.isEmpty() ||
                                         !result.diagnostic.rateLimitRemaining.isEmpty() ||
                                         !result.diagnostic.rateLimitReset.isEmpty();

    const QString contentType = result.diagnostic.responseContentType.toLower();
    const bool isTextResponse = contentType.startsWith(QStringLiteral("text/")) ||
                                contentType.contains(QStringLiteral("json")) ||
                                contentType.contains(QStringLiteral("xml"));
    if (!isTextResponse) {
        return;
    }

    const QByteArray body = reply->read(kResponseSummaryLimit);
    QString summary = QString::fromUtf8(body).simplified();
    if (summary.size() > kResponseSummaryLimit) {
        summary = summary.left(kResponseSummaryLimit) + QStringLiteral("...");
    }
    result.diagnostic.responseSummary = summary;
}

}  // namespace

// 创建已经完成的请求对象，供同步结果和测试场景复用。
AsyncNetworkRequest* AsyncNetworkRequest::createFinished(const NetworkResult& result, QObject* parent) {
    auto* request =
        new AsyncNetworkRequest(QUrl(), nullptr, result.diagnostic.resourceType, RetryPolicy(), QByteArray(), parent);
    if (result.wasCancelled) {
        request->m_cancelled.storeRelease(1);
    }
    // 设置已完成状态（同步），供调用方立即检查 isFinished() / result()
    {
        QMutexLocker locker(&request->m_resultMutex);
        request->m_result = result;
    }
    request->m_finished.storeRelease(1);

    // 延迟发射 finished 信号，确保调用方有机会先 connect
    // 使用 QTimer::singleShot(0, ...) 投递到当前线程的事件循环
    // 调用方必须在有事件循环的线程中使用（主线程、QThread 子类）
    QTimer::singleShot(0, request, [request, result]() { emit request->finished(result); });
    return request;
}

// 初始化请求状态、网络策略和诊断上下文。
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

// 在所属线程中中止当前请求并发布取消结果。
void AsyncNetworkRequest::cancel() {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, &AsyncNetworkRequest::cancel, Qt::QueuedConnection);
        return;
    }

    if (!m_cancelled.testAndSetRelaxed(0, 1)) {
        return;
    }

    qDebug() << "AsyncNetworkRequest: Cancelling request to" << m_url;

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    {
        QMutexLocker locker(&m_repliesMutex);
        if (m_currentReply && !m_currentReply->isFinished()) {
            m_currentReply->abort();
        }
    }

    completeWithResult(cancelledResult());
}

// 返回请求是否已经进入取消状态。
bool AsyncNetworkRequest::isCancelled() const {
    return m_cancelled.loadRelaxed() != 0;
}

// 返回请求是否已经产生最终结果。
bool AsyncNetworkRequest::isFinished() const {
    return m_finished.loadRelaxed() != 0;
}

// 在线程安全地读取请求结果。
NetworkResult AsyncNetworkRequest::result() const {
    QMutexLocker locker(&m_resultMutex);
    return m_result;
}

// 设置后续请求尝试使用的超时时间。
void AsyncNetworkRequest::setTimeoutMs(int ms) {
    m_timeoutMs = ms;
}

// 返回已经执行的重试次数。
int AsyncNetworkRequest::currentRetryCount() const {
    return m_currentRetryCount;
}

// 从第一次尝试开始执行请求生命周期。
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

// 启动一次网络请求，并安装超时、进度和完成处理器。
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
    // 禁止不安全的重定向（HTTPS -> HTTP），防止降级攻击
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
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
    connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            &AsyncNetworkRequest::handleDownloadProgress,
            Qt::DirectConnection);

    qDebug() << "AsyncNetworkRequest: Attempt" << attemptNumber << "started for" << m_url;
}

// 过滤过期回复并处理当前网络尝试的最终信号。
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

// 标记当前尝试超时并中止底层回复。
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

// 转发下载进度并在响应过大时提前终止请求。
void AsyncNetworkRequest::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (isCancelled() || isFinished()) {
        return;
    }

    // 转发进度信号
    emit downloadProgress(bytesReceived, bytesTotal);

    // 检查响应大小限制
    const RequestProfile profile = RequestProfiles::fromType(m_resourceType);
    const qint64 maxBytes = profile.maxResponseBytes;

    if (maxBytes <= 0) {
        return;  // 无限制
    }

    // 优先使用 Content-Length（bytesTotal），其次用已接收字节数
    const qint64 checkSize = (bytesTotal > 0) ? bytesTotal : bytesReceived;

    if (checkSize > maxBytes) {
        qWarning() << "AsyncNetworkRequest: Response too large (" << checkSize << "bytes, limit" << maxBytes
                   << "), aborting:" << m_url;
        QMutexLocker locker(&m_repliesMutex);
        if (m_currentReply) {
            m_currentReply->abort();
        }
    }
}

// 将网络回复转换为统一结果并提取错误诊断。
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
        captureErrorResponseDiagnostics(reply, result);
        result.error =
            result.statusCode > 0 ? QStringLiteral("HTTP %1: %2").arg(result.statusCode).arg(errorString) : errorString;
        result.success = false;
        result.wasCancelled = false;
        result.diagnostic.errorMessage = result.error;

        // 依据底层网络错误和 HTTP 状态码生成统一分类。
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

    // 安全兜底：检查实际读取的数据大小（Content-Length 可能缺失或不准确）
    {
        const RequestProfile profile = RequestProfiles::fromType(m_resourceType);
        if (profile.maxResponseBytes > 0 && data.size() > profile.maxResponseBytes) {
            result.error = QStringLiteral("Response too large: %1 bytes (limit %2)")
                               .arg(data.size())
                               .arg(profile.maxResponseBytes);
            result.success = false;
            result.wasCancelled = false;
            result.diagnostic.errorType = NetworkErrorType::Other;
            result.diagnostic.errorMessage = result.error;
            completeWithResult(result);
            return;
        }
    }

    // 校验重定向后的最终 URL 是否仍在白名单内
    {
        const QUrl finalUrl = reply->url();
        if (finalUrl != m_url) {
            QString errorMsg;
            if (!UrlUtils::isAllowedUrl(finalUrl, m_resourceType, &errorMsg)) {
                result.error = QStringLiteral("Redirect to disallowed URL: %1").arg(errorMsg);
                result.success = false;
                result.wasCancelled = false;
                result.diagnostic.errorType = NetworkErrorType::Other;
                result.diagnostic.errorMessage = result.error;
                completeWithResult(result);
                return;
            }
        }
    }

    // Check for gzip encoding and decompress if needed
    QVariant encodingVariant = reply->rawHeader("Content-Encoding");
    QString encoding = encodingVariant.toString().toLower();
    m_gzipUsed = encoding.contains("gzip");

    if (m_gzipUsed || GzipUtils::isGzipped(data)) {
        GzipUtils::DecompressResult decompResult = GzipUtils::decompress(data);
        if (decompResult.success) {
            data = decompResult.data;

            // 解压后重新检查大小限制（压缩可能放大数据）
            const RequestProfile profile = RequestProfiles::fromType(m_resourceType);
            if (profile.maxResponseBytes > 0 && data.size() > profile.maxResponseBytes) {
                result.error = QStringLiteral("Decompressed response too large: %1 bytes (limit %2)")
                                   .arg(data.size())
                                   .arg(profile.maxResponseBytes);
                result.success = false;
                result.wasCancelled = false;
                result.diagnostic.errorType = NetworkErrorType::Other;
                result.diagnostic.errorMessage = result.error;
                completeWithResult(result);
                return;
            }
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

// 按退避策略安排下一次请求尝试。
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

// 计算带抖动的指数退避等待时间。
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

// 判断当前错误是否允许再次请求。
bool AsyncNetworkRequest::shouldRetryInternal(int statusCode, QNetworkReply::NetworkError error, int retryCount) const {
    if (retryCount >= m_policy.maxRetries) {
        return false;
    }

    if (m_policy.retryableStatusCodes.contains(statusCode)) {
        return true;
    }

    // 网络层瞬时错误可重试，明确的业务拒绝不重试。
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

// 释放当前回复和计时器，为下一次尝试清理状态。
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

// 只允许第一个完成结果写入状态并发出信号。
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

// 构造保留原始诊断上下文的取消结果。
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

// 构造保留原始诊断上下文的超时结果。
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

// 检查当前是否仍有未完成的网络回复。
bool AsyncNetworkRequest::hasActiveReply() const {
    QMutexLocker locker(&m_repliesMutex);
    return m_currentReply && !m_currentReply->isFinished();
}

// 延迟发布结果，保证异步调用方有机会连接完成信号。
void AsyncNetworkRequest::completeWithResultDelayed(const NetworkResult& result, int delayMs) {
    if (delayMs <= 0) {
        QTimer::singleShot(0, this, [this, result]() { completeWithResult(result); });
    } else {
        QTimer::singleShot(delayMs, this, [this, result]() { completeWithResult(result); });
    }
}

}  // namespace EasyKiConverter
