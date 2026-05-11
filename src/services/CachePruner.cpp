#include "CachePruner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPair>
#include <QString>

#include <algorithm>

namespace EasyKiConverter {

namespace {
qint64 _calculateDirSize(const QString& dirPath) {
    qint64 size = 0;
    QDir dir(dirPath);
    if (!dir.exists()) {
        return size;
    }

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        size += info.size();
    }

    for (const QString& subDir : dir.entryList(QDir::Dirs)) {
        if (subDir != "." && subDir != "..") {
            size += _calculateDirSize(dirPath + "/" + subDir);
        }
    }

    return size;
}
}  // namespace

CachePruner::CachePruner(const QString& cacheRoot) : m_cacheRoot(cacheRoot) {}

qint64 CachePruner::calculateDirSize(const QString& dirPath) const {
    return _calculateDirSize(dirPath);
}

qint64 CachePruner::currentCacheSize() const {
    qint64 totalSize = 0;
    QDir dir(m_cacheRoot);
    for (const QString& entry : dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (entry == "model3d") {
            continue;
        }
        const QString path = QDir::cleanPath(m_cacheRoot + "/" + entry);
        QFileInfo info(path);
        if (info.isDir()) {
            totalSize += _calculateDirSize(path);
        } else {
            totalSize += info.size();
        }
    }
    return totalSize;
}

qint64 CachePruner::pruneTo(qint64 targetSizeBytes) {
    struct CacheEntry {
        QString name;
        QDateTime lastModified;
        qint64 size;
    };

    // 第一阶段：收集所有缓存条目信息（排除 model3d）
    QList<CacheEntry> cacheList;
    qint64 currentSize = 0;

    {
        QDir dir(m_cacheRoot);
        for (const QString& entry : dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (entry == "model3d") {
                continue;
            }
            const QString path = QDir::cleanPath(m_cacheRoot + "/" + entry);
            QFileInfo info(path);
            qint64 entrySize = info.isDir() ? _calculateDirSize(path) : info.size();
            cacheList.append({entry, info.lastModified(), entrySize});
            currentSize += entrySize;
        }
    }

    if (currentSize <= targetSizeBytes) {
        return currentSize;
    }

    // 按访问时间排序（最老的在前）
    std::sort(cacheList.begin(), cacheList.end(), [](const CacheEntry& a, const CacheEntry& b) {
        return a.lastModified < b.lastModified;
    });

    // 第二阶段：执行删除操作（使用缓存的大小值，避免重复扫描）
    for (const auto& entry : cacheList) {
        if (currentSize <= targetSizeBytes) {
            break;
        }

        const QString path = QDir::cleanPath(m_cacheRoot + "/" + entry.name);
        QFileInfo info(path);

        if (info.isDir()) {
            QDir d(path);
            if (d.removeRecursively()) {
                currentSize -= entry.size;
            }
        } else if (QFile::remove(path)) {
            currentSize -= entry.size;
        }
    }

    return currentSize;
}

}  // namespace EasyKiConverter
