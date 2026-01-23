#ifndef NETWORKUTILS_H
#define NETWORKUTILS_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QTimer>

namespace EasyKiConverter
{

    /**
     * @brief 网络工具�?
     *
     * 提供带重试机制的网络请求功能
     */
    class NetworkUtils : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函�?
         *
         * @param parent 父对�?
         */
        explicit NetworkUtils(QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~NetworkUtils() override;

        /**
         * @brief 发�?HTTP GET 请求
         *
         * @param url 请求 URL
         * @param timeout 超时时间（秒），默认 30 �?
         * @param maxRetries 最大重试次数，默认 3 �?
         */
        void sendGetRequest(const QString &url, int timeout = 30, int maxRetries = 3);

        /**
         * @brief 取消当前请求
         */
        void cancelRequest();

        /**
         * @brief 设置请求�?
         *
         * @param key 头字段名
         * @param value 头字段�?
         */
        void setHeader(const QString &key, const QString &value);

        /**
         * @brief 清除所有请求头
         */
        void clearHeaders();

        /**
         * @brief 设置是否期望接收二进制数�?
         *
         * @param expectBinaryData 是否期望接收二进制数�?
         */
        void setExpectBinaryData(bool expectBinaryData);

    signals:
        /**
         * @brief 请求成功信号
         *
         * @param data 响应数据（JSON 格式�?
         */
        void requestSuccess(const QJsonObject &data);

        /**
         * @brief 二进制数据获取成功信�?
         *
         * @param binaryData 二进制数�?
         */
        void binaryDataFetched(const QByteArray &binaryData);

        /**
         * @brief 请求失败信号
         *
         * @param errorMessage 错误消息
         */
        void requestError(const QString &errorMessage);

        /**
         * @brief 请求进度信号
         *
         * @param bytesReceived 已接收字节数
         * @param bytesTotal 总字节数
         */
        void requestProgress(qint64 bytesReceived, qint64 bytesTotal);

    private slots:
        /**
         * @brief 处理网络响应
         */
        void handleResponse();

        /**
         * @brief 处理网络错误
         */
        void handleError(QNetworkReply::NetworkError error);

        /**
         * @brief 处理请求超时
         */
        void handleTimeout();

    private:
        /**
         * @brief 执行请求
         */
        void executeRequest();

        /**
         * @brief 重试请求
         */
        void retryRequest();

        /**
         * @brief 检查是否需要重�?
         *
         * @param statusCode HTTP 状态码
         * @return bool 是否需要重�?
         */
        bool shouldRetry(int statusCode);

        /**
         * @brief 计算重试延迟时间
         *
         * @param retryCount 当前重试次数
         * @return int 延迟时间（毫秒）
         */
        int calculateRetryDelay(int retryCount);

        /**
         * @brief 解压 gzip 数据
         *
         * @param compressedData 压缩的数�?
         * @return QByteArray 解压后的数据
         */
        QByteArray decompressGzip(const QByteArray &compressedData);

    private:
        QNetworkAccessManager *m_networkManager;
        QNetworkReply *m_currentReply;
        QTimer *m_timeoutTimer;
        QString m_url;
        int m_timeout;
        int m_maxRetries;
        int m_retryCount;
        QMap<QString, QString> m_headers;
        bool m_isRequesting;
        bool m_expectBinaryData; // 是否期望接收二进制数�?
    };

} // namespace EasyKiConverter

#endif // NETWORKUTILS_H
