#include "AtomicFileWriter.h"

#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QThread>

namespace EasyKiConverter {

QMutex AtomicFileWriter::s_fileWriteMutex;

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

    {
        QMutexLocker locker(&s_fileWriteMutex);

        if (QFile::exists(finalPath)) {
            if (!QFile::remove(finalPath)) {
                qWarning() << "AtomicFileWriter: Failed to remove old file:" << finalPath;
            }
        }

        if (!QFile::rename(tempPath, finalPath)) {
            qWarning() << "AtomicFileWriter: Failed to rename:" << tempPath << "to" << finalPath;
            QFile::remove(tempPath);
            return false;
        }
    }

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

}  // namespace EasyKiConverter
