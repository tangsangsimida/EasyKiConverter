#include "FetchWorker.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
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
    , m_ownNetworkManager(nullptr)
    , m_need3DModel(need3DModel)
{
}

FetchWorker::~FetchWorker()
{
    if (m_ownNetworkManager) {
        m_ownNetworkManager->deleteLater();
    }
}

void FetchWorker::run()
{
    // 在工作线程中创建自己的 QNetworkAccessManager
    m_ownNetworkManager = new QNetworkAccessManager();
    m_ownNetworkManager->moveToThread(QThread::currentThread());

    ComponentExportStatus status;
    status.componentId = m_componentId;
    status.need3DModel = m_need3DModel;

    status.addDebugLog(QString("FetchWorker started for component: %1").arg(m_componentId));

    // 使用同步方式获取数据（避免QEventLoop死锁）
    bool hasError = false;
    QString errorMessage;

    // 1. 获取组件信息（包含CAD数据）
    QString componentInfoUrl = QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId);
    status.addDebugLog(QString("Fetching component info from: %1").arg(componentInfoUrl));

    QByteArray componentInfoData = httpGet(componentInfoUrl, 30000);

    if (componentInfoData.isEmpty()) {
        hasError = true;
        errorMessage = "Failed to fetch component info";
        status.addDebugLog(QString("ERROR: %1").arg(errorMessage));
    } else {
        status.componentInfoRaw = componentInfoData;
        status.cinfoJsonRaw = componentInfoData;  // cinfo JSON 原始数据
        status.cadDataRaw = componentInfoData;    // CAD数据和组件信息在同一个响应中
        status.cadJsonRaw = componentInfoData;    // cad JSON 原始数据
        status.addDebugLog(QString("Component info (including CAD) fetched for: %1, Size: %2 bytes").arg(m_componentId).arg(componentInfoData.size()));
    }

    // 清理网络管理器
    if (m_ownNetworkManager) {
        m_ownNetworkManager->deleteLater();
        m_ownNetworkManager = nullptr;
    }

    // 设置状态
    if (hasError) {
        status.fetchSuccess = false;
        status.fetchMessage = errorMessage;
        status.addDebugLog(QString("FetchWorker failed for component: %1, Error: %2").arg(m_componentId).arg(errorMessage));
    } else {
        status.fetchSuccess = true;
        status.fetchMessage = "Fetch completed successfully";
        status.addDebugLog(QString("FetchWorker completed successfully for component: %1").arg(m_componentId));
    }

    emit fetchCompleted(status);
}

QByteArray FetchWorker::httpGet(const QString &url, int timeoutMs)
{
    QByteArray result;
    
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "EasyKiConverter/1.0");
    request.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");
    
    QNetworkReply *reply = m_ownNetworkManager->get(request);
    
    // 使用QEventLoop等待响应
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, [&]() {
        reply->abort();
        loop.quit();
    });
    
    timeoutTimer.start(timeoutMs);
    loop.exec();
    
    if (reply->error() == QNetworkReply::NoError) {
        result = reply->readAll();
        
        // 检查是否是gzip压缩
        if (!result.isEmpty() && result.size() >= 2 && 
            (unsigned char)result[0] == 0x1f && (unsigned char)result[1] == 0x8b) {
            result = decompressGzip(result);
        }
    } else {
        qWarning() << "HTTP error:" << reply->errorString() << "URL:" << url;
    }
    
    reply->deleteLater();
    return result;
}

QByteArray FetchWorker::decompressGzip(const QByteArray &compressedData)
{
    if (compressedData.size() < 18) {
        qWarning() << "Invalid gzip data size";
        return QByteArray();
    }
    
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    
    // 初始化 zlib
    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        qWarning() << "Failed to initialize zlib";
        return QByteArray();
    }
    
    stream.next_in = (Bytef*)compressedData.data();
    stream.avail_in = compressedData.size();
    stream.next_out = nullptr;
    stream.avail_out = 0;
    
    // 解压
    QByteArray decompressed;
    const int bufferSize = 8192;
    char buffer[bufferSize];
    
    int ret;
    do {
        stream.avail_out = bufferSize;
        stream.next_out = (Bytef*)buffer;
        
        ret = inflate(&stream, Z_NO_FLUSH);
        
        if (ret == Z_OK || ret == Z_STREAM_END) {
            decompressed.append(buffer, bufferSize - stream.avail_out);
        }
    } while (ret == Z_OK);
    
    inflateEnd(&stream);
    
    if (ret != Z_STREAM_END) {
        qWarning() << "Failed to decompress gzip data, error:" << ret;
        return QByteArray();
    }
    
    return decompressed;
}

} // namespace EasyKiConverter