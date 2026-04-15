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
 * @brief 单例网络客户端，统一管理重试/退避行为
 */
class NetworkClient : public QObject, public INetworkClient, public TestableSingleton<NetworkClient> {
    Q_OBJECT
    friend class TestableSingleton<NetworkClient>;

public:
    /**
     * @brief 获取单例实例
     */
    using TestableSingleton<NetworkClient>::instance;

    /**
     * @brief 发送 HTTP GET 请求（同步，阻塞直到完成）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP GET 请求（带资源类型，用于配置Profile）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    NetworkResult get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP POST 请求（同步，阻塞直到完成）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP POST 请求（带资源类型，用于配置Profile）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用。
     *       请勿在 bare std::thread 中调用。
     */
    NetworkResult post(const QUrl& url,
                       const QByteArray& body,
                       ResourceType resourceType,
                       const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP GET 请求（异步，非阻塞）
     *
     * 返回的 AsyncNetworkRequest 由调用方管理生命周期。
     * 调用方应连接 finished() 信号并在完成后删除请求。
     * 可通过 AsyncNetworkRequest::cancel() 随时取消请求。
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用
     *       （如主线程或 QThread 子类）。请勿在 bare std::thread 中调用。
     *
     * @param url 请求 URL
     * @param resourceType 资源类型
     * @param policy 重试策略
     * @return AsyncNetworkRequest* - 调用方负责生命周期管理
     */
    Q_INVOKABLE AsyncNetworkRequest* getAsync(const QUrl& url,
                                              ResourceType resourceType = ResourceType::Unknown,
                                              const RetryPolicy& policy = {}) override;

    /**
     * @brief 发送 HTTP POST 请求（异步，非阻塞）
     *
     * @note 此方法使用 thread-local QNetworkAccessManager，必须在具有 Qt 事件循环的线程中调用
     *       （如主线程或 QThread 子类）。请勿在 bare std::thread 中调用。
     *
     * @param url 请求 URL
     * @param body 请求体
     * @param resourceType 资源类型
     * @param policy 重试策略
     * @return AsyncNetworkRequest* - 调用方负责生命周期管理
     */
    Q_INVOKABLE AsyncNetworkRequest* postAsync(const QUrl& url,
                                               const QByteArray& body,
                                               ResourceType resourceType = ResourceType::Unknown,
                                               const RetryPolicy& policy = {}) override;

    /**
     * @brief 检查数据是否为 gzip 压缩格式
     */
    static bool isGzipCompressed(const QByteArray& data);

    /**
     * @brief 解压 gzip 数据
     */
    static QByteArray decompressGzip(const QByteArray& data);

    /**
     * @brief 销毁单例实例
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
