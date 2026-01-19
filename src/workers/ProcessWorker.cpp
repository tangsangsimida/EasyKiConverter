#include "ProcessWorker.h"
#include "src/core/easyeda/EasyedaImporter.h"
#include "src/core/easyeda/EasyedaApi.h"
#include "src/core/utils/NetworkUtils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <zlib.h>
#include <QDebug>

namespace EasyKiConverter {

ProcessWorker::ProcessWorker(const ComponentExportStatus &status, QObject *parent)
    : QObject(parent)
    , m_status(status)
{
}

ProcessWorker::~ProcessWorker()
{
    if (m_networkAccessManager) {
        m_networkAccessManager->deleteLater();
    }
}

void ProcessWorker::run()
{
    qDebug() << "ProcessWorker started for component:" << m_status.componentId;

    // 在工作线程中创建 QNetworkAccessManager
    m_networkAccessManager = new QNetworkAccessManager();
    m_networkAccessManager->moveToThread(QThread::currentThread());

    // 创建导入器
    EasyedaImporter importer;

    // 解析组件信息
    if (!parseComponentInfo(m_status)) {
        m_status.processSuccess = false;
        m_status.processMessage = "Failed to parse component info";
        emit processCompleted(m_status);
        cleanup();
        return;
    }

    // 解析CAD数据
    if (!parseCadData(m_status)) {
        m_status.processSuccess = false;
        m_status.processMessage = "Failed to parse CAD data";
        emit processCompleted(m_status);
        cleanup();
        return;
    }

    // 下载3D模型数据（如果需要）
    if (m_status.need3DModel && m_status.footprintData && !m_status.footprintData->model3D().uuid().isEmpty()) {
        if (!fetch3DModelData(m_status)) {
            qWarning() << "Failed to fetch 3D model data for:" << m_status.componentId;
            // 3D模型下载失败不影响整体流程
        } else {
            // 解析3D模型数据
            if (!parse3DModelData(m_status)) {
                qWarning() << "Failed to parse 3D model data for:" << m_status.componentId;
                // 3D模型解析失败不影响整体流程
            }
        }
    }

    m_status.processSuccess = true;
    m_status.processMessage = "Process completed successfully";
    qDebug() << "ProcessWorker completed successfully for component:" << m_status.componentId;

    emit processCompleted(m_status);
    cleanup();
}

bool ProcessWorker::parseComponentInfo(ComponentExportStatus &status)
{
    if (status.componentInfoRaw.isEmpty()) {
        status.componentData = QSharedPointer<ComponentData>::create();
        status.componentData->setLcscId(status.componentId);
        return true;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(status.componentInfoRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse component info JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject obj = doc.object();
    status.componentData = QSharedPointer<ComponentData>::create();
    status.componentData->setLcscId(status.componentId);

    if (obj.contains("name")) {
        status.componentData->setName(obj["name"].toString());
    }

    return true;
}

bool ProcessWorker::parseCadData(ComponentExportStatus &status)
{
    if (status.cadDataRaw.isEmpty()) {
        return true;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(status.cadDataRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse CAD data JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject rootObj = doc.object();
    
    // API 返回的数据在 result 字段中，需要提取出来
    QJsonObject obj;
    if (rootObj.contains("result") && rootObj["result"].isObject()) {
        obj = rootObj["result"].toObject();
        qDebug() << "Extracted CAD data from 'result' field";
    } else {
        // 如果没有 result 字段，直接使用根对象
        obj = rootObj;
    }

    // 使用EasyedaImporter导入符号和封装数据
    EasyedaImporter importer;

    // 导入符号数据
    status.symbolData = importer.importSymbolData(obj);

    // 导入封装数据
    status.footprintData = importer.importFootprintData(obj);

    return true;
}

bool ProcessWorker::fetch3DModelData(ComponentExportStatus &status)
{
    if (!status.footprintData || status.footprintData->model3D().uuid().isEmpty()) {
        qDebug() << "No 3D model UUID available";
        return false;
    }

    QString uuid = status.footprintData->model3D().uuid();
    qDebug() << "Fetching 3D model for UUID:" << uuid;

    // 下载OBJ格式的3D模型
    QString objUrl = QString("https://modules.easyeda.com/3dmodel/%1").arg(uuid);
    QByteArray objData = httpGet(objUrl, 60000); // 60秒超时

    if (objData.isEmpty()) {
        qWarning() << "Failed to download 3D model from:" << objUrl;
        return false;
    }

    qDebug() << "Downloaded 3D model data size:" << objData.size();

    // 检查是否是ZIP格式（ZIP文件以PK开头）
    QByteArray actualObjData = objData;
    if (objData.size() >= 2 && objData[0] == 0x50 && objData[1] == 0x4B) {
        qDebug() << "3D model data is ZIP compressed, decompressing...";
        actualObjData = decompressZip(objData);
        qDebug() << "Decompressed 3D model data size:" << actualObjData.size();
    }

    // 检查是否是gzip格式
    if (actualObjData.size() >= 2 && (unsigned char)actualObjData[0] == 0x1f && (unsigned char)actualObjData[1] == 0x8b) {
        qDebug() << "3D model data is gzip compressed, decompressing...";
        actualObjData = decompressGzip(actualObjData);
        qDebug() << "Decompressed 3D model data size:" << actualObjData.size();
    }

    if (actualObjData.isEmpty()) {
        qWarning() << "Failed to decompress 3D model data";
        return false;
    }

    status.model3DObjRaw = actualObjData;
    qDebug() << "3D model (OBJ) data fetched successfully for:" << status.componentId;

    // 下载STEP格式的3D模型
    QString stepUrl = QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(uuid);
    QByteArray stepData = httpGet(stepUrl, 60000); // 60秒超时

    if (!stepData.isEmpty() && stepData.size() > 100) { // STEP文件通常大于100字节
        status.model3DStepRaw = stepData;
        qDebug() << "3D model (STEP) data fetched successfully for:" << status.componentId << "Size:" << stepData.size();
    } else {
        qWarning() << "Failed to download STEP model or STEP data too small for:" << status.componentId;
    }

    return true;
}

bool ProcessWorker::parse3DModelData(ComponentExportStatus &status)
{
    if (status.model3DObjRaw.isEmpty()) {
        return true;
    }

    // 创建3D模型数据
    status.model3DData = QSharedPointer<Model3DData>::create();
    status.model3DData->setRawObj(QString::fromUtf8(status.model3DObjRaw));

    // 从 footprintData 的 model3D 中获取 UUID
    if (status.footprintData && !status.footprintData->model3D().uuid().isEmpty()) {
        status.model3DData->setUuid(status.footprintData->model3D().uuid());
        qDebug() << "Using 3D model UUID from footprintData:" << status.model3DData->uuid();
    }

    return true;
}

QByteArray ProcessWorker::httpGet(const QString &url, int timeoutMs)
{
    QByteArray result;
    
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "EasyKiConverter/1.0");
    request.setRawHeader("Accept", "application/octet-stream, */*");
    
    QNetworkReply *reply = m_networkAccessManager->get(request);
    
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
    } else {
        qWarning() << "HTTP error:" << reply->errorString() << "URL:" << url;
    }
    
    reply->deleteLater();
    return result;
}

QByteArray ProcessWorker::decompressGzip(const QByteArray &compressedData)
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

QByteArray ProcessWorker::decompressZip(const QByteArray &zipData)
{
    if (zipData.size() < 30) {
        qWarning() << "Invalid ZIP data size";
        return QByteArray();
    }

    // 解析ZIP文件格式
    QByteArray result;
    int pos = 0;

    while (pos < zipData.size() - 30) {
        // 查找本地文件头签名 (0x04034b50)
        if (zipData[pos] != 0x50 || zipData[pos + 1] != 0x03 ||
            zipData[pos + 2] != 0x04 || zipData[pos + 3] != 0x4b) {
            pos++;
            continue;
        }

        // 读取本地文件头
        // 跳过版本、标志、压缩方法等字段
        // 读取压缩后的大小 (offset 18)
        std::uint16_t compressedSize = ((std::uint8_t)zipData[pos + 18] | ((std::uint8_t)zipData[pos + 19] << 8));
        // 读取未压缩的大小 (offset 22)
        std::uint16_t uncompressedSize = ((std::uint8_t)zipData[pos + 22] | ((std::uint8_t)zipData[pos + 23] << 8));
        // 读取文件名长度 (offset 26)
        std::uint16_t fileNameLen = ((std::uint8_t)zipData[pos + 26] | ((std::uint8_t)zipData[pos + 27] << 8));
        // 读取额外字段长度 (offset 28)
        std::uint16_t extraFieldLen = ((std::uint8_t)zipData[pos + 28] | ((std::uint8_t)zipData[pos + 29] << 8));

        // 跳过文件名和额外字段
        int dataStart = pos + 30 + fileNameLen + extraFieldLen;

        // 检查压缩方法 (offset 8)
        std::uint16_t compressionMethod = ((std::uint8_t)zipData[pos + 8] | ((std::uint8_t)zipData[pos + 9] << 8));

        if (compressionMethod == 8) { // DEFLATE
            // 解压数据
            QByteArray compressedData = zipData.mid(dataStart, compressedSize);
            QByteArray decompressedData = decompressGzip(compressedData);
            result.append(decompressedData);
        } else if (compressionMethod == 0) { // 不压缩
            QByteArray uncompressedData = zipData.mid(dataStart, uncompressedSize);
            result.append(uncompressedData);
        } else {
            qWarning() << "Unsupported ZIP compression method:" << compressionMethod;
        }

        // 移动到下一个文件
        pos = dataStart + compressedSize;
    }

    return result;
}

void ProcessWorker::cleanup()
{
    if (m_networkAccessManager) {
        m_networkAccessManager->deleteLater();
        m_networkAccessManager = nullptr;
    }
}

} // namespace EasyKiConverter