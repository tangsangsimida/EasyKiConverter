#include "FetchWorker.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

#include <zlib.h>

namespace EasyKiConverter {

// 速率限制检测静态变量初始化
QAtomicInt FetchWorker::s_activeRequests = 0;
QMutex FetchWorker::s_rateLimitMutex;
QDateTime FetchWorker::s_lastRateLimitTime;
int FetchWorker::s_backoffMs = 0;

FetchWorker::FetchWorker(const QString& componentId,
                         QNetworkAccessManager* networkAccessManager,
                         bool need3DModel,
                         QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_networkAccessManager(networkAccessManager)
    , m_ownNetworkManager(nullptr)
    , m_need3DModel(need3DModel) {}

FetchWorker::~FetchWorker() {
    // 不要删除 m_ownNetworkManager，因为它现在是线程局部存储的共享实例
    // if (m_ownNetworkManager) {
    //     m_ownNetworkManager->deleteLater();
    // }
}

void FetchWorker::run() {
    QElapsedTimer fetchTimer;
    fetchTimer.start();

    // 创建状态对象
    QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
    status->componentId = m_componentId;
    status->need3DModel = m_need3DModel;

    status->addDebugLog(QString("FetchWorker started for component: %1").arg(m_componentId));

    // 速率限制：如果最近被限流，延迟启动
    if (s_backoffMs > 0) {
        QMutexLocker locker(&s_rateLimitMutex);
        QDateTime now = QDateTime::currentDateTime();
        qint64 elapsedSinceLimit = s_lastRateLimitTime.msecsTo(now);
        if (elapsedSinceLimit < s_backoffMs * 2) {
            // 如果距离上次限流时间较短，应用退避延迟
            int delay = qMin(s_backoffMs, 5000);
            status->addDebugLog(QString("Rate limit backoff: delaying %1ms").arg(delay));
            QThread::msleep(delay);
        } else {
            // 超过限流冷却时间，重置退避延迟
            s_backoffMs = 0;
        }
    }

    // 增加活跃请求计数
    s_activeRequests.fetchAndAddRelaxed(1);

    // 使用线程局部存储缓存 QNetworkAccessManager
    // 避免为每个任务重复创建和销毁 QNAM（这是非常昂贵的操作）
    static thread_local QNetworkAccessManager* threadQNAM = nullptr;
    if (!threadQNAM) {
        threadQNAM = new QNetworkAccessManager();
        // QNAM 创建于当前线程，自动归属于当前线程
        qDebug() << "Created thread-local QNetworkAccessManager for thread:" << QThread::currentThreadId();
    }
    m_ownNetworkManager = threadQNAM;

    bool hasError = false;
    QString errorMessage;

    QString componentInfoUrl =
        QString("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(m_componentId);
    status->addDebugLog(QString("Fetching component info from: %1").arg(componentInfoUrl));

    QByteArray componentInfoData = httpGet(componentInfoUrl, 8000, status);

    if (componentInfoData.isEmpty()) {
        hasError = true;
        errorMessage = "Failed to fetch component info (empty response)";
        status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
    } else {
        // Validate JSON content
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(componentInfoData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            hasError = true;
            errorMessage = "Invalid JSON response: " + parseError.errorString();
            status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
        } else {
            QJsonObject rootObj = doc.object();

            // Check for API specific error fields
            if (rootObj.contains("success") && !rootObj["success"].toBool()) {
                hasError = true;
                errorMessage = "API returned error: " +
                               (rootObj.contains("message") ? rootObj["message"].toString() : "Unknown error");
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            }
            // Check if result exists and is valid
            else if (!rootObj.contains("result") || rootObj["result"].isNull()) {
                hasError = true;
                errorMessage = "Component not found (null result)";
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            }
            // Check if result is empty object (sometimes happens for invalid IDs)
            else if (rootObj["result"].isObject() && rootObj["result"].toObject().isEmpty()) {
                hasError = true;
                errorMessage = "Component data is empty";
                status->addDebugLog(QString("ERROR: %1").arg(errorMessage));
            } else {
                // Data seems valid
                status->componentInfoRaw = componentInfoData;
                status->cinfoJsonRaw = componentInfoData;
                status->cadDataRaw = componentInfoData;
                status->cadJsonRaw = componentInfoData;
                status->addDebugLog(QString("Component info (including CAD) fetched for: %1, Size: %2 bytes")
                                        .arg(m_componentId)
                                        .arg(componentInfoData.size()));

                if (m_need3DModel && !hasError) {
                    status->addDebugLog("Parsing CAD data to extract 3D model UUID...");
                    // Re-use doc/rootObj since we already parsed it
                    QJsonObject obj;
                    if (rootObj.contains("result") && rootObj["result"].isObject()) {
                        obj = rootObj["result"].toObject();
                    } else {
                        obj = rootObj;
                    }

                    if (!fetch3DModelData(status)) {
                        // 3D model fetch failure is not fatal for the whole component
                    }
                }
            }
        }
    }

    // 不要删除或清空 m_ownNetworkManager
    m_ownNetworkManager = nullptr;  // 解除引用，防止析构函数误删（虽然析构函数已修改，但为了安全）

    // 减少活跃请求计数
    s_activeRequests.fetchAndSubRelaxed(1);

    status->fetchDurationMs = fetchTimer.elapsed();

    if (hasError) {
        status->fetchSuccess = false;
        status->fetchMessage = errorMessage;
        status->addDebugLog(QString("FetchWorker failed for component: %1, Error: %2, Duration: %3ms")
                                .arg(m_componentId)
                                .arg(errorMessage)
                                .arg(status->fetchDurationMs));
    } else {
        status->fetchSuccess = true;
        status->fetchMessage = "Fetch completed successfully";
        status->addDebugLog(QString("FetchWorker completed successfully for component: %1, Duration: %2ms")
                                .arg(m_componentId)
                                .arg(status->fetchDurationMs));
    }

    emit fetchCompleted(status);
}

QByteArray FetchWorker::httpGet(const QString& url, int timeoutMs, QSharedPointer<ComponentExportStatus> status) {
    QByteArray result;
    int retryCount = 0;
    const int maxRetries = 3;
    QElapsedTimer requestTimer;

    while (retryCount <= maxRetries) {
        requestTimer.restart();

        if (retryCount > 0) {
            int delayMs = 500;  // Fixed 500ms delay as requested

            qDebug() << "Retrying request to" << url << "in" << delayMs << "ms (Retry" << retryCount << "/"
                     << maxRetries << ")";
            QThread::msleep(delayMs);
        }

        QNetworkRequest request{QUrl(url)};
        request.setRawHeader("User-Agent", "EasyKiConverter/1.0");
        request.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");

        QNetworkReply* reply = m_ownNetworkManager->get(request);

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timeoutTimer, &QTimer::timeout, [&]() {
            if (reply->isRunning()) {
                reply->abort();
            }
            loop.quit();
        });

        timeoutTimer.start(timeoutMs);
        loop.exec();

        bool success = false;
        if (reply->error() == QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode == 200) {
                result = reply->readAll();
                success = true;

                // 记录网络诊断信息
                if (status) {
                    ComponentExportStatus::NetworkDiagnostics diag;
                    diag.url = url;
                    diag.statusCode = statusCode;
                    diag.errorString = "";
                    diag.retryCount = retryCount;
                    diag.latencyMs = requestTimer.elapsed();
                    diag.wasRateLimited = false;
                    status->networkDiagnostics.append(diag);
                }
            } else if (statusCode == 429) {
                qWarning() << "Rate limit detected (HTTP 429) for URL:" << url;
                // 触发速率限制退避
                QMutexLocker locker(&s_rateLimitMutex);
                s_lastRateLimitTime = QDateTime::currentDateTime();
                s_backoffMs = qMin(s_backoffMs + 1000, 5000);  // 指数退避，最大5秒
                qWarning() << "Rate limit backoff increased to" << s_backoffMs
                           << "ms, active requests:" << s_activeRequests.loadRelaxed();

                // 记录网络诊断信息
                if (status) {
                    ComponentExportStatus::NetworkDiagnostics diag;
                    diag.url = url;
                    diag.statusCode = statusCode;
                    diag.errorString = "Rate Limited";
                    diag.retryCount = retryCount;
                    diag.latencyMs = requestTimer.elapsed();
                    diag.wasRateLimited = true;
                    status->networkDiagnostics.append(diag);
                }
                // Will retry
            } else if (statusCode >= 500) {
                qWarning() << "HTTP error" << statusCode << "for URL:" << url;
                // Will retry
            } else {
                qWarning() << "HTTP error" << statusCode << "for URL:" << url << "(No retry for this code)";
                retryCount = maxRetries + 1;  // Don't retry for other 4xx errors
            }
        } else if (reply->error() == QNetworkReply::OperationCanceledError) {
            // 请求被取消（超时），不再重试
            qWarning() << "Request timeout (cancelled) for URL:" << url << "after" << timeoutMs << "ms";

            // 记录网络诊断信息
            if (status) {
                ComponentExportStatus::NetworkDiagnostics diag;
                diag.url = url;
                diag.statusCode = 0;
                diag.errorString = "Timeout";
                diag.retryCount = retryCount;
                diag.latencyMs = requestTimer.elapsed();
                diag.wasRateLimited = false;
                status->networkDiagnostics.append(diag);
            }

            retryCount = maxRetries + 1;  // Skip retries
        } else {
            qWarning() << "Network error:" << reply->errorString() << "URL:" << url;

            // 记录网络诊断信息
            if (status) {
                ComponentExportStatus::NetworkDiagnostics diag;
                diag.url = url;
                diag.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                diag.errorString = reply->errorString();
                diag.retryCount = retryCount;
                diag.latencyMs = requestTimer.elapsed();
                diag.wasRateLimited = false;
                status->networkDiagnostics.append(diag);
            }

            // Will retry for other network errors (except timeout/cancelled)
        }

        reply->deleteLater();

        if (success) {
            if (!result.isEmpty() && result.size() >= 2 && (unsigned char)result[0] == 0x1f &&
                (unsigned char)result[1] == 0x8b) {
                result = decompressGzip(result);
            }
            return result;
        }

        retryCount++;
    }

    return QByteArray();
}

QByteArray FetchWorker::decompressGzip(const QByteArray& compressedData) {
    if (compressedData.size() < 18) {
        qWarning() << "Invalid gzip data size";
        return QByteArray();
    }

    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        qWarning() << "Failed to initialize zlib";
        return QByteArray();
    }

    stream.next_in = (Bytef*)compressedData.data();
    stream.avail_in = compressedData.size();
    stream.next_out = nullptr;
    stream.avail_out = 0;


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

QByteArray FetchWorker::decompressZip(const QByteArray& zipData) {
    if (zipData.size() < 30) {
        qWarning() << "Invalid ZIP data size";
        return QByteArray();
    }

    QByteArray result;
    int pos = 0;

    while (pos < zipData.size() - 30) {
        if (zipData[pos] != 0x50 || zipData[pos + 1] != 0x03 || zipData[pos + 2] != 0x04 || zipData[pos + 3] != 0x4b) {
            pos++;
            continue;
        }

        std::uint16_t compressedSize = ((std::uint8_t)zipData[pos + 18] | ((std::uint8_t)zipData[pos + 19] << 8));
        std::uint16_t uncompressedSize = ((std::uint8_t)zipData[pos + 22] | ((std::uint8_t)zipData[pos + 23] << 8));
        std::uint16_t fileNameLen = ((std::uint8_t)zipData[pos + 26] | ((std::uint8_t)zipData[pos + 27] << 8));
        std::uint16_t extraFieldLen = ((std::uint8_t)zipData[pos + 28] | ((std::uint8_t)zipData[pos + 29] << 8));
        int dataStart = pos + 30 + fileNameLen + extraFieldLen;
        std::uint16_t compressionMethod = ((std::uint8_t)zipData[pos + 8] | ((std::uint8_t)zipData[pos + 9] << 8));
        if (compressionMethod == 8) {
            QByteArray compressedData = zipData.mid(dataStart, compressedSize);
            QByteArray decompressedData = decompressGzip(compressedData);
            result.append(decompressedData);
        } else if (compressionMethod == 0) {
            QByteArray uncompressedData = zipData.mid(dataStart, uncompressedSize);
            result.append(uncompressedData);
        } else {
            qWarning() << "Unsupported ZIP compression method:" << compressionMethod;
        }

        pos = dataStart + compressedSize;
    }

    return result;
}

bool FetchWorker::fetch3DModelData(QSharedPointer<ComponentExportStatus> status) {
    QString uuid;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(status->cadDataRaw, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QString msg = "Failed to parse CAD data for 3D model UUID";
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    QJsonObject rootObj = doc.object();
    QJsonObject obj;

    if (rootObj.contains("result") && rootObj["result"].isObject()) {
        obj = rootObj["result"].toObject();
    } else {
        obj = rootObj;
    }

    if (obj.contains("packageDetail") && obj["packageDetail"].isObject()) {
        QJsonObject packageDetail = obj["packageDetail"].toObject();
        if (packageDetail.contains("dataStr")) {
            QJsonObject dataObj;
            if (packageDetail["dataStr"].isObject()) {
                dataObj = packageDetail["dataStr"].toObject();
            } else if (packageDetail["dataStr"].isString()) {
                QString dataStrRaw = packageDetail["dataStr"].toString();
                if (!dataStrRaw.isEmpty()) {
                    QJsonDocument dataDoc = QJsonDocument::fromJson(dataStrRaw.toUtf8());
                    dataObj = dataDoc.object();
                }
            }

            if (!dataObj.isEmpty()) {
                if (dataObj.contains("shape") && dataObj["shape"].isArray()) {
                    QJsonArray shapes = dataObj["shape"].toArray();
                    for (const QJsonValue& shapeVal : shapes) {
                        QString shapeStr = shapeVal.toString();
                        if (shapeStr.startsWith("SVGNODE~")) {
                            int tildeIndex = shapeStr.indexOf('~');
                            if (tildeIndex != -1) {
                                QString jsonStr = shapeStr.mid(tildeIndex + 1);
                                QJsonDocument nodeDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
                                QJsonObject nodeObj = nodeDoc.object();
                                if (nodeObj.contains("attrs") && nodeObj["attrs"].isObject()) {
                                    QJsonObject attrs = nodeObj["attrs"].toObject();
                                    if (attrs.contains("uuid") && !attrs["uuid"].toString().isEmpty()) {
                                        uuid = attrs["uuid"].toString();
                                        qDebug()
                                            << "Found 3D model UUID in packageDetail.dataStr.shape (SVGNODE):" << uuid;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (uuid.isEmpty()) {
        QString msg = "No 3D model UUID found in CAD data";
        status->addDebugLog(msg);
        qDebug() << msg;
        return false;
    }

    status->addDebugLog(QString("Fetching 3D model for UUID: %1").arg(uuid));
    qDebug() << "Fetching 3D model for UUID:" << uuid;


    QString objUrl = QString("https://modules.easyeda.com/3dmodel/%1").arg(uuid);
    status->addDebugLog(QString("Downloading 3D model from: %1").arg(objUrl));
    qDebug() << "Downloading 3D model from:" << objUrl;
    QByteArray objData = httpGet(objUrl, 10000, status);

    if (objData.isEmpty()) {
        QString msg = QString("ERROR: Failed to download 3D model from: %1").arg(objUrl);
        status->addDebugLog(msg);
        qWarning() << msg;
        return false;
    }

    status->addDebugLog(QString("Downloaded 3D model data size: %1 bytes").arg(objData.size()));


    QByteArray actualObjData = objData;


    if (objData.size() >= 2 && objData[0] == 0x50 && objData[1] == 0x4B) {
        status->addDebugLog("3D model data is ZIP compressed, decompressing...");


        actualObjData = decompressZip(objData);


        status->addDebugLog(QString("Decompressed 3D model data size: %1 bytes").arg(actualObjData.size()));
    }


    if (actualObjData.size() >= 2 && (unsigned char)actualObjData[0] == 0x1f &&


        (unsigned char)actualObjData[1] == 0x8b) {
        status->addDebugLog("3D model data is gzip compressed, decompressing...");


        actualObjData = decompressGzip(actualObjData);


        status->addDebugLog(QString("Decompressed 3D model data size: %1 bytes").arg(actualObjData.size()));
    }


    if (actualObjData.isEmpty()) {
        QString msg = "ERROR: Failed to decompress 3D model data";


        status->addDebugLog(msg);


        qWarning() << msg;


        return false;
    }


    status->model3DObjRaw = actualObjData;


    QString successMsg = QString("3D model (OBJ) data fetched successfully for: %1").arg(status->componentId);
    status->addDebugLog(successMsg);
    qDebug() << successMsg;


    QString stepUrl = QString("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(uuid);
    status->addDebugLog(QString("Downloading STEP model from: %1").arg(stepUrl));
    QByteArray stepData = httpGet(stepUrl, 10000, status);

    if (!stepData.isEmpty() && stepData.size() > 100) {
        status->model3DStepRaw = stepData;
        QString stepMsg = QString("3D model (STEP) data fetched successfully for: %1, Size: %2 bytes")
                              .arg(status->componentId)
                              .arg(stepData.size());
        status->addDebugLog(stepMsg);
        qDebug() << stepMsg;
    } else {
        status->addDebugLog(
            QString("WARNING: Failed to download STEP model or STEP data too small for: %1").arg(status->componentId));
    }

    return true;
}

}  // namespace EasyKiConverter
