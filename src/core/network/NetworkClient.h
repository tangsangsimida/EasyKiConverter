#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include "AsyncNetworkRequest.h"
#include "INetworkClient.h"
#include "utils/TestableSingleton.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRandomGenerator>

namespace EasyKiConverter {

/**
 * @brief Singleton network client with unified retry/backoff behavior
 */
class NetworkClient : public QObject, public INetworkClient, public TestableSingleton<NetworkClient> {
    Q_OBJECT
    friend class TestableSingleton<NetworkClient>;

public:
    /**
     * @brief Get singleton instance
     */
    using TestableSingleton<NetworkClient>::instance;

    /**
     * @brief Send HTTP GET request with retry (synchronous, blocks until complete)
     */
    NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) override;

    /**
     * @brief Send HTTP GET request with resource type for profiling
     */
    NetworkResult get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) override;

    /**
     * @brief Send HTTP POST request with retry (synchronous, blocks until complete)
     */
    NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) override;

    /**
     * @brief Send HTTP POST request with resource type for profiling
     */
    NetworkResult post(const QUrl& url,
                       const QByteArray& body,
                       ResourceType resourceType,
                       const RetryPolicy& policy = {}) override;

    /**
     * @brief Send HTTP GET request asynchronously (non-blocking)
     *
     * The returned AsyncNetworkRequest must be managed by the caller.
     * The caller should connect to finished() signal and delete the request when done.
     * The request can be cancelled at any time via AsyncNetworkRequest::cancel().
     *
     * @param url Request URL
     * @param resourceType Type of resource being requested
     * @param policy Retry policy
     * @return AsyncNetworkRequest* - caller takes ownership
     */
    Q_INVOKABLE AsyncNetworkRequest* getAsync(const QUrl& url,
                                              ResourceType resourceType = ResourceType::Unknown,
                                              const RetryPolicy& policy = {}) override;

    /**
     * @brief Send HTTP POST request asynchronously (non-blocking)
     *
     * @param url Request URL
     * @param body Request body
     * @param resourceType Type of resource being requested
     * @param policy Retry policy
     * @return AsyncNetworkRequest* - caller takes ownership
     */
    Q_INVOKABLE AsyncNetworkRequest* postAsync(const QUrl& url,
                                               const QByteArray& body,
                                               ResourceType resourceType = ResourceType::Unknown,
                                               const RetryPolicy& policy = {}) override;

    /**
     * @brief Check if data is gzip compressed
     */
    static bool isGzipCompressed(const QByteArray& data);

    /**
     * @brief Decompress gzip data
     */
    static QByteArray decompressGzip(const QByteArray& data);

    /**
     * @brief Destroy singleton instance
     */
    using TestableSingleton<NetworkClient>::destroyInstance;

protected:
    NetworkClient();
    ~NetworkClient() override;

private:
    NetworkResult executeRequest(const QUrl& url,
                                 const QByteArray& body,
                                 ResourceType resourceType,
                                 const RetryPolicy& policy);
    int calculateRetryDelay(int retryCount, const RetryPolicy& policy);
    bool shouldRetry(int statusCode, QNetworkReply::NetworkError error, int retryCount, const RetryPolicy& policy);
    void populateDiagnostic(NetworkDiagnostic& diag, const QUrl& url, ResourceType resourceType);
};

}  // namespace EasyKiConverter

#endif  // NETWORKCLIENT_H
