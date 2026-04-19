#include "EasyedaApi.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QJsonDocument>
#include <QMutexLocker>

namespace EasyKiConverter {

static const QString API_ENDPOINT = "https://easyeda.com/api/products/%1/components?version=6.5.51";
static const QString ENDPOINT_3D_MODEL = "https://modules.easyeda.com/3dmodel/%1";
static const QString ENDPOINT_3D_MODEL_STEP = "https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1";

EasyedaApi::EasyedaApi(QObject* parent)
    : QObject(parent), m_networkClient(&NetworkClient::instance()), m_isFetching(false), m_weakNetworkSupport(false) {}

EasyedaApi::EasyedaApi(INetworkClient* networkClient, QObject* parent)
    : QObject(parent)
    , m_networkClient(networkClient ? networkClient : &NetworkClient::instance())
    , m_isFetching(false)
    , m_weakNetworkSupport(false) {}

EasyedaApi::~EasyedaApi() {
    cancelRequest();
}

void EasyedaApi::setWeakNetworkSupport(bool enabled) {
    m_weakNetworkSupport = enabled;
}

bool EasyedaApi::weakNetworkSupport() const {
    return m_weakNetworkSupport;
}

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

void EasyedaApi::fetchCadData(const QString& lcscId) {
    if (!validateLcscId(lcscId)) {
        emit fetchError(lcscId, QString("Invalid LCSC ID format: %1").arg(lcscId));
        return;
    }

    fetchWithNetworkClient(lcscId, QUrl(buildComponentApiUrl(lcscId)), ResourceType::CadData, false);
}

void EasyedaApi::fetch3DModelObj(const QString& uuid) {
    if (uuid.isEmpty()) {
        emit fetchError("UUID is empty");
        return;
    }

    m_currentUuid = uuid;
    fetchWithNetworkClient(uuid, QUrl(build3DModelObjUrl(uuid)), ResourceType::Model3DObj, true);
}

void EasyedaApi::fetch3DModelStep(const QString& uuid) {
    if (uuid.isEmpty()) {
        emit fetchError("UUID is empty");
        return;
    }

    m_currentUuid = uuid;
    fetchWithNetworkClient(uuid, QUrl(build3DModelStepUrl(uuid)), ResourceType::Model3DStep, true);
}

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
        m_activeRequests.append(QPointer<AsyncNetworkRequest>(request));
    }

    connect(request,
            &AsyncNetworkRequest::finished,
            this,
            [this, request, id, resourceType, isBinary](const NetworkResult&) {
                handleAsyncRequestFinished(request, id, resourceType, isBinary);
            });
}

void EasyedaApi::handleAsyncRequestFinished(AsyncNetworkRequest* request,
                                            const QString& id,
                                            ResourceType resourceType,
                                            bool isBinary) {
    const NetworkResult result = request->result();

    {
        QMutexLocker locker(&m_requestsMutex);
        m_activeRequests.removeOne(QPointer<AsyncNetworkRequest>(request));
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

void EasyedaApi::cancelRequest() {
    QMutexLocker locker(&m_requestsMutex);
    for (auto& req : m_activeRequests) {
        if (req && !req.isNull()) {
            req->cancel();
        }
    }
    m_activeRequests.clear();
    m_isFetching = false;
}

void EasyedaApi::handleComponentInfoResponse(const QString& lcscId, const QJsonObject& data) {
    m_isFetching = false;
    if (data.contains("success") && !data["success"].toBool()) {
        emit fetchError(lcscId, "API Error");
        return;
    }
    emit componentInfoFetched(lcscId, data);
}

void EasyedaApi::handleCadDataResponse(const QString& lcscId, const QJsonObject& data) {
    if (!data.contains("result")) {
        emit fetchError(lcscId, "No result");
        return;
    }
    QJsonObject result = data["result"].toObject();
    result["lcscId"] = lcscId;
    emit cadDataFetched(lcscId, result);
}

void EasyedaApi::resetRequestState() {}

QString EasyedaApi::buildComponentApiUrl(const QString& lcscId) const {
    return API_ENDPOINT.arg(lcscId);
}

QString EasyedaApi::build3DModelObjUrl(const QString& uuid) const {
    return ENDPOINT_3D_MODEL.arg(uuid);
}

QString EasyedaApi::build3DModelStepUrl(const QString& uuid) const {
    return ENDPOINT_3D_MODEL_STEP.arg(uuid);
}

bool EasyedaApi::validateLcscId(const QString& lcscId) const {
    if (!lcscId.startsWith('C', Qt::CaseInsensitive)) {
        return false;
    }
    bool ok = false;
    lcscId.mid(1).toInt(&ok);
    return ok;
}

}  // namespace EasyKiConverter
