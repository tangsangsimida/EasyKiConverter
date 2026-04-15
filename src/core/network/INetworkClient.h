#ifndef INETWORKCLIENT_H
#define INETWORKCLIENT_H

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QtGlobal>

namespace EasyKiConverter {

class AsyncNetworkRequest;

/**
 * @brief Resource type for network requests
 *
 * Used to select appropriate RequestProfile and for diagnostics
 */
enum class ResourceType {
    ComponentInfo,  // Component metadata JSON from EasyEDA API
    CadData,  // CAD data JSON (symbol, footprint)
    ProductSearch,  // LCSC product search / detail JSON
    PreviewImage,  // Component preview images
    Datasheet,  // PDF datasheets
    Model3DObj,  // 3D model OBJ files
    Model3DStep,  // 3D model STEP files
    UpdateCheck,  // GitHub release metadata
    WorkerRequest,  // Generic worker request (FetchWorker, NetworkWorker)
    Unknown
};

/**
 * @brief Request profile containing type-specific network policy
 *
 * Each profile defines timeout, retry, backoff, and concurrency settings
 * optimized for a specific type of resource.
 */
struct RequestProfile {
    ResourceType type = ResourceType::Unknown;
    QString name;  // Human-readable name for diagnostics

    int connectTimeoutMs = 30000;  // Connection timeout
    int readTimeoutMs = 30000;  // Read timeout (for large files)
    int maxRetries = 3;
    double backoffBaseMs = 1000.0;  // Base delay for exponential backoff
    double backoffMultiplier = 2.0;  // Multiplier for each retry
    double jitterFactor = 0.2;  // Jitter percentage (0.0-1.0)

    int weakNetworkMaxRetries = 5;  // More retries in weak network mode
    int weakNetworkTimeoutMultiplier = 2;  // Double timeout in weak network mode

    // Priority for scheduling (lower = higher priority)
    int priority = 100;

    // Concurrency limit for this resource type
    int maxConcurrent = 5;

    // Whether to enable request cancellation
    bool allowCancellation = true;

    // Whether this resource type is cacheable
    bool cacheable = false;
};

/**
 * @brief Predefined request profiles for common resource types
 */
struct RequestProfiles {
    static RequestProfile componentInfo() {
        RequestProfile profile;
        profile.type = ResourceType::ComponentInfo;
        profile.name = "ComponentInfo";
        profile.connectTimeoutMs = 15000;
        profile.readTimeoutMs = 30000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 1000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 10;
        profile.maxConcurrent = 10;
        profile.allowCancellation = true;
        profile.cacheable = true;
        return profile;
    }

    static RequestProfile cadData() {
        RequestProfile profile;
        profile.type = ResourceType::CadData;
        profile.name = "CadData";
        profile.connectTimeoutMs = 15000;
        profile.readTimeoutMs = 45000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 1000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 10;
        profile.maxConcurrent = 5;
        profile.allowCancellation = true;
        profile.cacheable = true;
        return profile;
    }

    static RequestProfile previewImage() {
        RequestProfile profile;
        profile.type = ResourceType::PreviewImage;
        profile.name = "PreviewImage";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 40000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 50;
        profile.maxConcurrent = 5;
        profile.allowCancellation = true;
        profile.cacheable = true;
        return profile;
    }

    static RequestProfile productSearch() {
        RequestProfile profile;
        profile.type = ResourceType::ProductSearch;
        profile.name = "ProductSearch";
        profile.connectTimeoutMs = 15000;
        profile.readTimeoutMs = 30000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 1000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 20;
        profile.maxConcurrent = 5;
        profile.allowCancellation = true;
        profile.cacheable = true;
        return profile;
    }

    static RequestProfile datasheet() {
        RequestProfile profile;
        profile.type = ResourceType::Datasheet;
        profile.name = "Datasheet";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 120000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 60;
        profile.maxConcurrent = 3;
        profile.allowCancellation = true;
        profile.cacheable = true;
        return profile;
    }

    static RequestProfile model3DObj() {
        RequestProfile profile;
        profile.type = ResourceType::Model3DObj;
        profile.name = "Model3DObj";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 120000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 40;
        profile.maxConcurrent = 3;
        profile.allowCancellation = true;
        profile.cacheable = true;
        return profile;
    }

    static RequestProfile model3DStep() {
        RequestProfile profile;
        profile.type = ResourceType::Model3DStep;
        profile.name = "Model3DStep";
        profile.connectTimeoutMs = 20000;
        profile.readTimeoutMs = 120000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 2000.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.3;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 30;
        profile.maxConcurrent = 3;
        profile.allowCancellation = true;
        profile.cacheable = true;
        return profile;
    }

    static RequestProfile workerRequest() {
        RequestProfile profile;
        profile.type = ResourceType::WorkerRequest;
        profile.name = "WorkerRequest";
        profile.connectTimeoutMs = 30000;
        profile.readTimeoutMs = 45000;
        profile.maxRetries = 3;
        profile.backoffBaseMs = 3000.0;
        profile.backoffMultiplier = 1.7;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 5;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 20;
        profile.maxConcurrent = 5;
        profile.allowCancellation = true;
        profile.cacheable = false;
        return profile;
    }

    static RequestProfile updateCheck() {
        RequestProfile profile;
        profile.type = ResourceType::UpdateCheck;
        profile.name = "UpdateCheck";
        profile.connectTimeoutMs = 10000;
        profile.readTimeoutMs = 15000;
        profile.maxRetries = 2;
        profile.backoffBaseMs = 1500.0;
        profile.backoffMultiplier = 2.0;
        profile.jitterFactor = 0.2;
        profile.weakNetworkMaxRetries = 3;
        profile.weakNetworkTimeoutMultiplier = 2;
        profile.priority = 80;
        profile.maxConcurrent = 2;
        profile.allowCancellation = true;
        profile.cacheable = false;
        return profile;
    }

    static RequestProfile fromType(ResourceType type) {
        switch (type) {
            case ResourceType::ComponentInfo:
                return componentInfo();
            case ResourceType::CadData:
                return cadData();
            case ResourceType::ProductSearch:
                return productSearch();
            case ResourceType::PreviewImage:
                return previewImage();
            case ResourceType::Datasheet:
                return datasheet();
            case ResourceType::Model3DObj:
                return model3DObj();
            case ResourceType::Model3DStep:
                return model3DStep();
            case ResourceType::UpdateCheck:
                return updateCheck();
            case ResourceType::WorkerRequest:
                return workerRequest();
            default:
                return componentInfo();
        }
    }
};

/**
 * @brief Error type classification for diagnostics
 */
enum class NetworkErrorType {
    None,
    Timeout,
    ConnectionRefused,
    ConnectionReset,
    HostNotFound,
    SSLError,
    RateLimited,  // 429
    ServerError,  // 5xx
    NotFound,  // 404
    Forbidden,  // 403
    Canceled,
    DecompressionFailed,
    Other
};

/**
 * @brief Unified diagnostic information for network requests
 */
struct NetworkDiagnostic {
    QString url;
    QString host;
    ResourceType resourceType = ResourceType::Unknown;
    QString profileName;
    int statusCode = 0;
    NetworkErrorType errorType = NetworkErrorType::None;
    QString errorMessage;

    int retryCount = 0;
    qint64 latencyMs = 0;
    qint64 totalElapsedMs = 0;

    bool wasRateLimited = false;
    bool wasCanceled = false;
    bool usedCache = false;

    // Weak network detection
    bool isWeakNetwork = false;
    double weakNetworkScore = 0.0;  // 0.0 = perfect, 1.0 = very poor

    // Timing breakdown
    qint64 dnsLookupMs = 0;
    qint64 connectMs = 0;
    qint64 tlsHandshakeMs = 0;
    qint64 waitMs = 0;  // Time waiting for response
    qint64 transferMs = 0;  // Time for data transfer

    /**
     * @brief Convert error type to string for logging
     */
    static QString errorTypeToString(NetworkErrorType type) {
        switch (type) {
            case NetworkErrorType::None:
                return "None";
            case NetworkErrorType::Timeout:
                return "Timeout";
            case NetworkErrorType::ConnectionRefused:
                return "ConnectionRefused";
            case NetworkErrorType::ConnectionReset:
                return "ConnectionReset";
            case NetworkErrorType::HostNotFound:
                return "HostNotFound";
            case NetworkErrorType::SSLError:
                return "SSLError";
            case NetworkErrorType::RateLimited:
                return "RateLimited";
            case NetworkErrorType::ServerError:
                return "ServerError";
            case NetworkErrorType::NotFound:
                return "NotFound";
            case NetworkErrorType::Forbidden:
                return "Forbidden";
            case NetworkErrorType::Canceled:
                return "Canceled";
            case NetworkErrorType::DecompressionFailed:
                return "DecompressionFailed";
            default:
                return "Other";
        }
    }
};

/**
 * @brief Retry policy for network requests
 */
struct RetryPolicy {
    int maxRetries = 3;
    int baseTimeoutMs = 30000;
    double jitterFactor = 0.2;
    QVector<int> delays = {1000, 2000, 4000};  // Exponential backoff

    // Status codes that should trigger retry
    QVector<int> retryableStatusCodes = {429, 500, 502, 503, 504};

    /**
     * @brief Create RetryPolicy from RequestProfile
     */
    static RetryPolicy fromProfile(const RequestProfile& profile, bool isWeakNetwork = false) {
        RetryPolicy policy;
        const int baseTimeout = qMax(profile.connectTimeoutMs, profile.readTimeoutMs);
        const int timeout = isWeakNetwork ? baseTimeout * profile.weakNetworkTimeoutMultiplier : baseTimeout;
        policy.baseTimeoutMs = timeout;
        policy.maxRetries = isWeakNetwork ? profile.weakNetworkMaxRetries : profile.maxRetries;
        policy.jitterFactor = profile.jitterFactor;

        // Generate backoff delays
        policy.delays.clear();
        double delay = profile.backoffBaseMs;
        for (int i = 0; i < policy.maxRetries; ++i) {
            policy.delays.append(static_cast<int>(delay));
            delay *= profile.backoffMultiplier;
        }

        return policy;
    }
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
    bool wasCancelled = false;  // true if request was cancelled via abort()
    NetworkDiagnostic diagnostic;  // Unified diagnostic information
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
     * @brief Send HTTP GET request with resource type for profiling
     * @param url Request URL
     * @param resourceType Type of resource being requested
     * @param policy Retry policy
     * @return NetworkResult with response data or error
     */
    virtual NetworkResult get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) = 0;

    /**
     * @brief Send HTTP POST request
     * @param url Request URL
     * @param body Request body
     * @param policy Retry policy
     * @return NetworkResult with response data or error
     */
    virtual NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) = 0;

    /**
     * @brief Send HTTP POST request with resource type for profiling
     * @param url Request URL
     * @param body Request body
     * @param resourceType Type of resource being requested
     * @param policy Retry policy
     * @return NetworkResult with response data or error
     */
    virtual NetworkResult post(const QUrl& url,
                               const QByteArray& body,
                               ResourceType resourceType,
                               const RetryPolicy& policy = {}) = 0;

    /**
     * @brief Send HTTP GET request asynchronously
     */
    virtual AsyncNetworkRequest* getAsync(const QUrl& url,
                                          ResourceType resourceType,
                                          const RetryPolicy& policy = {}) = 0;

    /**
     * @brief Send HTTP POST request asynchronously
     */
    virtual AsyncNetworkRequest* postAsync(const QUrl& url,
                                           const QByteArray& body,
                                           ResourceType resourceType,
                                           const RetryPolicy& policy = {}) = 0;
};

}  // namespace EasyKiConverter

#endif  // INETWORKCLIENT_H
