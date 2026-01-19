#include "FetchWorker.h"
#include "src/core/easyeda/EasyedaApi.h"
#include <QEventLoop>
#include <QTimer>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <zlib.h>
#include <QDebug>

namespace EasyKiConverter {

FetchWorker::FetchWorker(
    const QString &componentId,
    QNetworkAccessManager *networkAccessManager,
    bool need3DModel,
    QObject *parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_networkAccessManager(networkAccessManager)
    , m_need3DModel(need3DModel)
{
}

FetchWorker::~FetchWorker()
{
}

void FetchWorker::run()
{
    qDebug() << "FetchWorker started for component:" << m_componentId;

    ComponentExportStatus status;
    status.componentId = m_componentId;
    status.need3DModel = m_need3DModel;

    // 创建EasyedaApi实例
    EasyedaApi *api = new EasyedaApi(this);

    // 使用QEventLoop等待异步操作完成
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    bool componentInfoFetched = false;
    bool cadDataFetched = false;
    bool model3DFetched = !m_need3DModel;  // 如果不需要3D模型，标记为已获取
    bool hasError = false;
    QString errorMessage;

    // 连接信号
    connect(api, &EasyedaApi::componentInfoFetched, [&](const QJsonObject &data) {
        status.componentInfoRaw = QJsonDocument(data).toJson(QJsonDocument::Compact);
        componentInfoFetched = true;
        if (cadDataFetched && model3DFetched) {
            loop.quit();
        }
    });

    connect(api, &EasyedaApi::cadDataFetched, [&](const QJsonObject &data) {
        status.cadDataRaw = QJsonDocument(data).toJson(QJsonDocument::Compact);
        cadDataFetched = true;
        
        // 检查是否需要3D模型
        if (data.contains("c_para") && data["c_para"].isObject()) {
            QJsonObject cPara = data["c_para"].toObject();
            if (cPara.contains("uuid")) {
                QString uuid = cPara["uuid"].toString();
                if (!uuid.isEmpty() && m_need3DModel) {
                    // 获取3D模型
                    api->fetch3DModelObj(uuid);
                    return;
                }
            }
        }
        
        // 不需要3D模型或没有uuid
        model3DFetched = true;
        if (componentInfoFetched && model3DFetched) {
            loop.quit();
        }
    });

    connect(api, &EasyedaApi::model3DFetched, [&](const QString &uuid, const QByteArray &data) {
        Q_UNUSED(uuid);
        status.model3DObjRaw = data;
        model3DFetched = true;
        if (componentInfoFetched && model3DFetched) {
            loop.quit();
        }
    });

    connect(api, &EasyedaApi::fetchError, [&](const QString &error) {
        hasError = true;
        errorMessage = error;
        loop.quit();
    });

    // 设置超时
    connect(&timeoutTimer, &QTimer::timeout, [&]() {
        hasError = true;
        errorMessage = "Fetch timeout";
        api->cancelRequest();
        loop.quit();
    });

    // 开始抓取
    timeoutTimer.start(60000);  // 60秒超时
    api->fetchComponentInfo(m_componentId);
    
    // 等待完成
    loop.exec();

    // 清理
    api->deleteLater();

    // 设置状态
    if (hasError) {
        status.fetchSuccess = false;
        status.fetchMessage = errorMessage;
        qDebug() << "FetchWorker failed for component:" << m_componentId << "Error:" << errorMessage;
    } else {
        status.fetchSuccess = componentInfoFetched && cadDataFetched && model3DFetched;
        if (status.fetchSuccess) {
            status.fetchMessage = "Fetch completed successfully";
            qDebug() << "FetchWorker completed successfully for component:" << m_componentId;
        } else {
            status.fetchMessage = "Fetch incomplete";
            qDebug() << "FetchWorker incomplete for component:" << m_componentId;
        }
    }

    emit fetchCompleted(status);
}

QByteArray FetchWorker::httpGet(const QString &url, int timeoutMs)
{
    Q_UNUSED(url);
    Q_UNUSED(timeoutMs);
    // 这个方法不再使用，改用EasyedaApi
    return QByteArray();
}

QByteArray FetchWorker::decompressGzip(const QByteArray &compressedData)
{
    Q_UNUSED(compressedData);
    // 这个方法不再使用，改用EasyedaApi
    return QByteArray();
}

} // namespace EasyKiConverter