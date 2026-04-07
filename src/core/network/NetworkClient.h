#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include "INetworkClient.h"
#include "utils/TestableSingleton.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRandomGenerator>
#include <QTimer>

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
     * @brief Send HTTP GET request with retry
     */
    NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) override;

    /**
     * @brief Send HTTP POST request with retry
     */
    NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) override;

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
                                const RetryPolicy& policy);
    int calculateRetryDelay(int retryCount, const RetryPolicy& policy);
    bool shouldRetry(int statusCode, int retryCount, const RetryPolicy& policy);

    QNetworkAccessManager* m_networkManager;
};

}  // namespace EasyKiConverter

#endif  // NETWORKCLIENT_H
