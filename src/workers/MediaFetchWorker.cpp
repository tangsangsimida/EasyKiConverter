#include "MediaFetchWorker.h"

#include "services/CacheRepository.h"
#include "services/ComponentCacheService.h"
#include "services/ConfigService.h"

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
    , m_isAborted(0) {}

MediaFetchWorker::~MediaFetchWorker() = default;

void MediaFetchWorker::run() {
    if (m_isAborted.loadAcquire()) {
        return;
    }

    m_previewImageDataList.clear();
    m_datasheetData.clear();
    m_diagnostics.clear();
    m_pendingOperations = 0;

    ComponentCacheService* cache = ComponentCacheService::instance();
    if (!cache) {
        qWarning() << "MediaFetchWorker: ComponentCacheService is null";
        emit fetchCompleted(m_componentId, m_previewImageDataList, m_datasheetData, m_diagnostics);
        return;
    }

    // 如果预览图为空，直接开始下载手册
    if (m_previewImageUrls.isEmpty()) {
        startDatasheetDownload(cache);
        return;
    }

    // 开始第一个预览图下载
    startPreviewDownload(0, cache);
}

void MediaFetchWorker::startPreviewDownload(int index, ComponentCacheService* cache) {
    if (m_isAborted.loadAcquire()) {
        finishAllDownloads();
        return;
    }

    if (index >= m_previewImageUrls.size()) {
        // 所有预览图下载完成，开始下载手册
        startDatasheetDownload(cache);
        return;
    }

    m_pendingOperations++;
    const QString& url = m_previewImageUrls[index];

    if (url.isEmpty()) {
        // URL为空，尝试直接从缓存加载
        QElapsedTimer timer;
        timer.start();
        QByteArray imageData = cache->loadPreviewImage(m_componentId, index);

        ComponentExportStatus::NetworkDiagnostics diag;
        diag.url = QString();
        diag.latencyMs = timer.elapsed();
        if (!imageData.isEmpty()) {
            diag.statusCode = 200;
            diag.errorString = "";
            diag.wasRateLimited = false;
            m_previewImageDataList.append(imageData);
        } else {
            diag.statusCode = 0;
            diag.errorString = "Not found in cache";
            diag.wasRateLimited = false;
        }
        diag.retryCount = 0;
        m_diagnostics.append(diag);

        m_pendingOperations--;
        // 继续下一个预览图
        startPreviewDownload(index + 1, cache);
        return;
    }

    // 使用异步下载
    CacheRepository::instance()->fetchPreviewImageAsync(
        m_componentId,
        url,
        index,
        &m_isAborted,
        ConfigService::instance()->getWeakNetworkSupport(),
        [this, index, cache](const QByteArray& imageData, const ComponentExportStatus::NetworkDiagnostics& diag) {
            if (m_isAborted.loadRelaxed()) {
                m_pendingOperations--;
                if (m_pendingOperations <= 0) {
                    finishAllDownloads();
                }
                return;
            }

            if (!imageData.isEmpty()) {
                m_previewImageDataList.append(imageData);
            }
            // 创建诊断信息的副本
            ComponentExportStatus::NetworkDiagnostics diagCopy = diag;
            diagCopy.latencyMs = diag.latencyMs;
            m_diagnostics.append(diagCopy);

            m_pendingOperations--;
            // 继续下一个预览图
            startPreviewDownload(index + 1, cache);
        });
}

void MediaFetchWorker::startDatasheetDownload(ComponentCacheService* cache) {
    if (m_isAborted.loadAcquire()) {
        finishAllDownloads();
        return;
    }

    if (m_datasheetUrl.isEmpty()) {
        // URL为空，尝试直接从缓存加载
        QElapsedTimer timer;
        timer.start();
        m_datasheetData = cache->loadDatasheet(m_componentId);

        ComponentExportStatus::NetworkDiagnostics diag;
        diag.url = QString();
        diag.latencyMs = timer.elapsed();
        if (!m_datasheetData.isEmpty()) {
            diag.statusCode = 200;
            diag.errorString = "";
            diag.wasRateLimited = false;
        } else {
            diag.statusCode = 0;
            diag.errorString = "Not found in cache";
            diag.wasRateLimited = false;
        }
        diag.retryCount = 0;
        m_diagnostics.append(diag);

        finishAllDownloads();
        return;
    }

    // 使用异步下载
    m_pendingOperations++;
    CacheRepository::instance()->fetchDatasheetAsync(
        m_componentId,
        m_datasheetUrl,
        &m_isAborted,
        ConfigService::instance()->getWeakNetworkSupport(),
        [this](const QByteArray& data, const ComponentExportStatus::NetworkDiagnostics& diag) {
            if (m_isAborted.loadRelaxed()) {
                m_pendingOperations--;
                if (m_pendingOperations <= 0) {
                    finishAllDownloads();
                }
                return;
            }

            m_datasheetData = data;
            ComponentExportStatus::NetworkDiagnostics diagCopy = diag;
            diagCopy.latencyMs = diag.latencyMs;
            m_diagnostics.append(diagCopy);

            m_pendingOperations--;
            finishAllDownloads();
        });
}

void MediaFetchWorker::finishAllDownloads() {
    // 检查是否所有操作都已完成
    if (m_pendingOperations > 0) {
        return;
    }

    // 发送完成信号（包含诊断信息）
    emit fetchCompleted(m_componentId, m_previewImageDataList, m_datasheetData, m_diagnostics);
}

void MediaFetchWorker::abort() {
    // 使用原子操作确保线程安全
    m_isAborted.storeRelease(1);
}

}  // namespace EasyKiConverter
