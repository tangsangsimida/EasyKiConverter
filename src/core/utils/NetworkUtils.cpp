#include "NetworkUtils.h"

#include <QBuffer>
#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QPointer>

// 包含 zlib.h（所有平台统一使用）
#include <zlib.h>

namespace EasyKiConverter {

NetworkUtils::NetworkUtils(QObject* parent)
    : INetworkAdapter(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_timeout(30)
    , m_maxRetries(3)
    , m_retryCount(0)
    , m_isRequesting(false)
    , m_expectBinaryData(false)
    , m_replyDeleting(false) {
    // 设置默认请求头
    m_headers["Accept-Encoding"] = "gzip, deflate";
    m_headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    m_headers["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
    m_headers["User-Agent"] = "EasyKiConverter/1.0.0";

    // 连接超时定时器
    connect(m_timeoutTimer, &QTimer::timeout, this, &NetworkUtils::handleTimeout);
}

NetworkUtils::~NetworkUtils() {
    cancelRequest();
}

void NetworkUtils::sendGetRequest(const QString& url, int timeout, int maxRetries) {
    qDebug() << "sendGetRequest called - URL:" << url << "m_isRequesting:" << m_isRequesting;

    if (m_currentReply != nullptr) {
        qWarning() << "A request is already in progress";
        return;
    }

    m_url = url;
    m_timeout = timeout;
    m_maxRetries = maxRetries;
    m_retryCount = 0;
    m_isRequesting = true;
    m_replyDeleting = false;  // 重置删除标志

    executeRequest();
}

void NetworkUtils::cancelRequest() {
    // 停止超时定时器，防止后续的竞态条件
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }

    // 防止重复调用 deleteLater（必须在访问 m_currentReply 之前检查）
    if (m_replyDeleting) {
        return;
    }

    if (!m_currentReply) {
        m_isRequesting = false;
        return;
    }

    // 标记为删除，防止重复处理
    m_replyDeleting = true;

    // 使用 QPointer 安全地持有指针
    QPointer<QNetworkReply> replyPtr(m_currentReply);

    // 清空成员变量
    m_currentReply = nullptr;
    m_isRequesting = false;

    // 如果对象还存在，安全地删除它
    if (replyPtr) {
        // 断开所有信号连接，防止后续事件触发
        replyPtr->disconnect();

        // 如果正在运行，先中止
        if (replyPtr->isRunning()) {
            replyPtr->abort();
        }

        // 安全地调用 deleteLater
        replyPtr->deleteLater();
    }
}

void NetworkUtils::setHeader(const QString& key, const QString& value) {
    m_headers[key] = value;
}

void NetworkUtils::clearHeaders() {
    m_headers.clear();
}

void NetworkUtils::setExpectBinaryData(bool expectBinaryData) {
    m_expectBinaryData = expectBinaryData;
}

void NetworkUtils::executeRequest() {
    // 重置删除标志，因为我们要创建新的 reply
    m_replyDeleting = false;

    QNetworkRequest request{QUrl(m_url)};

    for (auto it = m_headers.constBegin(); it != m_headers.constEnd(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    m_currentReply = m_networkManager->get(request);

    connect(m_currentReply, &QNetworkReply::finished, this, &NetworkUtils::handleResponse);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &NetworkUtils::handleError);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &NetworkUtils::requestProgress);

    m_timeoutTimer->start(m_timeout * 1000);
}

void NetworkUtils::handleResponse() {
    // 立即停止超时定时器，防止与 handleTimeout 竞态
    m_timeoutTimer->stop();

    // 防止重复处理（必须在访问 m_currentReply 之前检查）
    if (m_replyDeleting) {
        qWarning() << "Response: reply already marked for deletion";
        m_isRequesting = false;
        return;
    }

    if (!m_currentReply) {
        qWarning() << "Response: m_currentReply is null, possibly already handled by timeout";
        m_isRequesting = false;
        return;
    }

    int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (shouldRetry(statusCode) && m_retryCount < m_maxRetries) {
        // 重试前安全地清理当前 reply
        QPointer<QNetworkReply> replyPtr(m_currentReply);
        m_currentReply = nullptr;
        m_replyDeleting = true;

        if (replyPtr) {
            replyPtr->disconnect();
            replyPtr->deleteLater();
        }

        retryRequest();
        return;
    }

    QByteArray responseData = m_currentReply->readAll();

    // 安全地删除 reply
    QPointer<QNetworkReply> replyPtr(m_currentReply);
    m_currentReply = nullptr;
    m_replyDeleting = true;

    if (replyPtr) {
        replyPtr->disconnect();
        replyPtr->deleteLater();
    }

    if (statusCode != 200 && statusCode != 0) {  // 某些情况下状态码可能为 0 但数据有效（如 local file，虽然这里不常用）
        QString errorMsg = QString("HTTP request failed with status code: %1").arg(statusCode);
        emit requestError(errorMsg);
        m_isRequesting = false;
        return;
    }

    // 检查是否是 gzip 压缩的数据
    if (responseData.size() >= 2 && static_cast<unsigned char>(responseData[0]) == 0x1F &&
        static_cast<unsigned char>(responseData[1]) == 0x8B) {
        responseData = decompressGzip(responseData);
    }

    if (m_expectBinaryData) {
        emit binaryDataFetched(responseData);
        m_isRequesting = false;
        return;
    }

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QString errorMsg = QString("Failed to parse JSON response: %1").arg(parseError.errorString());
        emit requestError(errorMsg);
        m_isRequesting = false;
        return;
    }

    if (!jsonDoc.isObject()) {
        emit requestError("JSON response is not an object");
        m_isRequesting = false;
        return;
    }

    QJsonObject jsonObject = jsonDoc.object();
    m_isRequesting = false;
    emit requestSuccess(jsonObject);
}

void NetworkUtils::handleError(QNetworkReply::NetworkError error) {
    // 立即停止超时定时器，防止与 handleTimeout 竞态
    m_timeoutTimer->stop();

    // 防止重复处理（必须在访问 m_currentReply 之前检查）
    if (m_replyDeleting) {
        qWarning() << "Error: reply already marked for deletion";
        m_isRequesting = false;
        return;
    }

    if (!m_currentReply) {
        qWarning() << "Error: m_currentReply is null, possibly already handled by timeout";
        m_isRequesting = false;
        return;
    }

    QString errorMsg = m_currentReply->errorString();

    // 安全地删除 reply
    QPointer<QNetworkReply> replyPtr(m_currentReply);
    m_currentReply = nullptr;
    m_replyDeleting = true;

    if (replyPtr) {
        replyPtr->disconnect();
        replyPtr->deleteLater();
    }

    if (error == QNetworkReply::OperationCanceledError) {
        return;
    }

    if (m_retryCount < m_maxRetries) {
        retryRequest();
        return;
    }

    emit requestError(errorMsg);
    m_isRequesting = false;
}

void NetworkUtils::handleTimeout() {
    // 停止超时定时器，防止重复触发
    m_timeoutTimer->stop();

    // 防止重复处理（必须在访问 m_currentReply 之前检查）
    if (m_replyDeleting) {
        qWarning() << "Timeout: reply already marked for deletion";
        m_isRequesting = false;
        return;
    }

    // 检查是否已经处理完成
    if (!m_currentReply) {
        qWarning() << "Timeout: m_currentReply is already null, possibly handled by response";
        m_isRequesting = false;
        return;
    }

    // 使用 QPointer 安全地持有指针
    QPointer<QNetworkReply> replyPtr(m_currentReply);

    // 清空成员变量
    m_currentReply = nullptr;
    m_replyDeleting = true;
    m_isRequesting = false;

    // 如果对象还存在，安全地删除它
    if (replyPtr) {
        // 断开所有信号连接，防止后续事件触发
        replyPtr->disconnect();

        // 如果正在运行，先中止
        if (replyPtr->isRunning()) {
            replyPtr->abort();
        }

        // 安全地调用 deleteLater
        replyPtr->deleteLater();
    }

    if (m_retryCount < m_maxRetries) {
        retryRequest();
        return;
    }

    emit requestError("Request timeout");
    m_isRequesting = false;
}

void NetworkUtils::retryRequest() {
    m_retryCount++;
    int delay = calculateRetryDelay(m_retryCount);
    QTimer::singleShot(delay, this, &NetworkUtils::executeRequest);
}

bool NetworkUtils::shouldRetry(int statusCode) {
    return (statusCode == 429 || statusCode == 500 || statusCode == 502 || statusCode == 503 || statusCode == 504);
}

int NetworkUtils::calculateRetryDelay(int retryCount) {
    if (retryCount == 1)
        return 3000;
    if (retryCount == 2)
        return 5000;
    return 10000;
}

QByteArray NetworkUtils::decompressGzip(const QByteArray& compressedData) {
    if (compressedData.size() < 2)
        return QByteArray();

    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    if (inflateInit2(&stream, 15 + 16) != Z_OK)
        return QByteArray();

    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressedData.constData()));
    stream.avail_in = compressedData.size();

    QByteArray decompressedData;
    const int chunkSize = 4096;
    char buffer[chunkSize];

    int ret;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        stream.avail_out = chunkSize;
        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_OK || ret == Z_STREAM_END) {
            decompressedData.append(buffer, chunkSize - stream.avail_out);
        } else {
            inflateEnd(&stream);
            return QByteArray();
        }
    } while (ret != Z_STREAM_END && stream.avail_in > 0);

    inflateEnd(&stream);
    return (ret == Z_STREAM_END) ? decompressedData : QByteArray();
}

}  // namespace EasyKiConverter
