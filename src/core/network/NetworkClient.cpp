#include "NetworkClient.h"

#include "core/utils/GzipUtils.h"

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

namespace {

class BlockingRequestContext {
public:
    void complete(const NetworkResult& result) {
        QMutexLocker locker(&mutex);
        if (finished) {
            return;
        }
        storedResult = result;
        finished = true;
        condition.wakeAll();
    }

    NetworkResult wait() {
        QMutexLocker locker(&mutex);
        while (!finished) {
            condition.wait(&mutex);
        }
        return storedResult;
    }

private:
    QMutex mutex;
    QWaitCondition condition;
    NetworkResult storedResult;
    bool finished = false;
};

}  // namespace

NetworkClient::NetworkClient() {
    m_networkThread.setObjectName(QStringLiteral("EasyKiConverterNetworkThread"));
    m_networkThread.start();

    m_asyncNetworkManager = new QNetworkAccessManager();
    m_asyncNetworkManager->moveToThread(&m_networkThread);
}

NetworkClient::~NetworkClient() {
    QList<QPointer<AsyncNetworkRequest>> pendingRequests;
    {
        QMutexLocker locker(&m_asyncQueueMutex);
        for (const PendingAsyncRequest& pending : std::as_const(m_pendingAsyncRequests)) {
            pendingRequests.append(pending.request);
        }
        m_pendingAsyncRequests.clear();
    }

    for (const QPointer<AsyncNetworkRequest>& request : pendingRequests) {
        if (!request) {
            continue;
        }
        request->cancel();
        QMetaObject::invokeMethod(request, &QObject::deleteLater, Qt::QueuedConnection);
    }

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

AsyncNetworkRequest* NetworkClient::getAsync(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy) {
    return enqueueAsyncRequest(url, QByteArray(), resourceType, policy);
}

AsyncNetworkRequest* NetworkClient::postAsync(const QUrl& url,
                                              const QByteArray& body,
                                              ResourceType resourceType,
                                              const RetryPolicy& policy) {
    return enqueueAsyncRequest(url, body, resourceType, policy);
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
        updateStatsForEnqueuedRequest(resourceType);
    }

    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        this,
        [this, resourceType](const NetworkResult& result) {
            updateStatsForCompletedRequest(resourceType, result);
            onAsyncRequestFinished(resourceType);
        },
        Qt::QueuedConnection);

    scheduleAsyncPump();
    return request;
}

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
