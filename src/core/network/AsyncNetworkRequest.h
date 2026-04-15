#ifndef ASYNCNETWORKREQUEST_H
#define ASYNCNETWORKREQUEST_H

#include "INetworkClient.h"

#include <QAtomicInt>
#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>

namespace EasyKiConverter {

/**
 * @brief Async network request that supports cancellation
 *
 * Supports:
 * - Cancellation via cancel()
 * - Progress tracking
 * - Automatic retry with exponential backoff
 * - Gzip decompression
 * - POST requests with body data
 *
 * Usage:
 *   auto* request = client->getAsync(url, policy);
 *   connect(request, &AsyncNetworkRequest::finished, this, [](const NetworkResult& result) {
 *       if (result.wasCancelled) { ... }
 *   });
 *   // To cancel: request->cancel();
 */
class AsyncNetworkRequest : public QObject {
    Q_OBJECT

public:
    static AsyncNetworkRequest* createFinished(const NetworkResult& result, QObject* parent = nullptr);

    /**
     * @brief Destructor - ensures cleanup
     */
    ~AsyncNetworkRequest() override;

    /**
     * @brief Cancel the request
     *
     * Can be called from any thread. The request will abort as soon as possible.
     * The finished signal will be emitted with wasCancelled=true.
     */
    Q_INVOKABLE void cancel();

    /**
     * @brief Check if request has been cancelled
     */
    bool isCancelled() const;

    /**
     * @brief Check if request has completed (successfully or with error)
     */
    bool isFinished() const;

    /**
     * @brief Get the result
     *
     * Only valid after finished signal is emitted.
     */
    NetworkResult result() const;

    /**
     * @brief Set timeout in milliseconds
     *
     * Default is 30000ms (30 seconds).
     * Set to 0 to disable timeout.
     */
    void setTimeoutMs(int ms);

    /**
     * @brief Get current retry count
     */
    int currentRetryCount() const;

signals:
    /**
     * @brief Emitted when request completes (success, error, or cancelled)
     * @param result The result of the request
     */
    void finished(const NetworkResult& result);

    /**
     * @brief Emitted during download to report progress
     * @param bytesReceived Bytes received so far
     * @param bytesTotal Total bytes to receive (-1 if unknown)
     */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief Emitted when request times out
     */
    void timeout();

    /**
     * @brief Emitted when request is retried
     * @param retryCount Current retry count (0-based)
     * @param delayMs Delay before retry
     */
    void retrying(int retryCount, int delayMs);

private:
    friend class NetworkClient;

    /**
     * @brief Constructor - private, use NetworkClient::getAsync() or postAsync()
     */
    AsyncNetworkRequest(const QUrl& url,
                        QNetworkAccessManager* networkManager,
                        ResourceType resourceType,
                        const RetryPolicy& policy,
                        const QByteArray& body = QByteArray(),
                        QObject* parent = nullptr);

    /**
     * @brief Start the request
     */
    void start();

    /**
     * @brief Start a single attempt (may be called multiple times for retries)
     */
    void startAttempt(int attemptNumber);

    /**
     * @brief Handle attempt completion
     */
    void handleAttemptFinished();

    /**
     * @brief Handle timeout
     */
    void handleTimeout();

    /**
     * @brief Process response data
     */
    void processResponse(QNetworkReply* reply);

    /**
     * @brief Schedule a retry
     */
    void scheduleRetry(int retryCount);

    /**
     * @brief Calculate retry delay with jitter
     */
    int calculateRetryDelay(int retryCount) const;

    /**
     * @brief Check if should retry based on status code
     */
    bool shouldRetryInternal(int statusCode, QNetworkReply::NetworkError error, int retryCount) const;

    /**
     * @brief Cleanup current attempt
     */
    void cleanupAttempt();

    QUrl m_url;
    QNetworkAccessManager* m_networkManager;
    ResourceType m_resourceType;
    RetryPolicy m_policy;
    QByteArray m_postBody;  ///< Request body for POST requests
    bool m_isPostRequest;  ///< Whether this is a POST request
    QAtomicInt m_cancelled;
    QAtomicInt m_finished;
    mutable QMutex m_resultMutex;
    NetworkResult m_result;

    QNetworkReply* m_currentReply;
    QTimer* m_timeoutTimer;
    int m_currentRetryCount;
    int m_timeoutMs;
    bool m_gzipUsed;

    QList<QNetworkReply*> m_pendingReplies;  // For cleanup on destruction
    mutable QMutex m_repliesMutex;

    static const QList<QNetworkReply*> s_emptyReplies;
};

}  // namespace EasyKiConverter

#endif  // ASYNCNETWORKREQUEST_H
