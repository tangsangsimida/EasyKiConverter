#ifndef ASYNCNETWORKREQUEST_H
#define ASYNCNETWORKREQUEST_H

#include "core/network/INetworkClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

// Forward declaration for friend class
namespace EasyKiConverter::Test {
class MockNetworkClient;
}

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
 * @brief 异步网络请求，支持取消
 *
 * 支持：
 * - 通过 cancel() 取消请求
 * - 进度跟踪
 * - 自动重试与指数退避
 * - Gzip 解压
 * - 带请求体的 POST 请求
 *
 * 使用示例：
 *   auto* request = client->getAsync(url, policy);
 *   connect(request, &AsyncNetworkRequest::finished, this, [](const NetworkResult& result) {
 *       if (result.wasCancelled) { ... }
 *   });
 *   // 取消请求：request->cancel();
 */
class AsyncNetworkRequest : public QObject {
    Q_OBJECT

public:
    static AsyncNetworkRequest* createFinished(const NetworkResult& result, QObject* parent = nullptr);

    /**
     * @brief 延迟完成请求 - 用于测试场景
     * @param result 要发送的结果
     * @param delayMs 延迟毫秒数
     *
     * 此方法用于模拟异步网络请求，避免信号在槽连接建立前发出
     */
    Q_INVOKABLE void completeWithResultDelayed(const NetworkResult& result, int delayMs = 0);

    /**
     * @brief 析构函数，确保清理
     */
    ~AsyncNetworkRequest() override;

    /**
     * @brief 取消请求
     *
     * Can be called from any thread. The request will abort as soon as possible.
     * The finished signal will be emitted with wasCancelled=true.
     */
    Q_INVOKABLE void cancel();

    /**
     * @brief 检查请求是否已取消
     */
    bool isCancelled() const;

    /**
     * @brief 检查请求是否已完成 (successfully or with error)
     */
    bool isFinished() const;

    /**
     * @brief 获取结果
     *
     * Only valid after finished signal is emitted.
     */
    NetworkResult result() const;

    /**
     * @brief 设置超时时间（毫秒）
     *
     * Default is 30000ms (30 seconds).
     * Set to 0 to disable timeout.
     */
    void setTimeoutMs(int ms);

    /**
     * @brief 获取当前重试次数
     */
    int currentRetryCount() const;

signals:
    /**
     * @brief 请求完成时发射（成功、错误或取消） (success, error, or cancelled)
     * @param result The result of the request
     */
    void finished(const NetworkResult& result);

    /**
     * @brief 下载过程中发射以报告进度 to report progress
     * @param bytesReceived Bytes received so far
     * @param bytesTotal Total bytes to receive (-1 if unknown)
     */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief 请求超时时发射
     */
    void timeout();

    /**
     * @brief 请求重试时发射
     * @param retryCount Current retry count (0-based)
     * @param delayMs Delay before retry
     */
    void retrying(int retryCount, int delayMs);

private:
    friend class NetworkClient;
    friend class Test::MockNetworkClient;

    /**
     * @brief 构造函数 - 私有，使用 NetworkClient::getAsync() 或 postAsync()
     */
    AsyncNetworkRequest(const QUrl& url,
                        QNetworkAccessManager* networkManager,
                        ResourceType resourceType,
                        const RetryPolicy& policy,
                        const QByteArray& body = QByteArray(),
                        QObject* parent = nullptr);

    /**
     * @brief 启动请求
     */
    void start();

    /**
     * @brief 启动单次尝试（重试时会多次调用） (may be called multiple times for retries)
     */
    void startAttempt(int attemptNumber);

    /**
     * @brief 处理尝试完成
     */
    void handleAttemptFinished();

    /**
     * @brief 处理超时
     */
    void handleTimeout();

    /**
     * @brief 处理响应数据
     */
    void processResponse(QNetworkReply* reply);

    /**
     * @brief 调度重试
     */
    void scheduleRetry(int retryCount);

    /**
     * @brief 计算带抖动的重试延迟
     */
    int calculateRetryDelay(int retryCount) const;

    /**
     * @brief 检查是否应该根据状态码重试
     */
    bool shouldRetryInternal(int statusCode, QNetworkReply::NetworkError error, int retryCount) const;

    /**
     * @brief 清理当前尝试
     */
    void cleanupAttempt();
    bool completeWithResult(const NetworkResult& result);
    NetworkResult cancelledResult(const QString& errorMessage = QStringLiteral("Cancelled")) const;
    NetworkResult timeoutResult() const;
    bool hasActiveReply() const;

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
    QAtomicInt m_currentAttemptTimedOut;

    QList<QNetworkReply*> m_pendingReplies;  // For cleanup on destruction
    mutable QMutex m_repliesMutex;

    static const QList<QNetworkReply*> s_emptyReplies;
};

}  // namespace EasyKiConverter

#endif  // ASYNCNETWORKREQUEST_H
