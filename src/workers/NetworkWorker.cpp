#include "NetworkWorker.h"

#include "core/network/AsyncNetworkRequest.h"
#include "core/network/NetworkClient.h"
#include "core/utils/GzipUtils.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>

namespace EasyKiConverter {

NetworkWorker::NetworkWorker(const QString& componentId, TaskType taskType, const QString& uuid, QObject* parent)
    : m_componentId(componentId), m_taskType(taskType), m_uuid(uuid), m_currentRequest(nullptr) {
    Q_UNUSED(parent);
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
    }
}

bool NetworkWorker::fetchComponentInfo() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://easyeda.com/api/components/%1").arg(m_componentId));
    if (!executeRequest(url, ResourceType::ComponentInfo, DEFAULT_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        QString errorMessage = "Invalid JSON response";
        qWarning() << "JSON parse error in fetchComponentInfo:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    emit componentInfoFetched(m_componentId, doc.object());
    qDebug() << "Component info fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::fetchCadData() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://easyeda.com/api/components/%1/cad").arg(m_componentId));
    if (!executeRequest(url, ResourceType::CadData, DEFAULT_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    GzipUtils::DecompressResult decompResult = GzipUtils::decompress(responseData);
    if (!decompResult.success) {
        QString errorMessage = "Failed to decompress CAD data";
        qWarning() << "Decompression error in fetchCadData:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(decompResult.data);
    if (doc.isNull() || !doc.isObject()) {
        QString errorMessage = "Invalid JSON response";
        qWarning() << "JSON parse error in fetchCadData:" << errorMessage;
        emit fetchError(m_componentId, errorMessage);
        return false;
    }

    emit cadDataFetched(m_componentId, doc.object());
    qDebug() << "CAD data fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::fetch3DModelObj() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://easyeda.com/api/models/%1/obj").arg(m_uuid));
    if (!executeRequest(url, ResourceType::Model3DObj, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);
    qDebug() << "3D model OBJ data fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::fetch3DModelMtl() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://easyeda.com/api/models/%1/mtl").arg(m_uuid));
    if (!executeRequest(url, ResourceType::Model3DStep, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);
    qDebug() << "3D model MTL data fetched successfully for:" << m_componentId;
    return true;
}

bool NetworkWorker::executeRequest(const QUrl& url,
                                   ResourceType resourceType,
                                   int timeoutMs,
                                   int maxRetries,
                                   QByteArray& outData,
                                   QString& errorMsg) {
    RequestProfile profile = RequestProfiles::fromType(resourceType);
    profile.connectTimeoutMs = timeoutMs;
    profile.readTimeoutMs = qMax(profile.readTimeoutMs, timeoutMs);
    profile.maxRetries = maxRetries;
    RetryPolicy policy = RetryPolicy::fromProfile(profile);

    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(url, resourceType, policy);
    {
        QMutexLocker locker(&m_mutex);
        m_currentRequest = request;
    }

    QEventLoop loop;
    connect(request, &AsyncNetworkRequest::finished, &loop, &QEventLoop::quit);
    connect(request, &AsyncNetworkRequest::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            int progress = static_cast<int>((static_cast<double>(bytesReceived) / bytesTotal) * 100.0);
            emit requestProgress(m_componentId, qBound(0, progress, 100));
        }
    });
    loop.exec();

    {
        QMutexLocker locker(&m_mutex);
        if (m_currentRequest == request) {
            m_currentRequest = nullptr;
        }
    }

    const NetworkResult result = request->result();
    request->deleteLater();

    if (result.wasCancelled) {
        errorMsg = "Request cancelled";
        return false;
    }

    if (!result.success) {
        errorMsg = result.error.isEmpty()
                       ? QString("Network error for %1 after %2 retries").arg(url.toString()).arg(result.retryCount)
                       : result.error;
        qWarning() << "NetworkWorker:" << errorMsg << "for:" << m_componentId;
        return false;
    }

    outData = result.data;
    return true;
}

void NetworkWorker::abort() {
    QMutexLocker locker(&m_mutex);
    if (m_currentRequest) {
        qDebug() << "Aborting network request for component:" << m_componentId;
        m_currentRequest->cancel();
        m_currentRequest = nullptr;
    }
}

}  // namespace EasyKiConverter
