#include "CacheDirectoryMigrator.h"

#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

bool CacheDirectoryMigrator::migrate(const QString& oldCacheDir, const QString& newCacheDir) {
    if (oldCacheDir.isEmpty() || newCacheDir.isEmpty() || oldCacheDir == newCacheDir) {
        return true;
    }

    QDir source(oldCacheDir);
    if (!source.exists()) {
        return true;
    }

    QDir target;
    if (!target.exists(newCacheDir) && !target.mkpath(newCacheDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration target directory: {}", newCacheDir);
        return false;
    }

    const bool moved = moveDirectoryContents(oldCacheDir, newCacheDir);
    if (moved) {
        source.rmdir(oldCacheDir);
        LOG_DEBUG(LogModule::Core, "Migrated cache directory from {} to {}", oldCacheDir, newCacheDir);
    } else {
        LOG_WARN(LogModule::Core,
                 "Cache directory migration completed with skipped or failed entries: {} -> {}",
                 oldCacheDir,
                 newCacheDir);
    }
    return moved;
}

bool CacheDirectoryMigrator::moveDirectoryContents(const QString& sourceDir, const QString& targetDir) {
    QDir source(sourceDir);
    if (!source.exists()) {
        return true;
    }

    QDir target;
    if (!target.exists(targetDir) && !target.mkpath(targetDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration directory: {}", targetDir);
        return false;
    }

    bool allMoved = true;
    const QFileInfoList entries = source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo& entryInfo : entries) {
        const QString sourcePath = entryInfo.absoluteFilePath();
        const QString targetPath = QDir(targetDir).filePath(entryInfo.fileName());
        if (!moveCacheEntry(sourcePath, targetPath)) {
            allMoved = false;
        }
    }

    return allMoved;
}

bool CacheDirectoryMigrator::moveCacheEntry(const QString& sourcePath, const QString& targetPath) {
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        return true;
    }

    QFileInfo targetInfo(targetPath);
    if (targetInfo.exists()) {
        if (sourceInfo.isDir() && targetInfo.isDir()) {
            const bool moved = moveDirectoryContents(sourcePath, targetPath);
            QDir sourceDir(sourcePath);
            if (sourceDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                sourceDir.rmdir(sourcePath);
            }
            return moved;
        }

        LOG_WARN(LogModule::Core, "Skipping cache migration entry because target already exists: {}", targetPath);
        return false;
    }

    QDir targetParent(targetInfo.absolutePath());
    if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral("."))) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration parent directory: {}", targetInfo.absolutePath());
        return false;
    }

    if (sourceInfo.isDir()) {
        QDir dir;
        if (dir.rename(sourcePath, targetPath)) {
            return true;
        }

        if (!moveDirectoryContents(sourcePath, targetPath)) {
            return false;
        }

        QDir sourceDir(sourcePath);
        return sourceDir.removeRecursively();
    }

    if (QFile::rename(sourcePath, targetPath)) {
        return true;
    }

    if (QFile::copy(sourcePath, targetPath)) {
        return QFile::remove(sourcePath);
    }

    LOG_WARN(LogModule::Core, "Failed to migrate cache file: {} -> {}", sourcePath, targetPath);
    return false;
}

}  // namespace EasyKiConverter
