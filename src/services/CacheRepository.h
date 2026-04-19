#ifndef CACHEREPOSITORY_H
#define CACHEREPOSITORY_H

#include "ComponentCacheService.h"

#include <QAtomicInt>
#include <QFile>
#include <QFileInfo>

#include <functional>

namespace EasyKiConverter {

class CacheRepository {
public:
    using MediaFetchCallback = std::function<void(const QByteArray&, const ComponentExportStatus::NetworkDiagnostics&)>;

    static CacheRepository* instance() {
        static CacheRepository repository;
        return &repository;
    }

    void fetchPreviewImageAsync(const QString& lcscId,
                                const QString& imageUrl,
                                int imageIndex,
                                QAtomicInt* cancelled,
                                bool weakNetwork,
                                const MediaFetchCallback& onComplete) {
        ComponentCacheService* cache = ComponentCacheService::instance();
        if (!cache) {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = imageUrl;
            diag.errorString = QStringLiteral("Cache service unavailable");
            onComplete(QByteArray(), diag);
            return;
        }

        if (cancelled && cancelled->loadRelaxed()) {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = imageUrl;
            diag.errorString = QStringLiteral("Cancelled before start");
            onComplete(QByteArray(), diag);
            return;
        }

        const QString previewFilePath = cache->previewImagePath(lcscId, imageIndex);
        if (QFileInfo::exists(previewFilePath)) {
            QFile file(previewFilePath);
            if (file.open(QIODevice::ReadOnly)) {
                ComponentExportStatus::NetworkDiagnostics diag;
                diag.url = imageUrl;
                diag.statusCode = 200;
                diag.retryCount = 0;
                diag.wasRateLimited = false;
                onComplete(file.readAll(), diag);
                return;
            }
        }

        const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::previewImage(), weakNetwork);
        AsyncNetworkRequest* request =
            NetworkClient::instance().getAsync(QUrl(imageUrl), ResourceType::PreviewImage, policy);

        QObject::connect(request, &AsyncNetworkRequest::finished, request, [=](const NetworkResult& result) {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = imageUrl;
            diag.statusCode = result.statusCode;
            diag.retryCount = result.retryCount;
            diag.wasRateLimited = result.diagnostic.wasRateLimited;
            diag.latencyMs = result.elapsedMs;

            if ((cancelled && cancelled->loadRelaxed()) || result.wasCancelled) {
                diag.errorString = QStringLiteral("Cancelled");
                onComplete(QByteArray(), diag);
            } else if (result.success) {
                cache->savePreviewImage(lcscId, result.data, imageIndex);
                onComplete(result.data, diag);
            } else {
                diag.errorString = result.error;
                onComplete(QByteArray(), diag);
            }

            request->deleteLater();
        });
    }

    void fetchDatasheetAsync(const QString& lcscId,
                             const QString& datasheetUrl,
                             QAtomicInt* cancelled,
                             bool weakNetwork,
                             const MediaFetchCallback& onComplete) {
        ComponentCacheService* cache = ComponentCacheService::instance();
        if (!cache) {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = datasheetUrl;
            diag.errorString = QStringLiteral("Cache service unavailable");
            onComplete(QByteArray(), diag);
            return;
        }

        if (cancelled && cancelled->loadRelaxed()) {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = datasheetUrl;
            diag.errorString = QStringLiteral("Cancelled before start");
            onComplete(QByteArray(), diag);
            return;
        }

        QString ext =
            datasheetUrl.toLower().contains(QStringLiteral(".html")) ? QStringLiteral("html") : QStringLiteral("pdf");
        const QByteArray cachedDatasheet = cache->loadDatasheet(lcscId);
        if (!cachedDatasheet.isEmpty()) {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = datasheetUrl;
            diag.statusCode = 200;
            diag.retryCount = 0;
            diag.wasRateLimited = false;
            onComplete(cachedDatasheet, diag);
            return;
        }

        const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::datasheet(), weakNetwork);
        AsyncNetworkRequest* request =
            NetworkClient::instance().getAsync(QUrl(datasheetUrl), ResourceType::Datasheet, policy);

        QObject::connect(request, &AsyncNetworkRequest::finished, request, [=](const NetworkResult& result) mutable {
            ComponentExportStatus::NetworkDiagnostics diag;
            diag.url = datasheetUrl;
            diag.statusCode = result.statusCode;
            diag.retryCount = result.retryCount;
            diag.wasRateLimited = result.diagnostic.wasRateLimited;
            diag.latencyMs = result.elapsedMs;

            if ((cancelled && cancelled->loadRelaxed()) || result.wasCancelled) {
                diag.errorString = QStringLiteral("Cancelled");
                onComplete(QByteArray(), diag);
            } else if (result.success) {
                if (ext == QStringLiteral("pdf") && result.data.size() >= 5 && !result.data.startsWith("%PDF-")) {
                    ext = QStringLiteral("html");
                }
                cache->saveDatasheet(lcscId, result.data, ext);
                onComplete(result.data, diag);
            } else {
                diag.errorString = result.error;
                onComplete(QByteArray(), diag);
            }

            request->deleteLater();
        });
    }

private:
    CacheRepository() = default;
};

}  // namespace EasyKiConverter

#endif  // CACHEREPOSITORY_H
