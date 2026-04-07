#ifndef INETWORKCLIENT_H
#define INETWORKCLIENT_H

#include <QByteArray>
#include <QUrl>

namespace EasyKiConverter {

/**
 * @brief Retry policy for network requests
 */
struct RetryPolicy {
    int maxRetries = 3;
    int baseTimeoutMs = 30000;
    double jitterFactor = 0.2;
    std::vector<int> delays = {1000, 2000, 4000};  // Exponential backoff

    // Status codes that should trigger retry
    std::vector<int> retryableStatusCodes = {429, 500, 502, 503, 504};
};

/**
 * @brief Result of a network request
 */
struct NetworkResult {
    QByteArray data;
    int statusCode = 0;
    QString error;
    int retryCount = 0;
    qint64 elapsedMs = 0;
    bool success = false;
};

/**
 * @brief Network client interface
 *
 * Provides a unified interface for network requests with consistent
 * timeout/retry/backoff behavior across all network components.
 */
class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    /**
     * @brief Send HTTP GET request
     * @param url Request URL
     * @param policy Retry policy (default: 3 retries, 30s timeout)
     * @return NetworkResult with response data or error
     */
    virtual NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) = 0;

    /**
     * @brief Send HTTP POST request
     * @param url Request URL
     * @param body Request body
     * @param policy Retry policy
     * @return NetworkResult with response data or error
     */
    virtual NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) = 0;
};

}  // namespace EasyKiConverter

#endif  // INETWORKCLIENT_H
