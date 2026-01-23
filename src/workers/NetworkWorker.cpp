#include "NetworkWorker.h"
#include "core/easyeda/EasyedaApi.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QEventLoop>
#include <QDebug>
#include <zlib.h>

namespace EasyKiConverter
{

    NetworkWorker::NetworkWorker(
        const QString &componentId,
        TaskType taskType,
        const QString &uuid,
        QObject *parent)
        : QObject(parent), QRunnable(), m_componentId(componentId), m_taskType(taskType), m_uuid(uuid)
    {
        setAutoDelete(true); // 任务完成后自动删�?
    }

    NetworkWorker::~NetworkWorker()
    {
        qDebug() << "NetworkWorker destroyed for:" << m_componentId;
    }

    void NetworkWorker::run()
    {
        qDebug() << "NetworkWorker started for:" << m_componentId << "- TaskType:" << static_cast<int>(m_taskType);

        try
        {
            bool success = false;

            switch (m_taskType)
            {
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

            if (!success)
            {
                qWarning() << "Network request failed for:" << m_componentId;
                // 错误信号已经在各个fetch方法中发�?
            }
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in NetworkWorker for" << m_componentId << ":" << e.what();
            emit fetchError(m_componentId, QString("Exception: %1").arg(e.what()));
        }
        catch (...)
        {
            qWarning() << "Unknown exception in NetworkWorker for:" << m_componentId;
            emit fetchError(m_componentId, "Unknown exception occurred");
        }
    }

    bool NetworkWorker::fetchComponentInfo()
    {
        try
        {
            // 创建网络访问管理�?
            QNetworkAccessManager manager;
            QEventLoop loop;

            // 构建请求URL
            QString url = QString("https://easyeda.com/api/components/%1").arg(m_componentId);
            QNetworkRequest request{QUrl(url)};
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setRawHeader("Accept", "application/json");

            // 发送请�?
            QNetworkReply *reply = manager.get(request);

            // 连接信号
            QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
                             {
            if (bytesTotal > 0) {
                int progress = static_cast<int>((static_cast<double>(bytesReceived) / bytesTotal) * 100);
                emit requestProgress(m_componentId, progress);
            } });

            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

            // 等待响应
            loop.exec();

            // 检查错�?
            if (reply->error() != QNetworkReply::NoError)
            {
                QString errorMessage = QString("Network error: %1").arg(reply->errorString());
                qWarning() << "Network error in fetchComponentInfo:" << errorMessage;
                emit fetchError(m_componentId, errorMessage);
                reply->deleteLater();
                return false;
            }

            // 读取响应数据
            QByteArray responseData = reply->readAll();
            reply->deleteLater();

            // 解析JSON
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (doc.isNull() || !doc.isObject())
            {
                QString errorMessage = "Invalid JSON response";
                qWarning() << "JSON parse error in fetchComponentInfo:" << errorMessage;
                emit fetchError(m_componentId, errorMessage);
                return false;
            }

            // 发送成功信�?
            QJsonObject data = doc.object();
            emit componentInfoFetched(m_componentId, data);

            qDebug() << "Component info fetched successfully for:" << m_componentId;
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in fetchComponentInfo:" << e.what();
            emit fetchError(m_componentId, QString("Exception: %1").arg(e.what()));
            return false;
        }
        catch (...)
        {
            qWarning() << "Unknown exception in fetchComponentInfo";
            emit fetchError(m_componentId, "Unknown exception occurred");
            return false;
        }
    }

    bool NetworkWorker::fetchCadData()
    {
        try
        {
            // 创建网络访问管理�?
            QNetworkAccessManager manager;
            QEventLoop loop;

            // 构建请求URL
            QString url = QString("https://easyeda.com/api/components/%1/cad").arg(m_componentId);
            QNetworkRequest request{QUrl(url)};
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setRawHeader("Accept", "application/json");

            // 发送请�?
            QNetworkReply *reply = manager.get(request);

            // 连接信号
            QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
                             {
            if (bytesTotal > 0) {
                int progress = static_cast<int>((static_cast<double>(bytesReceived) / bytesTotal) * 100);
                emit requestProgress(m_componentId, progress);
            } });

            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

            // 等待响应
            loop.exec();

            // 检查错�?
            if (reply->error() != QNetworkReply::NoError)
            {
                QString errorMessage = QString("Network error: %1").arg(reply->errorString());
                qWarning() << "Network error in fetchCadData:" << errorMessage;
                emit fetchError(m_componentId, errorMessage);
                reply->deleteLater();
                return false;
            }

            // 读取响应数据
            QByteArray responseData = reply->readAll();
            reply->deleteLater();

            // 解压gzip数据
            QByteArray decompressedData = decompressGzip(responseData);
            if (decompressedData.isEmpty())
            {
                QString errorMessage = "Failed to decompress CAD data";
                qWarning() << "Decompression error in fetchCadData:" << errorMessage;
                emit fetchError(m_componentId, errorMessage);
                return false;
            }

            // 解析JSON
            QJsonDocument doc = QJsonDocument::fromJson(decompressedData);
            if (doc.isNull() || !doc.isObject())
            {
                QString errorMessage = "Invalid JSON response";
                qWarning() << "JSON parse error in fetchCadData:" << errorMessage;
                emit fetchError(m_componentId, errorMessage);
                return false;
            }

            // 发送成功信�?
            QJsonObject data = doc.object();
            emit cadDataFetched(m_componentId, data);

            qDebug() << "CAD data fetched successfully for:" << m_componentId;
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in fetchCadData:" << e.what();
            emit fetchError(m_componentId, QString("Exception: %1").arg(e.what()));
            return false;
        }
        catch (...)
        {
            qWarning() << "Unknown exception in fetchCadData";
            emit fetchError(m_componentId, "Unknown exception occurred");
            return false;
        }
    }

    bool NetworkWorker::fetch3DModelObj()
    {
        try
        {
            // 创建网络访问管理�?
            QNetworkAccessManager manager;
            QEventLoop loop;

            // 构建请求URL
            QString url = QString("https://easyeda.com/api/models/%1/obj").arg(m_uuid);
            QNetworkRequest request{QUrl(url)};
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
            request.setRawHeader("Accept", "application/octet-stream");

            // 发送请�?
            QNetworkReply *reply = manager.get(request);

            // 连接信号
            QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
                             {
            if (bytesTotal > 0) {
                int progress = static_cast<int>((static_cast<double>(bytesReceived) / bytesTotal) * 100);
                emit requestProgress(m_componentId, progress);
            } });

            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

            // 等待响应
            loop.exec();

            // 检查错�?
            if (reply->error() != QNetworkReply::NoError)
            {
                QString errorMessage = QString("Network error: %1").arg(reply->errorString());
                qWarning() << "Network error in fetch3DModelObj:" << errorMessage;
                emit fetchError(m_componentId, errorMessage);
                reply->deleteLater();
                return false;
            }

            // 读取响应数据
            QByteArray responseData = reply->readAll();
            reply->deleteLater();

            // 发送成功信�?
            emit model3DFetched(m_componentId, m_uuid, responseData);

            qDebug() << "3D model OBJ data fetched successfully for:" << m_componentId;
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in fetch3DModelObj:" << e.what();
            emit fetchError(m_componentId, QString("Exception: %1").arg(e.what()));
            return false;
        }
        catch (...)
        {
            qWarning() << "Unknown exception in fetch3DModelObj";
            emit fetchError(m_componentId, "Unknown exception occurred");
            return false;
        }
    }

    bool NetworkWorker::fetch3DModelMtl()
    {
        try
        {
            // 创建网络访问管理�?
            QNetworkAccessManager manager;
            QEventLoop loop;

            // 构建请求URL
            QString url = QString("https://easyeda.com/api/models/%1/mtl").arg(m_uuid);
            QNetworkRequest request{QUrl(url)};
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
            request.setRawHeader("Accept", "application/octet-stream");

            // 发送请�?
            QNetworkReply *reply = manager.get(request);

            // 连接信号
            QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
                             {
            if (bytesTotal > 0) {
                int progress = static_cast<int>((static_cast<double>(bytesReceived) / bytesTotal) * 100);
                emit requestProgress(m_componentId, progress);
            } });

            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

            // 等待响应
            loop.exec();

            // 检查错�?
            if (reply->error() != QNetworkReply::NoError)
            {
                QString errorMessage = QString("Network error: %1").arg(reply->errorString());
                qWarning() << "Network error in fetch3DModelMtl:" << errorMessage;
                emit fetchError(m_componentId, errorMessage);
                reply->deleteLater();
                return false;
            }

            // 读取响应数据
            QByteArray responseData = reply->readAll();
            reply->deleteLater();

            // 发送成功信�?
            emit model3DFetched(m_componentId, m_uuid, responseData);

            qDebug() << "3D model MTL data fetched successfully for:" << m_componentId;
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in fetch3DModelMtl:" << e.what();
            emit fetchError(m_componentId, QString("Exception: %1").arg(e.what()));
            return false;
        }
        catch (...)
        {
            qWarning() << "Unknown exception in fetch3DModelMtl";
            emit fetchError(m_componentId, "Unknown exception occurred");
            return false;
        }
    }

    QByteArray NetworkWorker::decompressGzip(const QByteArray &compressedData)
    {
        if (compressedData.isEmpty())
        {
            return QByteArray();
        }

        // 检查是否为gzip数据（gzip魔术数字�?x1f 0x8b�?
        if (compressedData.size() < 2 ||
            (static_cast<unsigned char>(compressedData[0]) != 0x1f ||
             static_cast<unsigned char>(compressedData[1]) != 0x8b))
        {
            // 不是gzip数据，直接返�?
            qDebug() << "Data is not gzip compressed, returning as-is";
            return compressedData;
        }

        // 初始化zlib解压�?
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = 0;
        stream.next_in = Z_NULL;

        // 使用16+MAX_WBITS来处理gzip格式
        int ret = inflateInit2(&stream, 16 + MAX_WBITS);
        if (ret != Z_OK)
        {
            qWarning() << "Failed to initialize decompression";
            return QByteArray();
        }

        // 设置输入数据
        stream.avail_in = compressedData.size();
        stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(compressedData.constData()));

        // 准备输出缓冲�?
        QByteArray decompressedData;
        const int chunkSize = 8192;
        char buffer[chunkSize];

        // 执行解压
        do
        {
            stream.avail_out = chunkSize;
            stream.next_out = reinterpret_cast<Bytef *>(buffer);

            ret = inflate(&stream, Z_NO_FLUSH);

            if (ret == Z_BUF_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            {
                qWarning() << "Decompression error:" << ret;
                inflateEnd(&stream);
                return QByteArray();
            }

            // 添加解压后的数据
            int have = chunkSize - stream.avail_out;
            if (have > 0)
            {
                decompressedData.append(buffer, have);
            }

        } while (ret != Z_STREAM_END);

        // 清理
        inflateEnd(&stream);

        qDebug() << "Decompressed" << compressedData.size() << "bytes to" << decompressedData.size() << "bytes";
        return decompressedData;
    }

} // namespace EasyKiConverter
