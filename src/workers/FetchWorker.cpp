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

namespace EasyKiConverter
{

    FetchWorker::FetchWorker(
        const QString &componentId,
        QNetworkAccessManager *networkAccessManager,
        bool need3DModel,
        QObject *parent)
        : QObject(parent), m_componentId(componentId), m_networkAccessManager(networkAccessManager), m_ownNetworkManager(nullptr), m_need3DModel(need3DModel)
    {
    }

    FetchWorker::~FetchWorker()
    {
        if (m_ownNetworkManager)
        {
            m_ownNetworkManager->deleteLater();
        }
    }

    void FetchWorker::run()
    {
        // 启动计时器
        QElapsedTimer fetchTimer;
        fetchTimer.start();

        // 在工作线程中创建自己的 QNetworkAccessManager
        m_ownNetworkManager = new QNetworkAccessManager();
        m_ownNetworkManager->moveToThread(QThread::currentThread());

        // 使用 QSharedPointer 创建 ComponentExportStatus（避免拷贝）
        QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
        status->componentId = m_componentId;
        status->need3DModel = m_need3DModel;

        status->addDebugLog(QString("FetchWorker started for component: %1").arg(m_componentId));

        // 使用同步方式获取数据（避免QEventLoop死锁）
        bool hasError = false;
        QString errorMessage;

        // 1. 获取组件信息（包含CAD数据）
        QString componentInfoUrl = QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId);
        status->addDebugLog(QString("Fetching component info from: %1").arg(componentInfoUrl));

        QByteArray componentInfoData = httpGet(componentInfoUrl, 30000);

        if (componentInfoData.isEmpty())
        {
            hasError = true;
            errorMessage = "Failed to fetch component info";
            status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
        }
        else
        {
            status->componentInfoRaw = componentInfoData;
            status->cinfoJsonRaw = componentInfoData; // cinfo JSON 原始数据
            status->cadDataRaw = componentInfoData;   // CAD数据和组件信息在同一个响应中
            status->cadJsonRaw = componentInfoData;   // cad JSON 原始数据
            status->addDebugLog(QString("Component info (including CAD) fetched for: %1, Size: %2 bytes").arg(m_componentId).arg(componentInfoData.size()));

            // 2. 如果需要3D模型，解析CAD数据获取UUID并下载3D模型
            if (m_need3DModel && !hasError)
            {
                status->addDebugLog("Parsing CAD data to extract 3D model UUID...");

                // 快速解析CAD数据以获取3D模型UUID
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(componentInfoData, &parseError);

                if (parseError.error == QJsonParseError::NoError)
                {
                    QJsonObject rootObj = doc.object();
                    QJsonObject obj;

                    // API 返回的数据在 result 字段中
                    if (rootObj.contains("result") && rootObj["result"].isObject())
                    {
                        obj = rootObj["result"].toObject();
                    }
                    else
                    {
                        obj = rootObj;
                    }

                    // 查找3D模型UUID
                    if (obj.contains("footprint") && obj["footprint"].isObject())
                    {
                        QJsonObject footprint = obj["footprint"].toObject();
                        if (footprint.contains("model3D") && footprint["model3D"].isObject())
                        {
                            QJsonObject model3D = footprint["model3D"].toObject();
                            if (model3D.contains("uuid") && !model3D["uuid"].toString().isEmpty())
                            {
                                QString uuid = model3D["uuid"].toString();
                                status->addDebugLog(QString("Found 3D model UUID: %1").arg(uuid));

                                // 下载3D模型
                                if (!fetch3DModelData(*status))
                                {
                                    status->addDebugLog(QString("WARNING: Failed to fetch 3D model data for: %1").arg(m_componentId));
                                    // 3D模型下载失败不影响整体流程
                                }
                            }
                        }
                    }
                }
                else
                {
                    status->addDebugLog(QString("WARNING: Failed to parse CAD data for 3D model UUID: %1").arg(parseError.errorString()));
                }
            }
        }

        // 清理网络管理器
        if (m_ownNetworkManager)
        {
            m_ownNetworkManager->deleteLater();
            m_ownNetworkManager = nullptr;
        }

        // 记录抓取耗时
        status->fetchDurationMs = fetchTimer.elapsed();

        // 设置状态
        if (hasError)
        {
            status->fetchSuccess = false;
            status->fetchMessage = errorMessage;
            status->addDebugLog(QString("FetchWorker failed for component: %1, Error: %2, Duration: %3ms").arg(m_componentId).arg(errorMessage).arg(status->fetchDurationMs));
        }
        else
        {
            status->fetchSuccess = true;
            status->fetchMessage = "Fetch completed successfully";
            status->addDebugLog(QString("FetchWorker completed successfully for component: %1, Duration: %2ms").arg(m_componentId).arg(status->fetchDurationMs));
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
        connect(&timeoutTimer, &QTimer::timeout, [&]()
                {
        reply->abort();
        loop.quit(); });

        timeoutTimer.start(timeoutMs);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError)
        {
            result = reply->readAll();

            // 检查是否是gzip压缩
            if (!result.isEmpty() && result.size() >= 2 &&
                (unsigned char)result[0] == 0x1f && (unsigned char)result[1] == 0x8b)
            {
                result = decompressGzip(result);
            }
        }
        else
        {
            qWarning() << "HTTP error:" << reply->errorString() << "URL:" << url;
        }

        reply->deleteLater();
        return result;
    }

    QByteArray FetchWorker::decompressGzip(const QByteArray &compressedData)
    {
        if (compressedData.size() < 18)
        {
            qWarning() << "Invalid gzip data size";
            return QByteArray();
        }

        z_stream stream;
        memset(&stream, 0, sizeof(stream));

        // 初始化 zlib
        if (inflateInit2(&stream, 15 + 16) != Z_OK)
        {
            qWarning() << "Failed to initialize zlib";
            return QByteArray();
        }

        stream.next_in = (Bytef *)compressedData.data();
        stream.avail_in = compressedData.size();
        stream.next_out = nullptr;
        stream.avail_out = 0;

        // 解压
        QByteArray decompressed;
        const int bufferSize = 8192;
        char buffer[bufferSize];

        int ret;
        do
        {
            stream.avail_out = bufferSize;
            stream.next_out = (Bytef *)buffer;

            ret = inflate(&stream, Z_NO_FLUSH);

            if (ret == Z_OK || ret == Z_STREAM_END)
            {
                decompressed.append(buffer, bufferSize - stream.avail_out);
            }
        } while (ret == Z_OK);

        inflateEnd(&stream);

        if (ret != Z_STREAM_END)
        {
            qWarning() << "Failed to decompress gzip data, error:" << ret;
            return QByteArray();
        }

        return decompressed;
    }

    QByteArray FetchWorker::decompressZip(const QByteArray &zipData)
    {
        if (zipData.size() < 30)
        {
            qWarning() << "Invalid ZIP data size";
            return QByteArray();
        }

        // 解析ZIP文件格式
        QByteArray result;
        int pos = 0;

        while (pos < zipData.size() - 30)
        {
            // 查找本地文件头签名 (0x04034b50)
            if (zipData[pos] != 0x50 || zipData[pos + 1] != 0x03 ||
                zipData[pos + 2] != 0x04 || zipData[pos + 3] != 0x4b)
            {
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

            if (compressionMethod == 8)
            { // DEFLATE
                // 解压数据
                QByteArray compressedData = zipData.mid(dataStart, compressedSize);
                QByteArray decompressedData = decompressGzip(compressedData);
                result.append(decompressedData);
            }
            else if (compressionMethod == 0)
            { // 不压缩
                QByteArray uncompressedData = zipData.mid(dataStart, uncompressedSize);
                result.append(uncompressedData);
            }
            else
            {
                qWarning() << "Unsupported ZIP compression method:" << compressionMethod;
            }

            // 移动到下一个文件
            pos = dataStart + compressedSize;
        }

        return result;
    }

    bool FetchWorker::fetch3DModelData(ComponentExportStatus &status)
    {
        // 从CAD数据中提取3D模型UUID
        QString uuid;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(status.cadDataRaw, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            status.addDebugLog("Failed to parse CAD data for 3D model UUID");
            return false;
        }

        QJsonObject rootObj = doc.object();
        QJsonObject obj;

        if (rootObj.contains("result") && rootObj["result"].isObject())
        {
            obj = rootObj["result"].toObject();
        }
        else
        {
            obj = rootObj;
        }

        // 查找3D模型UUID
        if (obj.contains("footprint") && obj["footprint"].isObject())
        {
            QJsonObject footprint = obj["footprint"].toObject();
            if (footprint.contains("model3D") && footprint["model3D"].isObject())
            {
                QJsonObject model3D = footprint["model3D"].toObject();
                if (model3D.contains("uuid") && !model3D["uuid"].toString().isEmpty())
                {
                    uuid = model3D["uuid"].toString();
                }
            }
        }

        if (uuid.isEmpty())
        {
            status.addDebugLog("No 3D model UUID found in CAD data");
            return false;
        }

        status.addDebugLog(QString("Fetching 3D model for UUID: %1").arg(uuid));

        // 下载OBJ格式的3D模型
        QString objUrl = QString("https://modules.easyeda.com/3dmodel/%1").arg(uuid);
        status.addDebugLog(QString("Downloading 3D model from: %1").arg(objUrl));
        QByteArray objData = httpGet(objUrl, 60000); // 60秒超时

        if (objData.isEmpty())
        {
            status.addDebugLog(QString("ERROR: Failed to download 3D model from: %1").arg(objUrl));
            return false;
        }

        status.addDebugLog(QString("Downloaded 3D model data size: %1 bytes").arg(objData.size()));

        // 检查是否是ZIP格式（ZIP文件以PK开头）
        QByteArray actualObjData = objData;
        if (objData.size() >= 2 && objData[0] == 0x50 && objData[1] == 0x4B)
        {
            status.addDebugLog("3D model data is ZIP compressed, decompressing...");
            actualObjData = decompressZip(objData);
            status.addDebugLog(QString("Decompressed 3D model data size: %1 bytes").arg(actualObjData.size()));
        }

        // 检查是否是gzip格式
        if (actualObjData.size() >= 2 && (unsigned char)actualObjData[0] == 0x1f && (unsigned char)actualObjData[1] == 0x8b)
        {
            status.addDebugLog("3D model data is gzip compressed, decompressing...");
            actualObjData = decompressGzip(actualObjData);
            status.addDebugLog(QString("Decompressed 3D model data size: %1 bytes").arg(actualObjData.size()));
        }

        if (actualObjData.isEmpty())
        {
            status.addDebugLog("ERROR: Failed to decompress 3D model data");
            return false;
        }

        status.model3DObjRaw = actualObjData;
        status.addDebugLog(QString("3D model (OBJ) data fetched successfully for: %1").arg(status.componentId));

        // 下载STEP格式的3D模型
        QString stepUrl = QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(uuid);
        status.addDebugLog(QString("Downloading STEP model from: %1").arg(stepUrl));
        QByteArray stepData = httpGet(stepUrl, 60000); // 60秒超时

        if (!stepData.isEmpty() && stepData.size() > 100)
        { // STEP文件通常大于100字节
            status.model3DStepRaw = stepData;
            status.addDebugLog(QString("3D model (STEP) data fetched successfully for: %1, Size: %2 bytes").arg(status.componentId).arg(stepData.size()));
        }
        else
        {
            status.addDebugLog(QString("WARNING: Failed to download STEP model or STEP data too small for: %1").arg(status.componentId));
        }

        return true;
    }

} // namespace EasyKiConverter
