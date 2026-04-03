#include "MediaFetchWorker.h"

#include <QDebug>
#include <QEventLoop>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

namespace EasyKiConverter {

MediaFetchWorker::MediaFetchWorker(const QString& componentId,
                                   const QStringList& previewImageUrls,
                                   const QString& datasheetUrl,
                                   QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_previewImageUrls(previewImageUrls)
    , m_datasheetUrl(datasheetUrl)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_isAborted(false) {}

MediaFetchWorker::~MediaFetchWorker() = default;

void MediaFetchWorker::run() {
    if (m_isAborted) {
        return;
    }

    QList<QByteArray> previewImageDataList;
    QByteArray datasheetData;

    // 下载预览图
    for (const QString& url : m_previewImageUrls) {
        if (m_isAborted) {
            break;
        }
        QByteArray imageData = httpGet(url, PREVIEW_IMAGE_TIMEOUT_MS);
        if (!imageData.isEmpty()) {
            previewImageDataList.append(imageData);
        }
    }

    // 下载手册
    if (!m_isAborted && !m_datasheetUrl.isEmpty()) {
        datasheetData = httpGet(m_datasheetUrl, DATASHEET_TIMEOUT_MS);
    }

    // 发送完成信号
    emit fetchCompleted(m_componentId, previewImageDataList, datasheetData);
}

void MediaFetchWorker::abort() {
    m_isAborted = true;
    QMutexLocker locker(&m_replyMutex);
    if (m_currentReply) {
        m_currentReply->abort();
    }
}

QByteArray MediaFetchWorker::httpGet(const QString& url, int timeoutMs) {
    if (url.isEmpty() || m_isAborted) {
        return QByteArray();
    }

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/145.0.0.0 Safari/537.36");
    request.setTransferTimeout(timeoutMs);

    QNetworkReply* reply = m_networkManager->get(request);
    {
        QMutexLocker locker(&m_replyMutex);
        m_currentReply = reply;
    }

    // 等待响应
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // 设置超时
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeoutMs);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();

    loop.exec();

    timer.stop();

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError && !m_isAborted) {
        data = reply->readAll();
    } else {
        qDebug() << "MediaFetchWorker: HTTP GET failed for" << url << "error:" << reply->errorString();
    }

    {
        QMutexLocker locker(&m_replyMutex);
        m_currentReply = nullptr;
    }
    reply->deleteLater();

    return data;
}

}  // namespace EasyKiConverter
