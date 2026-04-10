#include "AtomicFileWriter.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QLockFile>
#include <QThread>

namespace EasyKiConverter {

QString AtomicFileWriter::generateTempPath(const QString& tempDir, const QString& prefix, const QString& suffix) {
    qint64 threadId = reinterpret_cast<qint64>(QThread::currentThreadId());
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    return QString("%1/%2_%3_%4%5").arg(tempDir, prefix).arg(threadId).arg(timestamp, 0, 16).arg(suffix);
}

bool AtomicFileWriter::writeAtomically(const QString& tempDir,
                                       const QString& finalPath,
                                       const QString& suffix,
                                       WriteFunc writeFunc) {
    if (tempDir.isEmpty() || finalPath.isEmpty()) {
        qWarning() << "AtomicFileWriter: tempDir or finalPath is empty";
        return false;
    }

    if (!createDirectory(tempDir)) {
        qWarning() << "AtomicFileWriter: Failed to create temp directory:" << tempDir;
        return false;
    }

    QString tempPath = generateTempPath(tempDir, "file", suffix);

    if (!writeFunc(tempPath)) {
        qWarning() << "AtomicFileWriter: writeFunc failed for:" << tempPath;
        QFile::remove(tempPath);
        return false;
    }

    if (!QFile::exists(tempPath)) {
        qWarning() << "AtomicFileWriter: Temp file not found after write:" << tempPath;
        return false;
    }

    // 使用QLockFile实现per-file锁，避免全局串行化
    // 锁文件放在目标目录，文件名基于目标文件名的hash
    QString lockFileName = QString(".%1.lock").arg(qHash(finalPath), 0, 16);
    QString lockFilePath = QFileInfo(finalPath).absoluteDir().absoluteFilePath(lockFileName);
    QLockFile lockFile(lockFilePath);
    lockFile.setStaleLockTime(0);  // 禁用陈旧锁检测，避免异常退出后无法获取锁

    if (!lockFile.lock()) {
        qWarning() << "AtomicFileWriter: Failed to acquire lock for:" << finalPath;
        QFile::remove(tempPath);
        return false;
    }

    if (QFile::exists(finalPath)) {
        if (!QFile::remove(finalPath)) {
            qWarning() << "AtomicFileWriter: Failed to remove old file:" << finalPath;
        }
    }

    if (!QFile::rename(tempPath, finalPath)) {
        qWarning() << "AtomicFileWriter: Failed to rename:" << tempPath << "to" << finalPath;
        QFile::remove(tempPath);
        lockFile.unlock();
        return false;
    }

    lockFile.unlock();

    if (QFile::exists(finalPath)) {
        qDebug() << "AtomicFileWriter: File written atomically:" << finalPath;
        return true;
    }

    qWarning() << "AtomicFileWriter: Final file not found after rename:" << finalPath;
    return false;
}

bool AtomicFileWriter::writeAtomically(const QString& tempDir,
                                       const QString& finalPath,
                                       const QString& suffix,
                                       const QByteArray& data) {
    return writeAtomically(tempDir, finalPath, suffix, [&data](const QString& tempPath) -> bool {
        QFile file(tempPath);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        qint64 written = file.write(data);
        file.close();
        return written == data.size();
    });
}

bool AtomicFileWriter::createDirectory(const QString& path) {
    QDir dir;
    if (dir.exists(path)) {
        return true;
    }
    return dir.mkpath(path);
}

bool AtomicFileWriter::copyAtomically(const QString& sourcePath, const QString& finalPath, const QString& tempDir) {
    if (sourcePath.isEmpty() || finalPath.isEmpty() || tempDir.isEmpty()) {
        qWarning() << "AtomicFileWriter::copyAtomically: Invalid parameters";
        return false;
    }

    if (!QFileInfo::exists(sourcePath)) {
        qWarning() << "AtomicFileWriter::copyAtomically: Source file does not exist:" << sourcePath;
        return false;
    }

    if (!createDirectory(tempDir)) {
        qWarning() << "AtomicFileWriter::copyAtomically: Failed to create temp directory:" << tempDir;
        return false;
    }

    QString suffix = QFileInfo(finalPath).completeSuffix();
    if (!suffix.isEmpty()) {
        suffix = "." + suffix;
    }
    QString tempPath = generateTempPath(tempDir, "copy", suffix);

    // 拷贝到临时文件
    if (!QFile::copy(sourcePath, tempPath)) {
        qWarning() << "AtomicFileWriter::copyAtomically: Failed to copy" << sourcePath << "to" << tempPath;
        QFile::remove(tempPath);
        return false;
    }

    // 原子替换目标文件（使用per-file锁）
    // 使用QLockFile实现per-file锁，避免全局串行化
    QString lockFileName = QString(".%1.lock").arg(qHash(finalPath), 0, 16);
    QString lockFilePath = QFileInfo(finalPath).absoluteDir().absoluteFilePath(lockFileName);
    QLockFile lockFile(lockFilePath);
    lockFile.setStaleLockTime(0);  // 禁用陈旧锁检测

    if (!lockFile.lock()) {
        qWarning() << "AtomicFileWriter::copyAtomically: Failed to acquire lock for:" << finalPath;
        QFile::remove(tempPath);
        return false;
    }

    if (QFile::exists(finalPath)) {
        if (!QFile::remove(finalPath)) {
            qWarning() << "AtomicFileWriter::copyAtomically: Failed to remove old file:" << finalPath;
            QFile::remove(tempPath);
            lockFile.unlock();
            return false;
        }
    }

    if (!QFile::rename(tempPath, finalPath)) {
        qWarning() << "AtomicFileWriter::copyAtomically: Failed to rename:" << tempPath << "to" << finalPath;
        QFile::remove(tempPath);
        lockFile.unlock();
        return false;
    }

    lockFile.unlock();

    if (QFile::exists(finalPath)) {
        qDebug() << "AtomicFileWriter: File copied atomically:" << sourcePath << "->" << finalPath;
        return true;
    }

    qWarning() << "AtomicFileWriter::copyAtomically: Final file not found after copy:" << finalPath;
    return false;
}

}  // namespace EasyKiConverter
