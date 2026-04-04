#include "MediaFetchWorker.h"

#include "services/ComponentCacheService.h"

#include <QDebug>

namespace EasyKiConverter {

MediaFetchWorker::MediaFetchWorker(const QString& componentId,
                                   const QStringList& previewImageUrls,
                                   const QString& datasheetUrl,
                                   QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_previewImageUrls(previewImageUrls)
    , m_datasheetUrl(datasheetUrl)
    , m_isAborted(false) {}

MediaFetchWorker::~MediaFetchWorker() = default;

void MediaFetchWorker::run() {
    if (m_isAborted) {
        return;
    }

    QList<QByteArray> previewImageDataList;
    QByteArray datasheetData;
    m_diagnostics.clear();

    ComponentCacheService* cache = ComponentCacheService::instance();
    if (!cache) {
        qWarning() << "MediaFetchWorker: ComponentCacheService is null";
        emit fetchCompleted(m_componentId, previewImageDataList, datasheetData, m_diagnostics);
        return;
    }

    // 下载预览图（优先从缓存加载，缓存没有则下载并缓存）
    for (int i = 0; i < m_previewImageUrls.size(); ++i) {
        if (m_isAborted) {
            break;
        }
        const QString& url = m_previewImageUrls[i];

        // 跟踪网络诊断信息
        ComponentExportStatus::NetworkDiagnostics diag;
        QByteArray imageData;

        if (url.isEmpty()) {
            // URL为空，尝试直接从缓存加载（不经过网络）
            QElapsedTimer timer;
            timer.start();
            imageData = cache->loadPreviewImage(m_componentId, i);
            diag.url = QString();
            diag.latencyMs = timer.elapsed();
            if (!imageData.isEmpty()) {
                diag.statusCode = 200;
                diag.errorString = "";
                diag.wasRateLimited = false;
            } else {
                diag.statusCode = 0;
                diag.errorString = "Not found in cache";
                diag.wasRateLimited = false;
            }
            diag.retryCount = 0;
        } else {
            // 使用 downloadPreviewImage，它会检查缓存，缓存没有则下载并自动保存
            imageData = cache->downloadPreviewImage(m_componentId, url, i, &diag);
        }

        if (!imageData.isEmpty()) {
            previewImageDataList.append(imageData);
        }
        m_diagnostics.append(diag);
    }

    // 下载手册（优先从缓存加载，缓存没有则下载并缓存）
    if (!m_isAborted) {
        // 跟踪网络诊断信息
        ComponentExportStatus::NetworkDiagnostics diag;
        QElapsedTimer timer;
        timer.start();

        if (m_datasheetUrl.isEmpty()) {
            // URL为空，尝试直接从缓存加载
            datasheetData = cache->loadDatasheet(m_componentId);
            diag.url = QString();
            diag.latencyMs = timer.elapsed();
            if (!datasheetData.isEmpty()) {
                diag.statusCode = 200;
                diag.errorString = "";
                diag.wasRateLimited = false;
            } else {
                diag.statusCode = 0;
                diag.errorString = "Not found in cache";
                diag.wasRateLimited = false;
            }
        } else {
            // 使用 downloadDatasheet，它会检查缓存，缓存没有则下载并自动保存
            datasheetData = cache->downloadDatasheet(m_componentId, m_datasheetUrl, nullptr, &diag);
            diag.url = m_datasheetUrl;
            diag.latencyMs = timer.elapsed();
            if (datasheetData.isEmpty()) {
                diag.statusCode = 0;
                if (diag.errorString.isEmpty()) {
                    diag.errorString = "Download failed";
                }
                diag.wasRateLimited = false;
            } else {
                diag.statusCode = 200;
                diag.errorString = "";
                diag.wasRateLimited = false;
            }
        }
        diag.retryCount = 0;
        m_diagnostics.append(diag);
    }

    // 发送完成信号（包含诊断信息）
    emit fetchCompleted(m_componentId, previewImageDataList, datasheetData, m_diagnostics);
}

void MediaFetchWorker::abort() {
    m_isAborted = true;
    // 注意：ComponentCacheService 的下载方法是同步的，
    // abort 标志会在下次 run() 开始时检查
}

}  // namespace EasyKiConverter
