#include "EasyedaApi.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QJsonDocument>
#include <QMutexLocker>

#include <algorithm>

namespace EasyKiConverter {

static const QString API_ENDPOINT = "https://easyeda.com/api/products/%1/components?version=6.5.51";
static const QString ENDPOINT_3D_MODEL = "https://modules.easyeda.com/3dmodel/%1";
static const QString ENDPOINT_3D_MODEL_STEP = "https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1";

// 创建使用全局网络客户端的 EasyEDA API 服务。
EasyedaApi::EasyedaApi(QObject* parent)
    // 初始化网络客户端和请求状态。
    : QObject(parent), m_networkClient(&NetworkClient::instance()), m_isFetching(false), m_weakNetworkSupport(false) {}

// 创建使用指定网络客户端的 EasyEDA API 服务。
EasyedaApi::EasyedaApi(INetworkClient* networkClient, QObject* parent)
    // 初始化注入的网络客户端和请求状态。
    : QObject(parent)
    , m_networkClient(networkClient ? networkClient : &NetworkClient::instance())
    , m_isFetching(false)
    , m_weakNetworkSupport(false) {}

// 析构前取消所有尚未完成的网络请求。
EasyedaApi::~EasyedaApi() {
    cancelRequest();
}

// 设置网络请求是否启用弱网策略。
void EasyedaApi::setWeakNetworkSupport(bool enabled) {
    m_weakNetworkSupport = enabled;
}

// 返回当前弱网策略开关。
bool EasyedaApi::weakNetworkSupport() const {
    return m_weakNetworkSupport;
}

// 异步获取指定元器件的基础信息。
void EasyedaApi::fetchComponentInfo(const QString& lcscId) {
    if (m_isFetching) {
        qWarning() << "Already fetching component info";
        return;
    }

    if (!validateLcscId(lcscId)) {
        emit fetchError(QString("Invalid LCSC ID format: %1").arg(lcscId));
        return;
    }

    resetRequestState();
    m_currentLcscId = lcscId;
    m_isFetching = true;
    fetchWithNetworkClient(lcscId, QUrl(buildComponentApiUrl(lcscId)), ResourceType::ComponentInfo, false);
}

// 异步获取指定元器件的 CAD 数据。
void EasyedaApi::fetchCadData(const QString& lcscId) {
    if (!validateLcscId(lcscId)) {
        emit fetchError(lcscId, QString("Invalid LCSC ID format: %1").arg(lcscId));
        return;
    }

    fetchWithNetworkClient(lcscId, QUrl(buildComponentApiUrl(lcscId)), ResourceType::CadData, false);
}

// 异步获取指定 UUID 的 OBJ 模型。
void EasyedaApi::fetch3DModelObj(const QString& uuid) {
    if (uuid.isEmpty()) {
        emit fetchError("UUID is empty");
        return;
    }

    m_currentUuid = uuid;
    fetchWithNetworkClient(uuid, QUrl(build3DModelObjUrl(uuid)), ResourceType::Model3DObj, true);
}

// 异步获取指定 UUID 的 STEP 模型。
void EasyedaApi::fetch3DModelStep(const QString& uuid) {
    if (uuid.isEmpty()) {
        emit fetchError("UUID is empty");
        return;
    }

    m_currentUuid = uuid;
    fetchWithNetworkClient(uuid, QUrl(build3DModelStepUrl(uuid)), ResourceType::Model3DStep, true);
}

// 创建网络请求并登记活动请求，用于取消和回调归属校验。
void EasyedaApi::fetchWithNetworkClient(const QString& id, const QUrl& url, ResourceType resourceType, bool isBinary) {
    if (!m_networkClient) {
        emit fetchError(id, "NetworkClient not available");
        return;
    }

    RequestProfile profile = RequestProfiles::fromType(resourceType);
    RetryPolicy policy = RetryPolicy::fromProfile(profile, m_weakNetworkSupport);
    AsyncNetworkRequest* request = m_networkClient->getAsync(url, resourceType, policy);

    {
        QMutexLocker locker(&m_requestsMutex);
        m_activeRequests.append({QPointer<AsyncNetworkRequest>(request), id});
    }

    connect(request,
            &AsyncNetworkRequest::finished,
            this,
            [this, request, id, resourceType, isBinary](const NetworkResult&) {
                handleAsyncRequestFinished(request, id, resourceType, isBinary);
            });
}

// 处理网络请求完成事件并分发解析后的 API 结果。
void EasyedaApi::handleAsyncRequestFinished(AsyncNetworkRequest* request,
                                            const QString& id,
                                            ResourceType resourceType,
                                            bool isBinary) {
    const NetworkResult result = request->result();

    {
        QMutexLocker locker(&m_requestsMutex);
        const auto it = std::find_if(m_activeRequests.cbegin(),
                                     m_activeRequests.cend(),
                                     [request](const ActiveRequest& active) { return active.request == request; });
        if (it == m_activeRequests.cend()) {
            qDebug() << "EasyedaApi: Discarding callback for cancelled request:" << id;
            request->deleteLater();
            return;
        }
        m_activeRequests.erase(it);
    }

    if (result.wasCancelled) {
        if (resourceType == ResourceType::ComponentInfo) {
            m_isFetching = false;
        }
        emit fetchError(id, "Request cancelled");
        request->deleteLater();
        return;
    }

    if (!result.success) {
        if (resourceType == ResourceType::ComponentInfo) {
            m_isFetching = false;
        }
        emit fetchError(id, result.error);
        request->deleteLater();
        return;
    }

    if (isBinary) {
        emit model3DFetched(id, result.data);
        request->deleteLater();
        return;
    }

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(result.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit fetchError(id, QString("Failed to parse JSON response: %1").arg(parseError.errorString()));
        request->deleteLater();
        return;
    }

    if (!jsonDoc.isObject()) {
        emit fetchError(id, "JSON response is not an object");
        request->deleteLater();
        return;
    }

    const QJsonObject jsonObject = jsonDoc.object();
    // 按资源类型分发结构化响应。
    switch (resourceType) {
        case ResourceType::ComponentInfo:
            m_isFetching = false;
            handleComponentInfoResponse(id, jsonObject);
            break;
        case ResourceType::CadData:
            handleCadDataResponse(id, jsonObject);
            break;
        default:
            emit fetchError(id, "Unsupported JSON resource type");
            break;
    }

    request->deleteLater();
}

// 取消全部活动请求，并在释放互斥锁后执行取消操作。
void EasyedaApi::cancelRequest() {
    QList<QPointer<AsyncNetworkRequest>> requestsToCancel;
    {
        QMutexLocker locker(&m_requestsMutex);
        for (const ActiveRequest& active : std::as_const(m_activeRequests)) {
            requestsToCancel.append(active.request);
        }
        m_activeRequests.clear();
    }

    for (const auto& req : requestsToCancel) {
        if (req && !req.isNull()) {
            req->cancel();
        }
    }
    m_isFetching = false;
}

// 取消指定标识对应的活动请求，并保留其他并行请求。
void EasyedaApi::cancelRequestForId(const QString& id) {
    QList<QPointer<AsyncNetworkRequest>> requestsToCancel;
    {
        QMutexLocker locker(&m_requestsMutex);
        for (int index = m_activeRequests.size() - 1; index >= 0; --index) {
            const ActiveRequest& active = m_activeRequests.at(index);
            if (active.id.compare(id, Qt::CaseInsensitive) == 0) {
                requestsToCancel.append(active.request);
                m_activeRequests.removeAt(index);
            }
        }
    }

    for (const auto& request : requestsToCancel) {
        if (request && !request.isNull()) {
            request->cancel();
        }
    }
    if (id.compare(m_currentLcscId, Qt::CaseInsensitive) == 0) {
        m_isFetching = false;
    }
}

// 校验并转发元器件基础信息响应。
void EasyedaApi::handleComponentInfoResponse(const QString& lcscId, const QJsonObject& data) {
    m_isFetching = false;
    if (data.contains("success") && !data["success"].toBool()) {
        emit fetchError(lcscId, "API Error");
        return;
    }
    emit componentInfoFetched(lcscId, data);
}

// 校验并转发元器件 CAD 响应。
void EasyedaApi::handleCadDataResponse(const QString& lcscId, const QJsonObject& data) {
    if (!data.contains("result")) {
        emit fetchError(lcscId, "No result");
        return;
    }
    QJsonObject result = data["result"].toObject();
    result["lcscId"] = lcscId;
    emit cadDataFetched(lcscId, result);
}

// 预留请求状态重置接口，当前状态由活动请求列表维护。
void EasyedaApi::resetRequestState() {}

// 构造元器件信息和 CAD 数据接口地址。
QString EasyedaApi::buildComponentApiUrl(const QString& lcscId) const {
    return API_ENDPOINT.arg(lcscId);
}

// 构造 OBJ 模型接口地址。
QString EasyedaApi::build3DModelObjUrl(const QString& uuid) const {
    return ENDPOINT_3D_MODEL.arg(uuid);
}

// 构造 STEP 模型接口地址。
QString EasyedaApi::build3DModelStepUrl(const QString& uuid) const {
    return ENDPOINT_3D_MODEL_STEP.arg(uuid);
}

// 校验元器件编号是否符合 EasyEDA 接口要求。
bool EasyedaApi::validateLcscId(const QString& lcscId) const {
    if (!lcscId.startsWith('C', Qt::CaseInsensitive)) {
        return false;
    }
    bool ok = false;
    lcscId.mid(1).toInt(&ok);
    return ok;
}

}  // namespace EasyKiConverter
