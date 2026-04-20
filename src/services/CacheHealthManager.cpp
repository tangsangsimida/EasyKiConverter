#include "CacheHealthManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>

namespace EasyKiConverter {

namespace {
QString _componentCacheDir(const QString& cacheRoot, const QString& lcscId) {
    return cacheRoot + "/" + lcscId;
}

QString _metadataPath(const QString& cacheRoot, const QString& lcscId) {
    return _componentCacheDir(cacheRoot, lcscId) + "/component.json";
}

QString _datasheetPath(const QString& cacheRoot, const QString& lcscId) {
    return _componentCacheDir(cacheRoot, lcscId) + "/datasheet";
}

QString _previewImagePath(const QString& cacheRoot, const QString& lcscId, int index) {
    return _componentCacheDir(cacheRoot, lcscId) + "/preview_" + QString::number(index) + ".jpg";
}

QString _resolveDatasheetPath(const QString& cacheRoot,
                              const QString& lcscId,
                              const QString& format,
                              bool /*migrate*/) {
    QString ext = format.isEmpty() ? "pdf" : format;
    return _componentCacheDir(cacheRoot, lcscId) + "/datasheet." + ext;
}

QJsonObject _loadMetadataFromPath(const QString& metaPath) {
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    QByteArray data = metaFile.readAll();
    metaFile.close();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

bool _saveMetadataToPath(const QString& metaPath, const QJsonObject& metadata) {
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    QByteArray data = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    qint64 written = metaFile.write(data);
    metaFile.close();
    return written > 0;
}
}  // namespace

CacheHealthManager::CacheHealthManager(const QString& cacheRoot) : m_cacheRoot(cacheRoot) {}

int CacheHealthManager::healAll() {
    QDir rootDir(m_cacheRoot);
    if (!rootDir.exists()) {
        return 0;
    }

    int repairedComponents = 0;
    int removedComponents = 0;

    const QStringList componentDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : componentDirs) {
        if (entry == "model3d") {
            continue;
        }
        if (repairComponentCache(entry)) {
            ++repairedComponents;
        } else {
            ++removedComponents;
        }
    }

    repairModel3DCache();

    if (repairedComponents > 0 || removedComponents > 0) {
        qInfo().noquote()
            << QString("Cache self-heal completed, retained %1 component cache dirs and removed %2 invalid dirs")
                   .arg(repairedComponents)
                   .arg(removedComponents);
    }

    return repairedComponents;
}

QString CacheHealthManager::componentCacheDir(const QString& lcscId) const {
    return _componentCacheDir(m_cacheRoot, lcscId);
}

QString CacheHealthManager::metadataPath(const QString& lcscId) const {
    return _metadataPath(m_cacheRoot, lcscId);
}

QString CacheHealthManager::datasheetPath(const QString& lcscId) const {
    return _datasheetPath(m_cacheRoot, lcscId);
}

QString CacheHealthManager::previewImagePath(const QString& lcscId, int index) const {
    return _previewImagePath(m_cacheRoot, lcscId, index);
}

QString CacheHealthManager::resolveDatasheetPath(const QString& lcscId, const QString& format, bool migrate) const {
    return _resolveDatasheetPath(m_cacheRoot, lcscId, format, migrate);
}

QJsonObject CacheHealthManager::loadMetadata(const QString& lcscId) const {
    return _loadMetadataFromPath(metadataPath(lcscId));
}

bool CacheHealthManager::saveMetadata(const QString& lcscId, const QJsonObject& metadata) {
    return _saveMetadataToPath(metadataPath(lcscId), metadata);
}

bool CacheHealthManager::repairComponentCache(const QString& lcscId) {
    const QString dirPath = componentCacheDir(lcscId);
    QDir componentDir(dirPath);
    if (!componentDir.exists()) {
        return false;
    }

    const QString metaPath = metadataPath(lcscId);
    if (!QFileInfo::exists(metaPath)) {
        componentDir.removeRecursively();
        qWarning().noquote() << "Removed invalid cache dir without metadata:" << dirPath;
        return false;
    }

    QJsonObject metadata = loadMetadata(lcscId);
    if (metadata.isEmpty()) {
        componentDir.removeRecursively();
        qWarning().noquote() << "Removed invalid cache dir with unreadable metadata:" << dirPath;
        return false;
    }

    bool metadataChanged = false;
    if (metadata.value("lcscId").toString().isEmpty()) {
        metadata["lcscId"] = lcscId;
        metadataChanged = true;
    }
    if (!metadata.contains("cachedAt")) {
        metadata["cachedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        metadataChanged = true;
    }

    const QString cadPath = componentDir.filePath("cad_data.json");
    bool hasValidCad = false;
    if (QFileInfo::exists(cadPath)) {
        QFile cadFile(cadPath);
        if (cadFile.open(QIODevice::ReadOnly)) {
            const QByteArray cadData = cadFile.readAll();
            cadFile.close();

            QJsonParseError parseError;
            const QJsonDocument cadDoc = QJsonDocument::fromJson(cadData, &parseError);
            hasValidCad = !cadData.isEmpty() && parseError.error == QJsonParseError::NoError && cadDoc.isObject();
        }

        if (!hasValidCad) {
            QFile::remove(cadPath);
            qWarning().noquote() << "Removed broken cad_data.json from cache:" << cadPath;
        }
    }

    for (int index = 0; index < 3; ++index) {
        const QString previewPath = previewImagePath(lcscId, index);
        const QFileInfo previewInfo(previewPath);
        if (previewInfo.exists() && previewInfo.size() <= 0) {
            QFile::remove(previewPath);
            qWarning().noquote() << "Removed empty preview cache file:" << previewPath;
        }
    }

    const QString legacyDatasheetPath = datasheetPath(lcscId);
    if (QFileInfo::exists(legacyDatasheetPath)) {
        QFile legacyDatasheet(legacyDatasheetPath);
        if (legacyDatasheet.open(QIODevice::ReadOnly)) {
            const QByteArray legacyData = legacyDatasheet.read(8);
            legacyDatasheet.close();

            const QString format = legacyData.startsWith("%PDF-") ? QStringLiteral("pdf") : QStringLiteral("html");
            const QString targetPath = resolveDatasheetPath(lcscId, format, true);
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            if (legacyDatasheet.rename(legacyDatasheetPath, targetPath)) {
                metadata["datasheetFormat"] = format;
                metadataChanged = true;
                qInfo().noquote() << "Migrated legacy datasheet cache file:" << targetPath;
            } else {
                QFile::remove(legacyDatasheetPath);
                qWarning().noquote() << "Removed unreadable legacy datasheet cache file:" << legacyDatasheetPath;
            }
        } else {
            QFile::remove(legacyDatasheetPath);
            qWarning().noquote() << "Removed unreadable legacy datasheet cache file:" << legacyDatasheetPath;
        }
    }

    const QString datasheetFormat = metadata.value("datasheetFormat").toString();
    const QString resolvedDatasheetPath = resolveDatasheetPath(lcscId, datasheetFormat, false);
    if (QFileInfo::exists(resolvedDatasheetPath)) {
        const QFileInfo datasheetInfo(resolvedDatasheetPath);
        if (datasheetInfo.size() <= 0) {
            QFile::remove(resolvedDatasheetPath);
            qWarning().noquote() << "Removed empty datasheet cache file:" << resolvedDatasheetPath;
        } else if (datasheetFormat.isEmpty()) {
            metadata["datasheetFormat"] =
                resolvedDatasheetPath.endsWith(".html") ? QStringLiteral("html") : QStringLiteral("pdf");
            metadataChanged = true;
        }
    }

    const bool hasBasicIdentity =
        !metadata.value("lcscId").toString().isEmpty() || !metadata.value("name").toString().isEmpty();
    const bool hasPreviewUrls = !metadata.value("previewImages").toArray().isEmpty();
    const bool hasDatasheetUrl = !metadata.value("datasheet").toString().isEmpty();
    const bool hasModel3DUuid = !metadata.value("model3duuid").toString().isEmpty();

    if (!hasValidCad && !hasBasicIdentity && !hasPreviewUrls && !hasDatasheetUrl && !hasModel3DUuid) {
        componentDir.removeRecursively();
        qWarning().noquote() << "Removed unusable cache dir after self-heal:" << dirPath;
        return false;
    }

    if (metadataChanged) {
        saveMetadata(lcscId, metadata);
    }

    return true;
}

void CacheHealthManager::repairModel3DCache() {
    const QString modelCacheDir = m_cacheRoot + "/model3d";
    QDir dir(modelCacheDir);
    if (!dir.exists()) {
        return;
    }

    const QSet<QString> validSuffixes = {QStringLiteral("step"), QStringLiteral("wrl")};
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& fileInfo : files) {
        const QString suffix = fileInfo.suffix().toLower();
        const bool removable =
            fileInfo.size() <= 0 || !validSuffixes.contains(suffix) || fileInfo.fileName().startsWith(".tmp");
        if (removable) {
            QFile::remove(fileInfo.absoluteFilePath());
            qWarning().noquote() << "Removed broken 3D cache file:" << fileInfo.absoluteFilePath();
        }
    }
}

}  // namespace EasyKiConverter