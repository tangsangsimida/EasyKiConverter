#include "EasyedaApi.h"

#include "core/utils/INetworkAdapter.h"
#include "core/utils/NetworkUtils.h"

#include <QDebug>
#include <QJsonDocument>
#include <QMutexLocker>

namespace EasyKiConverter {

// API 端点
static const QString API_ENDPOINT = "https://easyeda.com/api/products/%1/components?version=6.5.51";
static const QString ENDPOINT_3D_MODEL = "https://modules.easyeda.com/3dmodel/%1";
static const QString ENDPOINT_3D_MODEL_STEP = "https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1";

EasyedaApi::EasyedaApi(QObject* parent) {
    m_networkUtils = new NetworkUtils(this);
    m_isFetching = false;
    m_requestType = RequestType::None;
    setParent(parent);

    if (m_networkUtils) {
        connect(m_networkUtils, &INetworkAdapter::requestSuccess, this, [this](const QJsonObject& data) {
            handleRequestSuccess(data);
        });
        connect(m_networkUtils, &INetworkAdapter::requestError, this, [this](const QString& error) {
            handleNetworkError(error);
        });
        connect(m_networkUtils, &INetworkAdapter::binaryDataFetched, this, [this](const QByteArray& data) {
            handleBinaryDataFetched(m_networkUtils, m_currentUuid, data);
        });
    }
}

EasyedaApi::EasyedaApi(INetworkAdapter* adapter, QObject* parent)
    : QObject(parent), m_networkUtils(adapter), m_isFetching(false), m_requestType(RequestType::None) {
    if (m_networkUtils) {
        if (!m_networkUtils->parent()) {
            m_networkUtils->setParent(this);
        }

        connect(m_networkUtils, &INetworkAdapter::requestSuccess, this, [this](const QJsonObject& data) {
            handleRequestSuccess(data);
        });
        connect(m_networkUtils, &INetworkAdapter::requestError, this, [this](const QString& error) {
            handleNetworkError(error);
        });
        connect(m_networkUtils, &INetworkAdapter::binaryDataFetched, this, [this](const QByteArray& data) {
            handleBinaryDataFetched(m_networkUtils, m_currentUuid, data);
        });
    }
}

EasyedaApi::~EasyedaApi() {
    cancelRequest();
}

void EasyedaApi::fetchComponentInfo(const QString& lcscId) {
    if (m_isFetching) {
        qWarning() << "Already fetching component info";
        return;
    }

    if (!validateLcscId(lcscId)) {
        QString errorMsg = QString("Invalid LCSC ID format: %1").arg(lcscId);
        emit fetchError(errorMsg);
        return;
    }

    resetRequestState();
    m_currentLcscId = lcscId;
    m_isFetching = true;
    m_requestType = RequestType::ComponentInfo;

    QString apiUrl = buildComponentApiUrl(lcscId);
    if (m_networkUtils) {
        m_networkUtils->sendGetRequest(apiUrl);
    }
}

void EasyedaApi::fetchCadData(const QString& lcscId) {
    if (!validateLcscId(lcscId)) {
        QString errorMsg = QString("Invalid LCSC ID format: %1").arg(lcscId);
        emit fetchError(lcscId, errorMsg);
        return;
    }

    // 为每个请求创建独立的 NetworkUtils 实例，支持并行请求
    NetworkUtils* networkUtils = new NetworkUtils(nullptr);

    // 跟踪活跃请求
    {
        QMutexLocker locker(&m_requestsMutex);
        m_activeRequests.append(QPointer<INetworkAdapter>(networkUtils));
    }

    // 连接信号
    connect(
        networkUtils, &INetworkAdapter::requestSuccess, this, [this, networkUtils, lcscId](const QJsonObject& data) {
            handleRequestSuccess(networkUtils, lcscId, data);
        });
    connect(networkUtils, &INetworkAdapter::requestError, this, [this, networkUtils, lcscId](const QString& error) {
        handleRequestError(networkUtils, lcscId, error);
    });

    QString apiUrl = buildComponentApiUrl(lcscId);
    networkUtils->sendGetRequest(apiUrl);
}

void EasyedaApi::fetch3DModelObj(const QString& uuid) {
    if (uuid.isEmpty()) {
        emit fetchError("UUID is empty");
        return;
    }

    // 为每个请求创建独立的 NetworkUtils 实例，支持并行请求
    NetworkUtils* networkUtils = new NetworkUtils(nullptr);
    networkUtils->setExpectBinaryData(true);

    // 跟踪活跃请求
    {
        QMutexLocker locker(&m_requestsMutex);
        m_activeRequests.append(QPointer<INetworkAdapter>(networkUtils));
    }

    // 连接信号
    connect(
        networkUtils, &INetworkAdapter::binaryDataFetched, this, [this, networkUtils, uuid](const QByteArray& data) {
            handleBinaryDataFetched(networkUtils, uuid, data);
        });
    connect(networkUtils, &INetworkAdapter::requestError, this, [this, networkUtils, uuid](const QString& error) {
        emit fetchError(uuid, error);
        {
            QMutexLocker locker(&m_requestsMutex);
            m_activeRequests.removeOne(QPointer<INetworkAdapter>(networkUtils));
        }
        networkUtils->deleteLater();
    });

    networkUtils->sendGetRequest(build3DModelObjUrl(uuid));
}

void EasyedaApi::fetch3DModelStep(const QString& uuid) {
    if (uuid.isEmpty()) {
        emit fetchError("UUID is empty");
        return;
    }

    // 为每个请求创建独立的 NetworkUtils 实例，支持并行请求
    NetworkUtils* networkUtils = new NetworkUtils(nullptr);
    networkUtils->setExpectBinaryData(true);

    // 跟踪活跃请求
    {
        QMutexLocker locker(&m_requestsMutex);
        m_activeRequests.append(QPointer<INetworkAdapter>(networkUtils));
    }

    // 连接信号
    connect(
        networkUtils, &INetworkAdapter::binaryDataFetched, this, [this, networkUtils, uuid](const QByteArray& data) {
            emit model3DFetched(uuid, data);
            {
                QMutexLocker locker(&m_requestsMutex);
                m_activeRequests.removeOne(QPointer<INetworkAdapter>(networkUtils));
            }
            networkUtils->deleteLater();
        });
    connect(networkUtils, &INetworkAdapter::requestError, this, [this, networkUtils, uuid](const QString& error) {
        emit fetchError(uuid, error);
        {
            QMutexLocker locker(&m_requestsMutex);
            m_activeRequests.removeOne(QPointer<INetworkAdapter>(networkUtils));
        }
        networkUtils->deleteLater();
    });

    networkUtils->sendGetRequest(build3DModelStepUrl(uuid));
}

void EasyedaApi::handleRequestSuccess(const QJsonObject& data) {
    switch (m_requestType) {
        case RequestType::ComponentInfo:
            handleComponentInfoResponse(data);
            break;
        case RequestType::CadData:
            handleCadDataResponse(data);
            break;
        case RequestType::Model3DObj:
        case RequestType::Model3DStep:
            handleModel3DResponse(data);
            break;
        default:
            break;
    }
}

void EasyedaApi::handleRequestSuccess(INetworkAdapter* adapter, const QString& lcscId, const QJsonObject& data) {
    // 直接使用传入的 lcscId 处理响应
    if (!data.contains("result")) {
        emit fetchError(lcscId, "No result");
    } else {
        QJsonObject result = data["result"].toObject();
        result["lcscId"] = lcscId;
        emit cadDataFetched(result);
    }

    {
        QMutexLocker locker(&m_requestsMutex);
        m_activeRequests.removeOne(QPointer<INetworkAdapter>(adapter));
    }
    adapter->deleteLater();
}

void EasyedaApi::handleRequestError(INetworkAdapter* adapter, const QString& id, const QString& error) {
    emit fetchError(id, error);
    {
        QMutexLocker locker(&m_requestsMutex);
        m_activeRequests.removeOne(QPointer<INetworkAdapter>(adapter));
    }
    adapter->deleteLater();
}

void EasyedaApi::handleBinaryDataFetched(INetworkAdapter* adapter, const QString& id, const QByteArray& data) {
    emit model3DFetched(id, data);

    QMutexLocker locker(&m_requestsMutex);
    QPointer<INetworkAdapter> ptr(adapter);
    if (m_activeRequests.contains(ptr)) {
        m_activeRequests.removeOne(ptr);
        adapter->deleteLater();
    }
}

void EasyedaApi::cancelRequest() {
    if (m_networkUtils)
        m_networkUtils->cancelRequest();
    m_isFetching = false;

    QMutexLocker locker(&m_requestsMutex);
    for (auto& req : m_activeRequests) {
        if (req)
            req->cancelRequest();
    }
    m_activeRequests.clear();
}

void EasyedaApi::handleComponentInfoResponse(const QJsonObject& data) {
    m_isFetching = false;
    if (data.contains("success") && !data["success"].toBool()) {
        emit fetchError("API Error");
        return;
    }
    emit componentInfoFetched(data);
}

void EasyedaApi::handleCadDataResponse(const QJsonObject& data) {
    m_isFetching = false;
    if (!data.contains("result")) {
        emit fetchError(m_currentLcscId, "No result");
        return;
    }
    QJsonObject result = data["result"].toObject();
    result["lcscId"] = m_currentLcscId;
    emit cadDataFetched(result);
}

void EasyedaApi::handleModel3DResponse(const QJsonObject& data) {
    Q_UNUSED(data);
}

void EasyedaApi::handleNetworkError(const QString& errorMessage) {
    m_isFetching = false;
    emit fetchError(errorMessage);
}

void EasyedaApi::resetRequestState() {
    if (m_networkUtils)
        m_networkUtils->setExpectBinaryData(false);
}

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
    if (!lcscId.startsWith('C', Qt::CaseInsensitive))
        return false;
    bool ok;
    lcscId.mid(1).toInt(&ok);
    return ok;
}

}  // namespace EasyKiConverter
