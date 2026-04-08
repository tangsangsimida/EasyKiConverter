#include "ComponentDataCache.h"

#include "../../models/ComponentData.h"
#include "../../models/FootprintDataSerializer.h"
#include "../../models/SymbolDataSerializer.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

ComponentDataCache::ComponentDataCache(QObject* parent) : QObject(parent) {}

ComponentDataCache::~ComponentDataCache() {
    // 析构前同步到磁盘
    flushToDisk();
}

void ComponentDataCache::setCacheDir(const QString& path) {
    QMutexLocker locker(&m_cacheMutex);
    m_cacheDir = path;

    // 创建子目录结构
    QDir dir(path);
    QStringList subDirs = {QStringLiteral("symbols"),
                           QStringLiteral("footprints"),
                           QStringLiteral("3dmodels"),
                           QStringLiteral("datasheets"),
                           QStringLiteral("previews")};

    for (const QString& subDir : subDirs) {
        dir.mkpath(subDir);
    }
}

bool ComponentDataCache::has(const QString& componentId) const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.contains(componentId);
}

QSharedPointer<ComponentData> ComponentDataCache::get(const QString& componentId) const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.value(componentId, nullptr);
}

void ComponentDataCache::put(const QString& componentId, const QSharedPointer<ComponentData>& data) {
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache[componentId] = data;
    }
    emit cacheUpdated(componentId);
}

void ComponentDataCache::remove(const QString& componentId) {
    QMutexLocker locker(&m_cacheMutex);
    m_cache.remove(componentId);
}

void ComponentDataCache::clear() {
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.clear();
    }
    emit cacheCleared();
}

QStringList ComponentDataCache::cachedComponentIds() const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.keys();
}

int ComponentDataCache::size() const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.size();
}

void ComponentDataCache::flushToDisk(const QString& componentId) {
    QMutexLocker locker(&m_cacheMutex);

    if (m_cacheDir.isEmpty()) {
        qWarning() << "ComponentDataCache: Cache dir not set";
        return;
    }

    QStringList idsToFlush;
    if (componentId.isEmpty()) {
        idsToFlush = m_cache.keys();
    } else if (m_cache.contains(componentId)) {
        idsToFlush.append(componentId);
    } else {
        return;
    }

    for (const QString& id : idsToFlush) {
        const auto& data = m_cache.value(id);
        if (!data)
            continue;

        // 构建元器件数据文件路径
        QString dataFilePath = m_cacheDir + QStringLiteral("/") + id + QStringLiteral(".json");

        QJsonObject json;
        json[QStringLiteral("componentId")] = id;

        // 序列化符号数据
        if (data->symbolData()) {
            QJsonObject symbolJson = SymbolDataSerializer::toJson(*data->symbolData());
            json[QStringLiteral("symbolData")] = symbolJson;
        }

        // 序列化封装数据
        if (data->footprintData()) {
            QJsonObject footprintJson = FootprintDataSerializer::toJson(*data->footprintData());
            json[QStringLiteral("footprintData")] = footprintJson;
        }

        // 写入JSON文件
        QFile file(dataFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(json).toJson());
            file.close();
        } else {
            qWarning() << "ComponentDataCache: Failed to write cache file" << dataFilePath;
        }
    }
}

bool ComponentDataCache::loadFromDisk(const QString& componentId) {
    if (m_cacheDir.isEmpty()) {
        return false;
    }

    QString dataFilePath = m_cacheDir + QStringLiteral("/") + componentId + QStringLiteral(".json");
    QFile file(dataFilePath);

    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "ComponentDataCache: Failed to parse cache file" << error.errorString();
        return false;
    }

    // 反序列化为ComponentData
    QJsonObject json = doc.object();
    auto data = QSharedPointer<ComponentData>::create();

    if (json.contains(QStringLiteral("symbolData"))) {
        QJsonObject symbolJson = json[QStringLiteral("symbolData")].toObject();
        SymbolData symbolData;
        if (SymbolDataSerializer::fromJson(symbolData, symbolJson)) {
            data->setSymbolData(QSharedPointer<SymbolData>::create(symbolData));
        }
    }

    if (json.contains(QStringLiteral("footprintData"))) {
        QJsonObject footprintJson = json[QStringLiteral("footprintData")].toObject();
        FootprintData footprintData;
        if (FootprintDataSerializer::fromJson(footprintData, footprintJson)) {
            data->setFootprintData(QSharedPointer<FootprintData>::create(footprintData));
        }
    }

    put(componentId, data);
    return true;
}

void ComponentDataCache::clearDiskCache(const QString& componentId) {
    if (m_cacheDir.isEmpty()) {
        return;
    }

    if (componentId.isEmpty()) {
        // 清除整个缓存目录
        QDir cacheDir(m_cacheDir);
        cacheDir.removeRecursively();
        setCacheDir(m_cacheDir);  // 重建目录结构
    } else {
        // 清除指定元器件的缓存文件
        QString dataFilePath = m_cacheDir + QStringLiteral("/") + componentId + QStringLiteral(".json");
        QFile::remove(dataFilePath);

        // 清除关联的文件
        QStringList subDirs = {QStringLiteral("symbols"),
                               QStringLiteral("footprints"),
                               QStringLiteral("3dmodels"),
                               QStringLiteral("datasheets"),
                               QStringLiteral("previews")};

        for (const QString& subDir : subDirs) {
            QString dirPath = m_cacheDir + QStringLiteral("/") + subDir;
            QDir subDirHandle(dirPath);
            QStringList filters = {componentId + QStringLiteral("*")};
            QStringList files = subDirHandle.entryList(filters);
            for (const QString& file : files) {
                subDirHandle.remove(file);
            }
        }
    }
}

}  // namespace EasyKiConverter
