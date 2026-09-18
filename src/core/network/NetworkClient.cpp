#include "NetworkClient.h"

#include "BlockingRequestContext.h"
#include "core/utils/GzipUtils.h"
#include "core/utils/UrlUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>

#include <limits>
#include <memory>

namespace EasyKiConverter {

NetworkClient::NetworkClient() {
    m_networkThread.setObjectName(QStringLiteral("EasyKiConverterNetworkThread"));
    m_networkThread.start();

    m_asyncNetworkManager = new QNetworkAccessManager();
    m_asyncNetworkManager->moveToThread(&m_networkThread);
}

NetworkClient::~NetworkClient() {
    cancelAllRequests();

    if (m_asyncNetworkManager) {
        QMetaObject::invokeMethod(m_asyncNetworkManager, &QObject::deleteLater, Qt::QueuedConnection);
        m_asyncNetworkManager = nullptr;
    }

    m_networkThread.quit();
    m_networkThread.wait();
}

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

/** @brief 发送使用默认资源类型的同步 GET 请求。 */
NetworkResult NetworkClient::get(const QUrl& url, const RetryPolicy& policy) {
    return executeRequest(url, QByteArray(), ResourceType::Unknown, policy);
}

/** @brief 发送带资源类型的同步 GET 请求。 */
NetworkResult NetworkClient::get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy) {
    return executeRequest(url, QByteArray(), resourceType, policy);
}

/** @brief 发送使用默认资源类型的同步 POST 请求。 */
NetworkResult NetworkClient::post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy) {
    return executeRequest(url, body, ResourceType::Unknown, policy);
}

NetworkResult NetworkClient::post(const QUrl& url,
                                  const QByteArray& body,
                                  ResourceType resourceType,
                                  const RetryPolicy& policy) {
    return executeRequest(url, body, resourceType, policy);
}

/** @brief 获取当前网络请求运行统计。 */
NetworkRuntimeStats NetworkClient::runtimeStats() const {
    NetworkRuntimeStats stats;

    QMutexLocker locker(&m_asyncQueueMutex);
    for (auto it = m_resourceStats.cbegin(); it != m_resourceStats.cend(); ++it) {
        const NetworkResourceStats& snapshot = it.value().snapshot;
        stats.totalQueuedRequests += snapshot.queuedRequests;
        stats.totalActiveRequests += snapshot.activeRequests;
        stats.totalStartedRequests += snapshot.startedRequests;
        stats.totalCompletedRequests += snapshot.completedRequests;
        stats.totalSucceededRequests += snapshot.succeededRequests;
        stats.totalFailedRequests += snapshot.failedRequests;
        stats.totalCancelledRequests += snapshot.cancelledRequests;
        stats.totalTimeoutRequests += snapshot.timeoutRequests;
        stats.totalRateLimitedRequests += snapshot.rateLimitedRequests;
        stats.totalRetryAttempts += snapshot.retryAttempts;
        stats.totalBackpressureEvents += snapshot.backpressureEvents;
        stats.resources.append(snapshot);
    }

    return stats;
}

/** @brief 将网络请求运行统计格式化为诊断文本。 */
QString NetworkClient::formatRuntimeStats() const {
    const NetworkRuntimeStats stats = runtimeStats();
    QStringList lines;
    lines << QStringLiteral(
                 "NetworkRuntimeStats total{queued=%1 active=%2 started=%3 completed=%4 ok=%5 fail=%6 "
                 "cancelled=%7 timeout=%8 rateLimited=%9 retries=%10 backpressure=%11}")
                 .arg(stats.totalQueuedRequests)
                 .arg(stats.totalActiveRequests)
                 .arg(stats.totalStartedRequests)
                 .arg(stats.totalCompletedRequests)
                 .arg(stats.totalSucceededRequests)
                 .arg(stats.totalFailedRequests)
                 .arg(stats.totalCancelledRequests)
                 .arg(stats.totalTimeoutRequests)
                 .arg(stats.totalRateLimitedRequests)
                 .arg(stats.totalRetryAttempts)
                 .arg(stats.totalBackpressureEvents);

    for (const NetworkResourceStats& resource : stats.resources) {
        lines << QStringLiteral(
                     "  [%1] queued=%2/%3 active=%4/%5 backpressure=%6 started=%7 completed=%8 ok=%9 fail=%10 "
                     "cancelled=%11 timeout=%12 rateLimited=%13 retries=%14 queueDelay(last/avg/max)=%15/%16/%17ms "
                     "latency(last/avg/max)=%18/%19/%20ms")
                     .arg(resource.profileName)
                     .arg(resource.queuedRequests)
                     .arg(resource.peakQueuedRequests)
                     .arg(resource.activeRequests)
                     .arg(resource.peakActiveRequests)
                     .arg(resource.backpressureEvents)
                     .arg(resource.startedRequests)
                     .arg(resource.completedRequests)
                     .arg(resource.succeededRequests)
                     .arg(resource.failedRequests)
                     .arg(resource.cancelledRequests)
                     .arg(resource.timeoutRequests)
                     .arg(resource.rateLimitedRequests)
                     .arg(resource.retryAttempts)
                     .arg(resource.lastQueueDelayMs)
                     .arg(resource.averageQueueDelayMs)
                     .arg(resource.maxQueueDelayMs)
                     .arg(resource.lastLatencyMs)
                     .arg(resource.averageLatencyMs)
                     .arg(resource.maxLatencyMs);
    }

    return lines.join(QLatin1Char('\n'));
}

/** @brief 创建异步 GET 请求并加入调度队列。 */
AsyncNetworkRequest* NetworkClient::getAsync(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy) {
    return enqueueAsyncRequest(url, QByteArray(), resourceType, policy);
}

AsyncNetworkRequest* NetworkClient::postAsync(const QUrl& url,
                                              const QByteArray& body,
                                              ResourceType resourceType,
                                              const RetryPolicy& policy) {
    return enqueueAsyncRequest(url, body, resourceType, policy);
}

/** @brief 填充网络请求诊断中的 URL、主机和资源类型。 */
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
    // 同步路径：在创建 request 前做 URL 校验，避免白名单拒绝后死锁
    // （QWaitCondition::wait() 不处理事件循环，queued 完成信号无法到达）
    {
        QString errorMsg;
        if (!UrlUtils::isAllowedUrl(url, resourceType, &errorMsg)) {
            qWarning() << "NetworkClient: URL rejected by whitelist (sync):" << errorMsg;
            NetworkResult result;
            populateDiagnostic(result.diagnostic, url, resourceType);
            result.error = errorMsg;
            result.success = false;
            result.diagnostic.errorType = NetworkErrorType::Other;
            result.diagnostic.errorMessage = errorMsg;
            return result;
        }
    }

    auto* request = enqueueAsyncRequest(url, body, resourceType, policy);
    if (!request) {
        NetworkResult result;
        populateDiagnostic(result.diagnostic, url, resourceType);
        result.error = QStringLiteral("Failed to create async request");
        result.success = false;
        result.diagnostic.errorType = NetworkErrorType::Other;
        result.diagnostic.errorMessage = result.error;
        return result;
    }

    auto context = std::make_shared<BlockingRequestContext>();
    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        request,
        [context](const NetworkResult& result) { context->complete(result); },
        Qt::DirectConnection);

    const NetworkResult result = context->wait();
    QMetaObject::invokeMethod(request, &QObject::deleteLater, Qt::QueuedConnection);
    return result;
}

AsyncNetworkRequest* NetworkClient::enqueueAsyncRequest(const QUrl& url,
                                                        const QByteArray& body,
                                                        ResourceType resourceType,
                                                        const RetryPolicy& policy) {
    if (!m_asyncNetworkManager) {
        return nullptr;
    }

    // URL 白名单校验：拒绝非预期的 scheme 或 host
    {
        QString errorMsg;
        if (!UrlUtils::isAllowedUrl(url, resourceType, &errorMsg)) {
            qWarning() << "NetworkClient: URL rejected by whitelist:" << errorMsg;
            // 返回已完成的失败 request，避免调用方需处理 nullptr
            NetworkResult failResult;
            failResult.success = false;
            failResult.wasCancelled = false;
            failResult.error = errorMsg;
            failResult.diagnostic.url = url.toString();
            failResult.diagnostic.host = url.host();
            failResult.diagnostic.resourceType = resourceType;
            failResult.diagnostic.profileName = RequestProfiles::fromType(resourceType).name;
            failResult.diagnostic.errorType = NetworkErrorType::Other;
            failResult.diagnostic.errorMessage = errorMsg;
            return AsyncNetworkRequest::createFinished(failResult);
        }
    }

    auto* request = new AsyncNetworkRequest(url, m_asyncNetworkManager, resourceType, policy, body, nullptr);
    request->moveToThread(&m_networkThread);

    const RequestProfile profile = RequestProfiles::fromType(resourceType);
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        PendingAsyncRequest pending;
        pending.request = request;
        pending.resourceType = resourceType;
        pending.priority = profile.priority;
        pending.sequence = m_asyncSequence++;
        pending.enqueuedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingAsyncRequests.append(pending);
        m_liveAsyncRequests.append(request);
        updateStatsForEnqueuedRequest(resourceType);
    }

    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        this,
        [this, request, resourceType](const NetworkResult& result) {
            updateStatsForCompletedRequest(resourceType, result);
            {
                QMutexLocker locker(&m_asyncQueueMutex);
                m_liveAsyncRequests.removeAll(request);
            }
            onAsyncRequestFinished(resourceType);
        },
        Qt::QueuedConnection);

    scheduleAsyncPump();
    return request;
}

/** @brief 安排异步请求队列的下一次调度。 */
void NetworkClient::scheduleAsyncPump() {
    bool shouldSchedule = false;
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        if (!m_asyncPumpScheduled) {
            m_asyncPumpScheduled = true;
            shouldSchedule = true;
        }
    }

    if (shouldSchedule) {
        QMetaObject::invokeMethod(this, &NetworkClient::pumpAsyncQueue, Qt::QueuedConnection);
    }
}

/** @brief 按并发和优先级限制启动等待中的异步请求。 */
void NetworkClient::pumpAsyncQueue() {
    while (true) {
        QPointer<AsyncNetworkRequest> requestToStart;
        ResourceType resourceType = ResourceType::Unknown;

        {
            QMutexLocker locker(&m_asyncQueueMutex);
            int bestIndex = -1;
            int bestPriority = std::numeric_limits<int>::max();
            quint64 bestSequence = std::numeric_limits<quint64>::max();

            for (int i = m_pendingAsyncRequests.size() - 1; i >= 0; --i) {
                const PendingAsyncRequest& pending = m_pendingAsyncRequests.at(i);
                if (!pending.request || pending.request->isFinished()) {
                    m_pendingAsyncRequests.removeAt(i);
                }
            }

            for (int i = 0; i < m_pendingAsyncRequests.size(); ++i) {
                const PendingAsyncRequest& pending = m_pendingAsyncRequests.at(i);
                if (!pending.request || pending.request->isFinished()) {
                    continue;
                }

                const RequestProfile profile = RequestProfiles::fromType(pending.resourceType);
                const int activeCount = m_activeAsyncRequestsByType.value(static_cast<int>(pending.resourceType), 0);
                if (activeCount >= profile.maxConcurrent) {
                    continue;
                }

                if (pending.priority < bestPriority ||
                    (pending.priority == bestPriority && pending.sequence < bestSequence)) {
                    bestIndex = i;
                    bestPriority = pending.priority;
                    bestSequence = pending.sequence;
                }
            }

            if (bestIndex < 0) {
                m_asyncPumpScheduled = false;
                break;
            }

            PendingAsyncRequest pending = m_pendingAsyncRequests.takeAt(bestIndex);
            requestToStart = pending.request;
            resourceType = pending.resourceType;
            m_activeAsyncRequestsByType[static_cast<int>(resourceType)] =
                m_activeAsyncRequestsByType.value(static_cast<int>(resourceType), 0) + 1;
            const qint64 queueDelayMs = qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - pending.enqueuedAtMs);
            updateStatsForDequeuedRequest(resourceType, queueDelayMs);
            updateStatsForStartedRequest(resourceType);
        }

        if (!requestToStart) {
            onAsyncRequestFinished(resourceType);
            continue;
        }

        QMetaObject::invokeMethod(
            requestToStart,
            [requestToStart]() {
                if (requestToStart) {
                    requestToStart->start();
                }
            },
            Qt::QueuedConnection);
    }
}

/** @brief 取消队列中和运行中的全部异步请求。 */
void NetworkClient::cancelAllRequests() {
    QList<QPointer<AsyncNetworkRequest>> requestsToCancel;
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        requestsToCancel = m_liveAsyncRequests;
        m_pendingAsyncRequests.clear();
        m_asyncPumpScheduled = false;
    }

    for (const QPointer<AsyncNetworkRequest>& request : std::as_const(requestsToCancel)) {
        if (!request || request->isFinished()) {
            continue;
        }

        if (request->thread() == QThread::currentThread()) {
            request->cancel();
        } else if (QThread::currentThread() == &m_networkThread) {
            request->cancel();
        } else {
            const Qt::ConnectionType connectionType =
                m_networkThread.isRunning() ? Qt::BlockingQueuedConnection : Qt::QueuedConnection;
            QMetaObject::invokeMethod(request, &AsyncNetworkRequest::cancel, connectionType);
        }
    }
}

/** @brief 收敛异步请求完成后的活动计数并继续调度。 */
void NetworkClient::onAsyncRequestFinished(ResourceType resourceType) {
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        const int key = static_cast<int>(resourceType);
        const int activeCount = m_activeAsyncRequestsByType.value(key, 0);
        if (activeCount <= 1) {
            m_activeAsyncRequestsByType.remove(key);
        } else {
            m_activeAsyncRequestsByType[key] = activeCount - 1;
        }
        refreshDynamicStatsLocked(resourceType);
    }

    scheduleAsyncPump();
}

/** @brief 记录请求入队后的资源统计。 */
void NetworkClient::updateStatsForEnqueuedRequest(ResourceType resourceType) {
    const int key = static_cast<int>(resourceType);
    MutableResourceStats& stats = m_resourceStats[key];
    const RequestProfile profile = RequestProfiles::fromType(resourceType);
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    if (m_activeAsyncRequestsByType.value(key, 0) >= profile.maxConcurrent) {
        ++stats.snapshot.backpressureEvents;
    }
    refreshDynamicStatsLocked(resourceType);
}

/** @brief 记录请求离开等待队列时的延迟统计。 */
void NetworkClient::updateStatsForDequeuedRequest(ResourceType resourceType, qint64 queueDelayMs) {
    const int key = static_cast<int>(resourceType);
    MutableResourceStats& stats = m_resourceStats[key];
    const RequestProfile profile = RequestProfiles::fromType(resourceType);
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    stats.snapshot.lastQueueDelayMs = queueDelayMs;
    stats.totalQueueDelayMs += queueDelayMs;
    stats.snapshot.averageQueueDelayMs =
        stats.snapshot.startedRequests > 0
            ? (stats.totalQueueDelayMs / static_cast<qint64>(stats.snapshot.startedRequests))
            : 0;
    stats.snapshot.maxQueueDelayMs = qMax(stats.snapshot.maxQueueDelayMs, queueDelayMs);
    refreshDynamicStatsLocked(resourceType);
}

/** @brief 记录请求开始执行时的资源统计。 */
void NetworkClient::updateStatsForStartedRequest(ResourceType resourceType) {
    const int key = static_cast<int>(resourceType);
    MutableResourceStats& stats = m_resourceStats[key];
    const RequestProfile profile = RequestProfiles::fromType(resourceType);
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    ++stats.snapshot.startedRequests;
    stats.snapshot.averageQueueDelayMs =
        stats.snapshot.startedRequests > 0
            ? (stats.totalQueueDelayMs / static_cast<qint64>(stats.snapshot.startedRequests))
            : 0;
    refreshDynamicStatsLocked(resourceType);
}

/** @brief 根据请求结果更新完成、失败和重试统计。 */
void NetworkClient::updateStatsForCompletedRequest(ResourceType resourceType, const NetworkResult& result) {
    QMutexLocker locker(&m_asyncQueueMutex);
    const int key = static_cast<int>(resourceType);
    MutableResourceStats& stats = m_resourceStats[key];
    const RequestProfile profile = RequestProfiles::fromType(resourceType);
    stats.snapshot.resourceType = resourceType;
    stats.snapshot.profileName = profile.name;
    stats.snapshot.maxConcurrent = profile.maxConcurrent;
    ++stats.snapshot.completedRequests;
    stats.snapshot.retryAttempts += static_cast<quint64>(qMax(0, result.retryCount));
    stats.snapshot.lastLatencyMs = result.elapsedMs;
    stats.totalLatencyMs += result.elapsedMs;
    stats.snapshot.averageLatencyMs =
        stats.snapshot.completedRequests > 0
            ? (stats.totalLatencyMs / static_cast<qint64>(stats.snapshot.completedRequests))
            : 0;
    stats.snapshot.maxLatencyMs = qMax(stats.snapshot.maxLatencyMs, result.elapsedMs);

    if (result.wasCancelled || result.diagnostic.wasCanceled) {
        ++stats.snapshot.cancelledRequests;
    } else if (result.success) {
        ++stats.snapshot.succeededRequests;
    } else {
        ++stats.snapshot.failedRequests;
    }

    if (result.diagnostic.errorType == NetworkErrorType::Timeout) {
        ++stats.snapshot.timeoutRequests;
    }
    if (result.diagnostic.wasRateLimited || result.statusCode == 429) {
        ++stats.snapshot.rateLimitedRequests;
    }
    refreshDynamicStatsLocked(resourceType);
}

/** @brief 在持有统计锁时刷新指定资源的动态计数。 */
void NetworkClient::refreshDynamicStatsLocked(ResourceType resourceType) {
    const int key = static_cast<int>(resourceType);
    MutableResourceStats& stats = m_resourceStats[key];
    int queuedCount = 0;
    for (const PendingAsyncRequest& pending : std::as_const(m_pendingAsyncRequests)) {
        if (pending.request && !pending.request->isFinished() && pending.resourceType == resourceType) {
            ++queuedCount;
        }
    }

    stats.snapshot.queuedRequests = queuedCount;
    stats.snapshot.activeRequests = m_activeAsyncRequestsByType.value(key, 0);
    stats.snapshot.peakQueuedRequests = qMax(stats.snapshot.peakQueuedRequests, queuedCount);
    stats.snapshot.peakActiveRequests = qMax(stats.snapshot.peakActiveRequests, stats.snapshot.activeRequests);
}

/** @brief 计算重试次数对应的退避时长。 */
int NetworkClient::calculateRetryDelay(int retryCount, const RetryPolicy& policy) {
    if (retryCount >= static_cast<int>(policy.delays.size())) {
        return policy.delays.back();
    }
    int baseDelay = policy.delays[retryCount];

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

    // 网络层临时错误允许进入重试流程。
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
