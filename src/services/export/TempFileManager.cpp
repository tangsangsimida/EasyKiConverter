#include "TempFileManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QThread>

namespace EasyKiConverter {

TempFileManager::TempFileManager(QObject* parent) : QObject(parent) {}

TempFileManager::~TempFileManager() {
    rollbackAll();
}

void TempFileManager::setOutputPath(const QString& outputPath) {
    QMutexLocker locker(&m_mutex);
    m_outputPath = outputPath;
}

QString TempFileManager::tempDirectory() const {
    if (m_outputPath.isEmpty()) {
        return QString();
    }
    return m_outputPath + QDir::separator() + m_tempDirName;
}

QString TempFileManager::createTempFilePath(const QString& componentId, const QString& suffix) {
    QMutexLocker locker(&m_mutex);

    if (!ensureTempDirectory()) {
        return QString();
    }

    // 生成临时文件名：componentId_uuid.suffix
    QString tempName = generateUniqueTempName(componentId, suffix);
    QString tempPath = tempDirectory() + QDir::separator() + tempName;

    m_tempFiles.insert(tempPath);
    qDebug() << "TempFileManager: Created temp path:" << tempPath << "for component:" << componentId;

    return tempPath;
}

QString TempFileManager::tempFilePath(const QString& finalPath) const {
    QMutexLocker locker(&m_mutex);

    if (finalPath.isEmpty() || m_outputPath.isEmpty()) {
        return QString();
    }

    // 将最终路径转换为临时路径
    // 例如: /output/C12345.kicad_sym -> /output/.tmp/C12345_uuid.kicad_sym
    QString fileName = QFileInfo(finalPath).fileName();

    // 查找对应的临时文件
    for (const QString& tempPath : m_tempFiles) {
        if (tempPath.endsWith(fileName)) {
            return tempPath;
        }
    }

    // 如果没找到匹配的，返回基于最终路径名的临时路径
    return tempDirectory() + QDir::separator() + fileName;
}

QString TempFileManager::createSymbolTempPath(const QString& libName, const QString& suffix) {
    QMutexLocker locker(&m_mutex);

    if (!ensureTempDirectory()) {
        return QString();
    }

    // 符号库临时文件：libName.suffix
    QString tempName = libName + suffix;
    QString tempPath = tempDirectory() + QDir::separator() + tempName;

    m_tempFiles.insert(tempPath);
    qDebug() << "TempFileManager: Created symbol temp path:" << tempPath;

    return tempPath;
}

QString TempFileManager::createTempDirectoryPath(const QString& dirName) {
    QMutexLocker locker(&m_mutex);

    if (!ensureTempDirectory()) {
        return QString();
    }

    // 临时目录路径：.tmp/dirName
    QString tempPath = tempDirectory() + QDir::separator() + dirName;

    m_tempFiles.insert(tempPath);
    qDebug() << "TempFileManager: Created temp directory path:" << tempPath;

    return tempPath;
}

bool TempFileManager::commit(const QString& finalPath) {
    QMutexLocker locker(&m_mutex);

    if (finalPath.isEmpty()) {
        qWarning() << "TempFileManager: Cannot commit empty final path";
        return false;
    }

    // 查找对应的临时文件
    QString tempPath;
    for (const QString& t : m_tempFiles) {
        if (t.endsWith(QFileInfo(finalPath).fileName())) {
            tempPath = t;
            break;
        }
    }

    if (tempPath.isEmpty()) {
        qWarning() << "TempFileManager: No temp file found for:" << finalPath;
        return false;
    }

    if (!QFile::exists(tempPath)) {
        qWarning() << "TempFileManager: Temp file does not exist:" << tempPath;
        m_tempFiles.remove(tempPath);
        return false;
    }

    // 确保最终目录存在
    QFileInfo finalInfo(finalPath);
    QDir finalDir = finalInfo.absoluteDir();
    if (!finalDir.exists()) {
        if (!finalDir.mkpath(finalInfo.absolutePath())) {
            qWarning() << "TempFileManager: Failed to create directory:" << finalInfo.absolutePath();
            return false;
        }
    }

    // 删除已存在的最终文件（如果允许覆盖应该由调用方处理）
    if (QFile::exists(finalPath)) {
        if (!QFile::remove(finalPath)) {
            qWarning() << "TempFileManager: Failed to remove existing file:" << finalPath;
            return false;
        }
    }

    // 重命名临时文件为最终路径
    if (!QFile::rename(tempPath, finalPath)) {
        qWarning() << "TempFileManager: Failed to rename:" << tempPath << "to" << finalPath;
        return false;
    }

    m_tempFiles.remove(tempPath);
    qDebug() << "TempFileManager: Committed:" << tempPath << "->" << finalPath;

    locker.unlock();
    emit commitCompleted(finalPath, true);

    return true;
}

bool TempFileManager::commitDirectory(const QString& finalDirPath) {
    QMutexLocker locker(&m_mutex);

    if (finalDirPath.isEmpty()) {
        qWarning() << "TempFileManager: Cannot commit empty final directory path";
        return false;
    }

    // 查找对应的临时目录
    QString tempPath;
    QString dirName = QFileInfo(finalDirPath).fileName();
    for (const QString& t : m_tempFiles) {
        if (t.endsWith(dirName)) {
            tempPath = t;
            break;
        }
    }

    if (tempPath.isEmpty()) {
        qWarning() << "TempFileManager: No temp directory found for:" << finalDirPath;
        return false;
    }

    if (!QDir(tempPath).exists()) {
        qWarning() << "TempFileManager: Temp directory does not exist:" << tempPath;
        m_tempFiles.remove(tempPath);
        return false;
    }

    // 确保最终目录的父目录存在
    QFileInfo finalInfo(finalDirPath);
    QDir parentDir = finalInfo.absoluteDir();
    if (!parentDir.exists()) {
        if (!parentDir.mkpath(finalInfo.absolutePath())) {
            qWarning() << "TempFileManager: Failed to create parent directory:" << finalInfo.absolutePath();
            return false;
        }
    }

    // 删除已存在的最终目录（如果允许覆盖应该由调用方处理）
    if (QDir(finalDirPath).exists()) {
        if (!QDir(finalDirPath).removeRecursively()) {
            qWarning() << "TempFileManager: Failed to remove existing directory:" << finalDirPath;
            return false;
        }
    }

    // 重命名临时目录为最终路径
    if (!QDir().rename(tempPath, finalDirPath)) {
        qWarning() << "TempFileManager: Failed to rename:" << tempPath << "to" << finalDirPath;
        return false;
    }

    m_tempFiles.remove(tempPath);
    qDebug() << "TempFileManager: Committed directory:" << tempPath << "->" << finalDirPath;

    locker.unlock();
    emit commitCompleted(finalDirPath, true);

    return true;
}

void TempFileManager::rollbackAll() {
    QMutexLocker locker(&m_mutex);

    int deletedCount = 0;
    for (const QString& tempPath : m_tempFiles) {
        if (QFileInfo(tempPath).isDir()) {
            if (QDir(tempPath).removeRecursively()) {
                deletedCount++;
            } else {
                qWarning() << "TempFileManager: Failed to remove temp directory:" << tempPath;
            }
        } else if (deleteFile(tempPath)) {
            deletedCount++;
        }
    }

    m_tempFiles.clear();

    locker.unlock();
    emit cleanupCompleted(deletedCount);

    qDebug() << "TempFileManager: Rolled back" << deletedCount << "temp files";
}

void TempFileManager::cleanupTempDirectory() {
    QMutexLocker locker(&m_mutex);

    QString tempDir = tempDirectory();
    if (tempDir.isEmpty() || !QDir(tempDir).exists()) {
        return;
    }

    int deletedCount = 0;
    QDir dir(tempDir);
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        if (deleteFile(info.absoluteFilePath())) {
            deletedCount++;
        }
    }

    // 删除临时目录本身
    if (dir.isEmpty()) {
        QDir().rmdir(tempDir);
    }

    locker.unlock();
    emit cleanupCompleted(deletedCount);
}

void TempFileManager::cleanupOrphanedTempFiles() {
    QString tempDir = tempDirectory();
    if (tempDir.isEmpty() || !QDir(tempDir).exists()) {
        return;
    }

    int deletedCount = 0;
    QDir dir(tempDir);
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        if (info.fileName().startsWith('.')) {
            continue;  // 跳过隐藏文件
        }
        if (deleteFile(info.absoluteFilePath())) {
            deletedCount++;
        }
    }

    // 如果目录为空，删除目录
    if (dir.isEmpty()) {
        QDir().rmdir(tempDir);
    }

    qDebug() << "TempFileManager: Cleaned up" << deletedCount << "orphaned temp files";
}

void TempFileManager::registerTempFile(const QString& tempPath) {
    QMutexLocker locker(&m_mutex);
    m_tempFiles.insert(tempPath);
}

QSet<QString> TempFileManager::registeredTempFiles() const {
    QMutexLocker locker(&m_mutex);
    return m_tempFiles;
}

QString TempFileManager::generateUniqueTempName(const QString& prefix, const QString& suffix) const {
    // 生成唯一的临时文件名避免冲突
    // 格式: prefix_uuid_suffix
    quint64 random = QRandomGenerator::global()->generate64();
    QString uuid = QString::number(random, 16);
    return prefix + QStringLiteral("_") + uuid + suffix;
}

bool TempFileManager::ensureTempDirectory() const {
    QString tempDir = tempDirectory();
    if (tempDir.isEmpty()) {
        return false;
    }

    QDir dir(tempDir);
    if (dir.exists()) {
        return true;
    }

    return dir.mkpath(tempDir);
}

bool TempFileManager::deleteFile(const QString& path) const {
    if (path.isEmpty()) {
        return true;
    }

    QFile file(path);
    if (!file.exists()) {
        return true;  // 文件不存在视为删除成功
    }

    if (file.remove()) {
        qDebug() << "TempFileManager: Deleted:" << path;
        return true;
    }

    qWarning() << "TempFileManager: Failed to delete:" << path;
    return false;
}

}  // namespace EasyKiConverter
