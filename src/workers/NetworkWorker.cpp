#include "NetworkWorker.h"

#include "core/network/AsyncNetworkRequest.h"
#include "core/network/BlockingRequestContext.h"
#include "core/network/NetworkClient.h"
#include "core/utils/GzipUtils.h"
#include "services/ConfigService.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QWaitCondition>

namespace EasyKiConverter {

/** @brief 创建指定网络任务类型的后台 Worker。 */
NetworkWorker::NetworkWorker(const QString& componentId, TaskType taskType, const QString& uuid, QObject* parent)
    // 保存任务参数并等待 run() 在工作线程中执行。
    : m_componentId(componentId), m_taskType(taskType), m_uuid(uuid), m_currentRequest(nullptr) {
    Q_UNUSED(parent);
}

/** @brief 记录 Worker 销毁并释放其请求关联。 */
NetworkWorker::~NetworkWorker() {
    qDebug() << "NetworkWorker destroyed for:" << m_componentId;
}

/** @brief 根据任务类型执行对应的网络获取流程。 */
void NetworkWorker::run() {
    qDebug() << "NetworkWorker started for:" << m_componentId << "- TaskType:" << static_cast<int>(m_taskType);

    bool success = false;

    // 根据任务类型选择对应的请求和结果处理流程。
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

/** @brief 获取元器件基础信息。 */
bool NetworkWorker::fetchComponentInfo() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId));
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

/** @brief 获取元器件 CAD 数据。 */
bool NetworkWorker::fetchCadData() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId));
    if (!executeRequest(url, ResourceType::CadData, DEFAULT_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    // Note: AsyncNetworkRequest already handles gzip decompression internally.
    // responseData is already decompressed if it was gzip-compressed.
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
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

/** @brief 获取元器件 OBJ 三维模型。 */
bool NetworkWorker::fetch3DModelObj() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://modules.easyeda.com/3dmodel/%1").arg(m_uuid));
    if (!executeRequest(url, ResourceType::Model3DObj, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);
    qDebug() << "3D model OBJ data fetched successfully for:" << m_componentId;
    return true;
}

/** @brief 获取元器件 MTL 三维材质文件。 */
bool NetworkWorker::fetch3DModelMtl() {
    QByteArray responseData;
    QString errorMsg;

    const QUrl url(QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(m_uuid));
    if (!executeRequest(url, ResourceType::Model3DStep, MODEL_TIMEOUT_MS, MAX_RETRIES, responseData, errorMsg)) {
        emit fetchError(m_componentId, errorMsg);
        return false;
    }

    emit model3DFetched(m_componentId, m_uuid, responseData);
    qDebug() << "3D model MTL data fetched successfully for:" << m_componentId;
    return true;
}

/** @brief 执行一次带重试策略的同步等待请求。 */
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
    RetryPolicy policy = RetryPolicy::fromProfile(profile, ConfigService::instance()->getWeakNetworkSupport());

    AsyncNetworkRequest* request = NetworkClient::instance().getAsync(url, resourceType, policy);
    if (!request) {
        errorMsg = QStringLiteral("Failed to create network request");
        return false;
    }

    // 白名单拒绝等场景：request 已完成，直接返回结果，避免阻塞等待
    if (request->isFinished()) {
        const NetworkResult doneResult = request->result();
        delete request;
        if (doneResult.wasCancelled) {
            errorMsg = "Request cancelled";
            return false;
        }
        if (!doneResult.success) {
            errorMsg =
                doneResult.error.isEmpty()
                    ? QString("Network error for %1 after %2 retries").arg(url.toString()).arg(doneResult.retryCount)
                    : doneResult.error;
            qWarning() << "NetworkWorker:" << errorMsg << "for:" << m_componentId;
            return false;
        }
        outData = doneResult.data;
        return true;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_currentRequest = request;
    }

    auto context = std::make_shared<BlockingRequestContext>();
    QObject::connect(
        request,
        &AsyncNetworkRequest::finished,
        request,
        [context](const NetworkResult& result) { context->complete(result); },
        Qt::DirectConnection);

    const NetworkResult result = context->wait();

    {
        QMutexLocker locker(&m_mutex);
        if (m_currentRequest == request) {
            m_currentRequest = nullptr;
        }
    }
    QMetaObject::invokeMethod(request, &QObject::deleteLater, Qt::QueuedConnection);

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

/** @brief 取消当前网络请求并标记 Worker 终止。 */
void NetworkWorker::abort() {
    QMutexLocker locker(&m_mutex);
    if (m_currentRequest) {
        qDebug() << "Aborting network request for component:" << m_componentId;
        m_currentRequest->cancel();
        m_currentRequest = nullptr;
    }
}

}  // namespace EasyKiConverter
