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

    ComponentCacheService* cache = ComponentCacheService::instance();

    // 下载预览图（优先从缓存加载，缓存没有则下载并缓存）
    for (int i = 0; i < m_previewImageUrls.size(); ++i) {
        if (m_isAborted) {
            break;
        }
        const QString& url = m_previewImageUrls[i];
        if (url.isEmpty()) {
            continue;
        }

        // 使用 downloadPreviewImage，它会检查缓存，缓存没有则下载并自动保存
        QByteArray imageData = cache->downloadPreviewImage(m_componentId, url, i);
        if (!imageData.isEmpty()) {
            previewImageDataList.append(imageData);
        }
    }

    // 下载手册（优先从缓存加载，缓存没有则下载并缓存）
    if (!m_isAborted && !m_datasheetUrl.isEmpty()) {
        // 使用 downloadDatasheet，它会检查缓存，缓存没有则下载并自动保存
        datasheetData = cache->downloadDatasheet(m_componentId, m_datasheetUrl, nullptr);
    }

    // 发送完成信号
    emit fetchCompleted(m_componentId, previewImageDataList, datasheetData);
}

void MediaFetchWorker::abort() {
    m_isAborted = true;
    // 注意：ComponentCacheService 的下载方法是同步的，
    // abort 标志会在下次 run() 开始时检查
}

}  // namespace EasyKiConverter
