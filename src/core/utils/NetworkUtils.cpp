#include "NetworkUtils.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QBuffer>
#include <QByteArray>
#include <zlib.h>

namespace EasyKiConverter
{

    NetworkUtils::NetworkUtils(QObject *parent)
        : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_currentReply(nullptr), m_timeoutTimer(new QTimer(this)), m_timeout(30), m_maxRetries(3), m_retryCount(0), m_isRequesting(false), m_expectBinaryData(false)
    {
        // 设置默认请求�?
        m_headers["Accept-Encoding"] = "gzip, deflate";
        m_headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
        m_headers["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
        m_headers["User-Agent"] = "EasyKiConverter/1.0.0";

        // 连接超时定时�?
        connect(m_timeoutTimer, &QTimer::timeout, this, &NetworkUtils::handleTimeout);
    }

    NetworkUtils::~NetworkUtils()
    {
        cancelRequest();
    }

    void NetworkUtils::sendGetRequest(const QString &url, int timeout, int maxRetries)
    {
        qDebug() << "sendGetRequest called - URL:" << url << "m_isRequesting:" << m_isRequesting << "m_currentReply:" << (m_currentReply != nullptr);

        if (m_isRequesting)
        {
            // 检查状态是否不一致：m_isRequesting �?true，但没有活跃的请�?
            if (!m_currentReply && !m_timeoutTimer->isActive())
            {
                qWarning() << "Inconsistent state detected: m_isRequesting is true but no active request. Resetting state.";
                m_isRequesting = false;
            }
            else
            {
                qWarning() << "A request is already in progress - m_currentReply:" << (m_currentReply != nullptr) << "m_timeoutTimer active:" << m_timeoutTimer->isActive();
                return;
            }
        }

        m_url = url;
        m_timeout = timeout;
        m_maxRetries = maxRetries;
        m_retryCount = 0;
        m_isRequesting = true;

        qDebug() << "sendGetRequest - Setting m_isRequesting to true, calling executeRequest()";
        executeRequest();
    }

    void NetworkUtils::cancelRequest()
    {
        if (m_currentReply)
        {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }

        if (m_timeoutTimer->isActive())
        {
            m_timeoutTimer->stop();
        }

        m_isRequesting = false;
    }

    void NetworkUtils::setHeader(const QString &key, const QString &value)
    {
        m_headers[key] = value;
    }

    void NetworkUtils::clearHeaders()
    {
        m_headers.clear();
    }

    void NetworkUtils::setExpectBinaryData(bool expectBinaryData)
    {
        m_expectBinaryData = expectBinaryData;
    }

    void NetworkUtils::executeRequest()
    {
        // 创建网络请求
        QNetworkRequest request{QUrl(m_url)};

        // 设置请求�?
        for (auto it = m_headers.constBegin(); it != m_headers.constEnd(); ++it)
        {
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }

        // 发�?GET 请求
        m_currentReply = m_networkManager->get(request);

        // 连接信号
        connect(m_currentReply, &QNetworkReply::finished, this, &NetworkUtils::handleResponse);
        connect(m_currentReply, &QNetworkReply::errorOccurred, this, &NetworkUtils::handleError);
        connect(m_currentReply, &QNetworkReply::downloadProgress, this, &NetworkUtils::requestProgress);

        // 启动超时定时�?
        m_timeoutTimer->start(m_timeout * 1000);

        qDebug() << "Sending GET request to:" << m_url << "(Retry:" << m_retryCount << "/" << m_maxRetries << ")";
    }

    void NetworkUtils::handleResponse()
    {
        // 停止超时定时�?
        m_timeoutTimer->stop();

        if (!m_currentReply)
        {
            m_isRequesting = false;
            return;
        }

        // 检�?HTTP 状态码
        int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "HTTP Status Code:" << statusCode;

        // 检查是否需要重�?
        if (shouldRetry(statusCode) && m_retryCount < m_maxRetries)
        {
            retryRequest();
            return;
        }

        // 读取响应数据
        QByteArray responseData = m_currentReply->readAll();

        // 清理网络回复
        m_currentReply->deleteLater();
        m_currentReply = nullptr;

        // 检�?HTTP 状态码
        if (statusCode != 200)
        {
            QString errorMsg = QString("HTTP request failed with status code: %1").arg(statusCode);
            qWarning() << errorMsg;
            emit requestError(errorMsg);
            m_isRequesting = false;
            return;
        }

        // 期望接收二进制数据，检查是否是 gzip 压缩的数据并解压�?
        if (m_expectBinaryData)
        {
            // 检查是否是 gzip 压缩的数�?
            if (responseData.size() >= 2 &&
                static_cast<unsigned char>(responseData[0]) == 0x1F &&
                static_cast<unsigned char>(responseData[1]) == 0x8B)
            {
                qDebug() << "Binary data is gzip compressed, decompressing...";
                qDebug() << "Compressed size:" << responseData.size();
                responseData = decompressGzip(responseData);
                qDebug() << "Decompressed size:" << responseData.size();
            }
            qDebug() << "Binary data fetched, final size:" << responseData.size();
            qDebug() << "First 100 chars (hex):" << responseData.left(100).toHex();
            qDebug() << "First 100 chars (raw):" << QString::fromLatin1(responseData.left(100));
            emit binaryDataFetched(responseData);
            m_isRequesting = false;
            return;
        }

        // 检查是否是 gzip 压缩的数�?
        if (responseData.size() >= 2 &&
            static_cast<unsigned char>(responseData[0]) == 0x1F &&
            static_cast<unsigned char>(responseData[1]) == 0x8B)
        {
            qDebug() << "Response is gzip compressed, decompressing...";
            responseData = decompressGzip(responseData);
        }

        // 输出响应数据的前 1000 个字符用于调�?
        qDebug() << "Response data (first 1000 chars):" << responseData.left(1000);

        // 解析 JSON 响应
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            QString errorMsg = QString("Failed to parse JSON response: %1").arg(parseError.errorString());
            qWarning() << errorMsg;
            emit requestError(errorMsg);
            m_isRequesting = false;
            return;
        }

        if (!jsonDoc.isObject())
        {
            QString errorMsg = "JSON response is not an object";
            qWarning() << errorMsg;
            emit requestError(errorMsg);
            m_isRequesting = false;
            return;
        }

        QJsonObject jsonObject = jsonDoc.object();

        // 检查响应是否为�?
        if (jsonObject.isEmpty())
        {
            QString errorMsg = "JSON response is empty";
            qWarning() << errorMsg;
            emit requestError(errorMsg);
            m_isRequesting = false;
            return;
        }

        // 检查响应是否包含错误信�?
        if (jsonObject.contains("success") && jsonObject["success"].toBool() == false)
        {
            QString errorMsg = QString("API returned error: %1").arg(QJsonDocument(jsonObject).toJson(QJsonDocument::Compact));
            qWarning() << errorMsg;
            emit requestError(errorMsg);
            m_isRequesting = false;
            return;
        }

        // 发送成功信�?
        qDebug() << "handleResponse - Setting m_isRequesting to false before emitting requestSuccess, m_isRequesting:" << m_isRequesting;
        m_isRequesting = false;
        emit requestSuccess(jsonObject);
        qDebug() << "handleResponse - requestSuccess emitted, m_isRequesting:" << m_isRequesting;
    }

    void NetworkUtils::handleError(QNetworkReply::NetworkError error)
    {
        Q_UNUSED(error)

        // 停止超时定时�?
        m_timeoutTimer->stop();

        if (!m_currentReply)
        {
            m_isRequesting = false;
            return;
        }

        QString errorMsg = QString("Network error: %1").arg(m_currentReply->errorString());
        qWarning() << errorMsg;

        // 清理网络回复
        m_currentReply->deleteLater();
        m_currentReply = nullptr;

        // 如果�?操作被取�?错误（由abort()触发），说明超时已经处理了，不需要重�?
        if (error == QNetworkReply::OperationCanceledError)
        {
            // 超时已经由handleTimeout处理，这里不需要重�?
            qWarning() << "Request was cancelled, timeout handler will handle retry";
            return;
        }

        // 检查是否需要重�?
        if (m_retryCount < m_maxRetries)
        {
            retryRequest();
            return;
        }

        // 发送失败信�?
        emit requestError(errorMsg);
        m_isRequesting = false;
    }

    void NetworkUtils::handleTimeout()
    {
        qWarning() << "Request timeout after" << m_timeout << "seconds";

        // 取消请求
        if (m_currentReply)
        {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }

        // 检查是否需要重�?
        if (m_retryCount < m_maxRetries)
        {
            // 保持 m_isRequesting �?true，因为重试仍在进行中
            retryRequest();
            return;
        }

        // 不再重试，发送失败信号并重置标志
        emit requestError("Request timeout");
        m_isRequesting = false;
    }

    void NetworkUtils::retryRequest()
    {
        m_retryCount++;

        // 计算重试延迟
        int delay = calculateRetryDelay(m_retryCount);
        qDebug() << "Retrying request in" << delay << "ms (Retry:" << m_retryCount << "/" << m_maxRetries << ")";

        // 延迟后重�?
        QTimer::singleShot(delay, this, &NetworkUtils::executeRequest);
    }

    bool NetworkUtils::shouldRetry(int statusCode)
    {
        // 需要重试的 HTTP 状态码
        // 429: Too Many Requests
        // 500: Internal Server Error
        // 502: Bad Gateway
        // 503: Service Unavailable
        // 504: Gateway Timeout
        return (statusCode == 429 || statusCode == 500 || statusCode == 502 ||
                statusCode == 503 || statusCode == 504);
    }

    int NetworkUtils::calculateRetryDelay(int retryCount)
    {
        // 指数退避算法：delay = base_delay * (2 ^ (retry_count - 1))
        // 基础延迟�?1 �?
        return 1000 * static_cast<int>(std::pow(2, retryCount - 1));
    }

    QByteArray NetworkUtils::decompressGzip(const QByteArray &compressedData)
    {
        // 检查是否是 gzip 格式
        if (compressedData.size() < 2)
        {
            qWarning() << "Data too small to be gzip compressed";
            return compressedData;
        }

        // 检�?gzip 魔数数字 (0x1F, 0x8B)
        if (static_cast<unsigned char>(compressedData[0]) != 0x1F ||
            static_cast<unsigned char>(compressedData[1]) != 0x8B)
        {
            qWarning() << "Data is not gzip compressed";
            return compressedData;
        }

        // 使用 zlib 解压
        z_stream stream;
        memset(&stream, 0, sizeof(stream));

        // 初始�?zlib
        if (inflateInit2(&stream, 15 + 16) != Z_OK)
        { // 15 + 16 启用 gzip 解码
            qWarning() << "Failed to initialize zlib for gzip decompression";
            return compressedData;
        }

        stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(compressedData.constData()));
        stream.avail_in = compressedData.size();

        QByteArray decompressedData;
        const int chunkSize = 4096;
        char buffer[chunkSize];

        int ret;
        do
        {
            stream.next_out = reinterpret_cast<Bytef *>(buffer);
            stream.avail_out = chunkSize;

            ret = inflate(&stream, Z_NO_FLUSH);

            if (ret == Z_OK || ret == Z_STREAM_END)
            {
                decompressedData.append(buffer, chunkSize - stream.avail_out);
            }
            else
            {
                qWarning() << "Gzip decompression error:" << ret;
                inflateEnd(&stream);
                return compressedData;
            }
        } while (ret != Z_STREAM_END && stream.avail_in > 0);

        inflateEnd(&stream);

        if (ret != Z_STREAM_END)
        {
            qWarning() << "Gzip decompression did not complete successfully";
        }

        qDebug() << "Gzip decompression completed. Original size:" << compressedData.size()
                 << "Decompressed size:" << decompressedData.size();

        return decompressedData;
    }

} // namespace EasyKiConverter
