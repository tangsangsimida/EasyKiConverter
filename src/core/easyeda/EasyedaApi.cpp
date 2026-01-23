#include "EasyedaApi.h"
#include <qDebug>
#include <QJsonDocument>

namespace EasyKiConverter
{

    // API 端点
    static const QString API_ENDPOINT = "https://easyeda.com/api/products/%1/components?version=6.5.51";
    static const QString ENDPOINT_3D_MODEL = "https://modules.easyeda.com/3dmodel/%1";
    static const QString ENDPOINT_3D_MODEL_STEP = "https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1";

    EasyedaApi::EasyedaApi(QObject *parent)
        : QObject(parent), m_networkUtils(new NetworkUtils(this)), m_isFetching(false), m_requestType(RequestType::None)
    {
        // 注意：不再连接默认的 m_networkUtils 信号，因为并行请求会创建独立�?NetworkUtils 实例

        // 设置请求�?
        m_networkUtils->setHeader("Accept-Encoding", "gzip, deflate");
        m_networkUtils->setHeader("Accept", "application/json, text/javascript, */*; q=0.01");
        m_networkUtils->setHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        m_networkUtils->setHeader("User-Agent", "EasyKiConverter/1.0.0");
    }

    EasyedaApi::~EasyedaApi()
    {
        cancelRequest();
    }

    void EasyedaApi::fetchComponentInfo(const QString &lcscId)
    {
        if (m_isFetching)
        {
            qWarning() << "Already fetching component info";
            return;
        }

        if (!validateLcscId(lcscId))
        {
            QString errorMsg = QString("Invalid LCSC ID format: %1").arg(lcscId);
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        resetRequestState();
        m_currentLcscId = lcscId;
        m_isFetching = true;
        m_requestType = RequestType::ComponentInfo;

        QString apiUrl = buildComponentApiUrl(lcscId);
        // qDebug() << "Fetching component info from:" << apiUrl;

        m_networkUtils->sendGetRequest(apiUrl, 30, 3);
    }

    void EasyedaApi::fetchCadData(const QString &lcscId)
    {
        if (!validateLcscId(lcscId))
        {
            QString errorMsg = QString("Invalid LCSC ID format: %1").arg(lcscId);
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        // 为每个请求创建独立的 NetworkUtils 实例以支持并行请�?
        NetworkUtils *networkUtils = new NetworkUtils(this);
        connect(networkUtils, &NetworkUtils::requestSuccess, this, [this, networkUtils, lcscId](const QJsonObject &data)
                { handleRequestSuccess(networkUtils, lcscId, data); });
        connect(networkUtils, &NetworkUtils::requestError, this, [this, networkUtils, lcscId](const QString &error)
                { handleRequestError(networkUtils, lcscId, error); });
        connect(networkUtils, &NetworkUtils::binaryDataFetched, this, [this, networkUtils, lcscId](const QByteArray &data)
                { handleBinaryDataFetched(networkUtils, lcscId, data); });

        // 设置请求�?
        networkUtils->setHeader("Accept-Encoding", "gzip, deflate");
        networkUtils->setHeader("Accept", "application/json, text/javascript, */*; q=0.01");
        networkUtils->setHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        networkUtils->setHeader("User-Agent", "EasyKiConverter/1.0.0");

        m_currentLcscId = lcscId;
        m_requestType = RequestType::CadData;

        QString apiUrl = buildComponentApiUrl(lcscId);
        // //qDebug() << "Fetching CAD data from:" << apiUrl;

        networkUtils->sendGetRequest(apiUrl, 30, 3);
    }

    void EasyedaApi::fetch3DModelObj(const QString &uuid)
    {
        if (uuid.isEmpty())
        {
            QString errorMsg = "UUID is empty";
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        // 为每个请求创建独立的 NetworkUtils 实例以支持并行请�?
        NetworkUtils *networkUtils = new NetworkUtils(this);
        connect(networkUtils, &NetworkUtils::binaryDataFetched, this, [this, networkUtils, uuid](const QByteArray &data)
                { handleBinaryDataFetched(networkUtils, uuid, data); });
        connect(networkUtils, &NetworkUtils::requestError, this, [this, networkUtils, uuid](const QString &error)
                { handleRequestError(networkUtils, uuid, error); });

        // 设置请求�?
        networkUtils->setHeader("Accept-Encoding", "gzip, deflate");
        networkUtils->setHeader("Accept", "application/octet-stream, */*");
        networkUtils->setHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        networkUtils->setHeader("User-Agent", "EasyKiConverter/1.0.0");

        // 设置期望接收二进制数�?
        networkUtils->setExpectBinaryData(true);

        QString apiUrl = build3DModelObjUrl(uuid);
        // qDebug() << "Fetching 3D model (OBJ) from:" << apiUrl;

        networkUtils->sendGetRequest(apiUrl, 30, 3);
    }

    void EasyedaApi::fetch3DModelStep(const QString &uuid)
    {
        if (uuid.isEmpty())
        {
            QString errorMsg = "UUID is empty";
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        // 为每个请求创建独立的 NetworkUtils 实例以支持并行请�?
        NetworkUtils *networkUtils = new NetworkUtils(this);
        connect(networkUtils, &NetworkUtils::binaryDataFetched, this, [this, networkUtils, uuid](const QByteArray &data)
                { handleBinaryDataFetched(networkUtils, uuid, data); });
        connect(networkUtils, &NetworkUtils::requestError, this, [this, networkUtils, uuid](const QString &error)
                { handleRequestError(networkUtils, uuid, error); });

        // 设置请求�?
        networkUtils->setHeader("Accept-Encoding", "gzip, deflate");
        networkUtils->setHeader("Accept", "application/octet-stream, */*");
        networkUtils->setHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        networkUtils->setHeader("User-Agent", "EasyKiConverter/1.0.0");

        // 设置期望接收二进制数�?
        networkUtils->setExpectBinaryData(true);

        QString apiUrl = build3DModelStepUrl(uuid);
        // qDebug() << "Fetching 3D model (STEP) from:" << apiUrl;

        networkUtils->sendGetRequest(apiUrl, 30, 3);
    }

    void EasyedaApi::handleRequestSuccess(const QJsonObject &data)
    {
        // 根据请求类型调用相应的处理函�?
        switch (m_requestType)
        {
        case RequestType::ComponentInfo:
            handleComponentInfoResponse(data);
            break;
        case RequestType::CadData:
            handleCadDataResponse(data);
            break;
        default:
            qWarning() << "Unknown request type";
            emit fetchError("Unknown request type");
            break;
        }
    }

    void EasyedaApi::handleRequestSuccess(NetworkUtils *networkUtils, const QString &lcscId, const QJsonObject &data)
    {
        // 保存当前处理�?LCSC ID
        QString savedLcscId = m_currentLcscId;
        m_currentLcscId = lcscId;

        // 处理数据
        handleCadDataResponse(data);

        // 恢复 LCSC ID
        m_currentLcscId = savedLcscId;

        // 清理 NetworkUtils
        networkUtils->deleteLater();
    }

    void EasyedaApi::handleRequestError(NetworkUtils *networkUtils, const QString &lcscId, const QString &error)
    {
        qWarning() << "Request error for" << lcscId << ":" << error;
        emit fetchError(error);
        networkUtils->deleteLater();
    }

    void EasyedaApi::handleBinaryDataFetched(NetworkUtils *networkUtils, const QString &lcscId, const QByteArray &data)
    {
        // qDebug() << "Binary data fetched for:" << lcscId << "Size:" << data.size();
        emit model3DFetched(lcscId, data);
        networkUtils->deleteLater();
    }

    void EasyedaApi::cancelRequest()
    {
        m_networkUtils->cancelRequest();
        m_isFetching = false;
    }

    void EasyedaApi::handleComponentInfoResponse(const QJsonObject &data)
    {
        m_isFetching = false;

        if (data.isEmpty())
        {
            QString errorMsg = QString("Empty response for LCSC ID: %1").arg(m_currentLcscId);
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        // 检查响应是否包含错�?
        if (data.contains("success") && data["success"].toBool() == false)
        {
            QString errorMsg = QString("API returned error for LCSC ID: %1").arg(m_currentLcscId);
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        // 发送成功信�?
        emit componentInfoFetched(data);
    }

    void EasyedaApi::handleCadDataResponse(const QJsonObject &data)
    {
        m_isFetching = false;

        if (data.isEmpty())
        {
            QString errorMsg = QString("Empty response for LCSC ID: %1").arg(m_currentLcscId);
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        // 检查是否包�?result �?
        if (!data.contains("result"))
        {
            QString errorMsg = QString("Response missing 'result' key for LCSC ID: %1").arg(m_currentLcscId);
            qWarning() << errorMsg;
            emit fetchError(errorMsg);
            return;
        }

        QJsonObject result = data["result"].toObject();

        // 添加 LCSC ID �?result 对象�?
        result["lcscId"] = m_currentLcscId;

        // 发送成功信�?
        emit cadDataFetched(result);
    }

    void EasyedaApi::handleModel3DResponse(const QJsonObject &data)
    {
        m_isFetching = false;

        // 注意�?D 模型数据可能是二进制数据，这里需要特殊处�?
        // 检查是否有二进制数�?
        if (data.contains("binaryData"))
        {
            QByteArray binaryData = QByteArray::fromBase64(data["binaryData"].toString().toUtf8());
            emit model3DFetched(m_currentUuid, binaryData);
        }
        else
        {
            // 如果没有二进制数据，发送空数据
            emit model3DFetched(m_currentUuid, QByteArray());
        }
    }

    void EasyedaApi::handleNetworkError(const QString &errorMessage)
    {
        m_isFetching = false;
        qWarning() << "Network error:" << errorMessage;
        emit fetchError(errorMessage);
    }

    void EasyedaApi::resetRequestState()
    {
        // 重置 NetworkUtils 的期望数据类型为 JSON（非二进制）
        m_networkUtils->setExpectBinaryData(false);
        // qDebug() << "Request state reset - expecting JSON data";
    }

    QString EasyedaApi::buildComponentApiUrl(const QString &lcscId) const
    {
        return API_ENDPOINT.arg(lcscId);
    }

    QString EasyedaApi::build3DModelObjUrl(const QString &uuid) const
    {
        return ENDPOINT_3D_MODEL.arg(uuid);
    }

    QString EasyedaApi::build3DModelStepUrl(const QString &uuid) const
    {
        return ENDPOINT_3D_MODEL_STEP.arg(uuid);
    }

    bool EasyedaApi::validateLcscId(const QString &lcscId) const
    {
        if (lcscId.isEmpty())
        {
            return false;
        }

        if (!lcscId.startsWith('C', Qt::CaseInsensitive))
        {
            return false;
        }

        // 检查后面是否为数字
        QString numberPart = lcscId.mid(1);
        if (numberPart.isEmpty())
        {
            return false;
        }

        bool ok;
        numberPart.toInt(&ok);
        return ok;
    }

} // namespace EasyKiConverter
