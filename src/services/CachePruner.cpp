#include "CachePruner.h"

#include <QDir>
#include <QFileInfo>
#include <QPair>
#include <QString>

#include <algorithm>

namespace EasyKiConverter {

namespace {
QString _componentCacheDir(const QString& cacheRoot, const QString& lcscId) {
    return cacheRoot + "/" + lcscId;
}

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
    for (const QString& subDir : dir.entryList(QDir::Dirs)) {
        if (subDir == "." || subDir == ".." || subDir == "model3d") {
            continue;
        }
        QString dirPath = QDir::cleanPath(m_cacheRoot + "/" + subDir);
        QFileInfo info(dirPath);
        if (info.exists()) {
            totalSize += _calculateDirSize(dirPath);
        }
    }
    return totalSize;
}

qint64 CachePruner::pruneTo(qint64 targetSizeBytes) {
    // 第一阶段：锁外收集所有缓存目录的信息
    QList<QPair<QString, QDateTime>> cacheList;
    qint64 currentSize = 0;

    {
        QDir dir(m_cacheRoot);
        for (const QString& subDir : dir.entryList(QDir::Dirs)) {
            if (subDir == "." || subDir == ".." || subDir == "model3d") {
                continue;
            }
            QString dirPath = QDir::cleanPath(m_cacheRoot + "/" + subDir);
            QFileInfo info(dirPath);
            if (info.exists()) {
                cacheList.append(qMakePair(subDir, info.lastModified()));
                currentSize += _calculateDirSize(dirPath);
            }
        }
    }

    if (currentSize <= targetSizeBytes) {
        return currentSize;
    }

    // 按访问时间排序（最老的在前）
    std::sort(
        cacheList.begin(), cacheList.end(), [](const QPair<QString, QDateTime>& a, const QPair<QString, QDateTime>& b) {
            return a.second < b.second;
        });

    // 第二阶段：执行删除操作
    for (const auto& pair : cacheList) {
        if (currentSize <= targetSizeBytes) {
            break;
        }

        QString dirPath = QDir::cleanPath(m_cacheRoot + "/" + pair.first);
        qint64 dirSize = _calculateDirSize(dirPath);

        QDir d;
        if (d.exists(dirPath)) {
            d.removeRecursively();
            currentSize -= dirSize;
        }
    }

    return currentSize;
}

}  // namespace EasyKiConverter