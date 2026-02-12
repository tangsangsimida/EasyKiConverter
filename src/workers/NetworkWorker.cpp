#include "NetworkWorker.h"

#include "core/easyeda/EasyedaApi.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

#include <zlib.h>

namespace EasyKiConverter {

NetworkWorker::NetworkWorker(const QString& componentId, TaskType taskType, const QString& uuid, QObject* parent)
    : QObject(parent), QRunnable(), m_componentId(componentId), m_taskType(taskType), m_uuid(uuid) {
    setAutoDelete(true);  // 任务完成后自动删除
}

NetworkWorker::~NetworkWorker() {
    qDebug() << "NetworkWorker destroyed for:" << m_componentId;
}

void NetworkWorker::run() {
    qDebug() << "NetworkWorker started for:" << m_componentId << "- TaskType:" << static_cast<int>(m_taskType);

    bool success = false;

    switch (m_taskType) {
        case TaskType::FetchComponentInfo:
            success = fetchComponentInfo();
            break;
        case TaskType::FetchCadData:
            success = fetchCadData();
            break;
        case TaskType::Fetch3DModelObj:
            success = fetch3DModelObj();
            break;
        case TaskType::Fetch3DModelMtl:
            success = fetch3DModelMtl();
            break;
        default:
            qWarning() << "Unknown task type:" << static_cast<int>(m_taskType);
            emit fetchError(m_componentId, "Unknown task type");
            return;
    }

    if (!success) {
        qWarning() << "Network request failed for:" << m_componentId;
        // 错误信号已经在各个fetch方法中发出
    }
}

bool NetworkWorker::fetchComponentInfo() {
    QNetworkAccessManager manager;

    QString url = QString("https://easyeda.com/api/components/%1").arg(m_componentId);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");

    QByteArray responseData;
    QString errorMsg;

    if (!executeRequest(manager, request, DEFAULT_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    // 解析JSON
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        QString errorMessage = "Invalid JSON response";
        qWarning() << "JSON parse error in fetchComponentInfo:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    QJsonObject data = doc.object();
    emit componentInfoFetched(m_componentId, data);

    qDebug() << "Component info fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::fetchCadData() {
    QNetworkAccessManager manager;

    QString url = QString("https://easyeda.com/api/components/%1/cad").arg(m_componentId);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");

    QByteArray responseData;
    QString errorMsg;

    if (!executeRequest(manager, request, DEFAULT_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    // 解压gzip数据
    QByteArray decompressedData = decompressGzip(responseData);
    if (decompressedData.isEmpty()) {
        QString errorMessage = "Failed to decompress CAD data";
        qWarning() << "Decompression error in fetchCadData:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    // 解析JSON
    QJsonDocument doc = QJsonDocument::fromJson(decompressedData);
    if (doc.isNull() || !doc.isObject()) {
        QString errorMessage = "Invalid JSON response";
        qWarning() << "JSON parse error in fetchCadData:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    QJsonObject data = doc.object();
    emit cadDataFetched(m_componentId, data);

    qDebug() << "CAD data fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::fetch3DModelObj() {
    QNetworkAccessManager manager;

    QString url = QString("https://easyeda.com/api/models/%1/obj").arg(m_uuid);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("Accept", "application/octet-stream");

    QByteArray responseData;
    QString errorMsg;

    if (!executeRequest(manager, request, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);

    qDebug() << "3D model OBJ data fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::fetch3DModelMtl() {
    QNetworkAccessManager manager;

    QString url = QString("https://easyeda.com/api/models/%1/mtl").arg(m_uuid);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("Accept", "application/octet-stream");

    QByteArray responseData;
    QString errorMsg;

    if (!executeRequest(manager, request, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);

    qDebug() << "3D model MTL data fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::executeRequest(QNetworkAccessManager& manager,
                                   const QNetworkRequest& request,
                                   int timeoutMs,
                                   int maxRetries,
                                   QByteArray& outData,
                                   QString& errorMsg) {
    for (int retry = 0; retry <= maxRetries; ++retry) {
        // 重试前等待（递增延迟）
        if (retry > 0) {
            int delayIndex = retry - 1;
            int delay = RETRY_DELAYS_MS[qMin(
                delayIndex, static_cast<int>(sizeof(RETRY_DELAYS_MS) / sizeof(RETRY_DELAYS_MS[0])) - 1)];
            qDebug() << "NetworkWorker: Retrying request in" << delay << "ms (Retry" << retry << "/" << maxRetries
                     << ")";
            QThread::msleep(delay);
        }

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        QPointer<QNetworkReply> reply;
        {
            QMutexLocker locker(&m_mutex);
            m_currentReply = manager.get(request);
            reply = m_currentReply;
        }

        if (reply.isNull()) {
            errorMsg = "Failed to create network request";
            qWarning() << "NetworkWorker: Failed to create network reply for:" << m_componentId;
            continue;
        }

        // 连接信号
        QObject::connect(
            reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                int progress = static_cast<int>((static_cast<double>(bytesReceived) / bytesTotal) * 100);
                progress = qBound(0, progress, 100);
                emit requestProgress(m_componentId, progress);
            }
            }
            emit requestProgress(m_componentId, progress);
    }
});

QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
    if (reply && reply->isRunning()) {
        reply->abort();
    }
    loop.quit();
});

timeoutTimer.start(timeoutMs);
loop.exec();

// 清理跟踪
{
    QMutexLocker locker(&m_mutex);
    m_currentReply.clear();
}

// 检查是否被中断
if (reply.isNull()) {
    errorMsg = "Request aborted";
    qWarning() << "NetworkWorker: Request aborted for:" << m_componentId;
    return false;  // 主动中断不重试
}

if (reply->error() == QNetworkReply::NoError) {
    outData = reply->readAll();
    reply->deleteLater();
    return true;
}
}

// 超时或网络错误，继续重试
errorMsg = QString("Network error: %1 (attempt %2/%3)").arg(reply->errorString()).arg(retry + 1).arg(maxRetries + 1);
qWarning() << "NetworkWorker:" << errorMsg << "for:" << m_componentId;

reply->deleteLater();
}

return false;
}

void NetworkWorker::abort() {
    QMutexLocker locker(&m_mutex);
    if (m_currentReply) {
        qDebug() << "Aborting network request for component:" << m_componentId;
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply.clear();
    }
}

QByteArray NetworkWorker::decompressGzip(const QByteArray& compressedData) {
    if (compressedData.isEmpty()) {
        return QByteArray();
    }

    // 检查是否为gzip数据（gzip魔术数字：0x1f 0x8b）
    if (compressedData.size() < 2) {
        qDebug() << "Data too short to be gzip compressed";
        return QByteArray();
    }

    unsigned char firstByte = static_cast<unsigned char>(compressedData[0]);
    unsigned char secondByte = static_cast<unsigned char>(compressedData[1]);

    if (firstByte != 0x1f || secondByte != 0x8b) {
        // 不是gzip数据，直接返回
        qDebug() << "Data is not gzip compressed, returning as-is";
        return compressedData;
    }

    // 初始化zlib解压器
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    // 使用16+MAX_WBITS来处理gzip格式
    int ret = inflateInit2(&stream, 16 + MAX_WBITS);
    if (ret != Z_OK) {
        qWarning() << "Failed to initialize decompression, error code:" << ret;
        return QByteArray();
    }

    // 设置输入数据
    stream.avail_in = compressedData.size();
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressedData.constData()));

    // 准备输出缓冲区
    QByteArray decompressedData;
    const int chunkSize = 8192;
    char buffer[chunkSize];

    // 执行解压
    do {
        stream.avail_out = chunkSize;
        stream.next_out = reinterpret_cast<Bytef*>(buffer);

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret == Z_BUF_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            qWarning() << "Decompression error, error code:" << ret;
            inflateEnd(&stream);
            return QByteArray();
        }

        // 添加解压后的数据
        int have = chunkSize - stream.avail_out;
        if (have > 0) {
            decompressedData.append(buffer, have);
        }

    } while (ret != Z_STREAM_END);

    // 清理
    inflateEnd(&stream);

    qDebug() << "Decompressed" << compressedData.size() << "bytes to" << decompressedData.size() << "bytes";
    return decompressedData;
}

}  // namespace EasyKiConverter