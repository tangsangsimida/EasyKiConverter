#include "TempFileManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QThread>

#include <chrono>
#include <thread>

namespace EasyKiConverter {

namespace {

struct BackupEntry {
    QString finalPath;
    QString backupPath;
    QString tempPath;
    bool isDirectory = false;
    bool existedBefore = false;
};

constexpr int kDefaultBackupRetentionDays = 3;
constexpr int kDefaultMaxBackupSets = 3;

QString backupRootPath() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        appDataPath = QDir::homePath() + QDir::separator() + QStringLiteral(".easykiconverter");
    }
    return QDir(appDataPath).filePath(QStringLiteral("backups"));
}

QString createTransactionId() {
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmsszzzZ")) + QStringLiteral("_") +
           QString::number(QRandomGenerator::global()->generate64(), 16);
}

bool ensureParentDirectory(const QString& path) {
    const QFileInfo info(path);
    const QDir parentDir = info.absoluteDir();
    return parentDir.exists() || QDir().mkpath(info.absolutePath());
}

bool copyDirectoryRecursively(const QString& sourcePath, const QString& targetPath) {
    QDir sourceDir(sourcePath);
    if (!sourceDir.exists()) {
        return false;
    }

    if (!QDir().mkpath(targetPath)) {
        return false;
    }

    const QFileInfoList entries =
        sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString sourceEntryPath = entry.absoluteFilePath();
        const QString targetEntryPath = QDir(targetPath).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryRecursively(sourceEntryPath, targetEntryPath)) {
                return false;
            }
        } else {
            if (QFile::exists(targetEntryPath) && !QFile::remove(targetEntryPath)) {
                return false;
            }
            if (!QFile::copy(sourceEntryPath, targetEntryPath)) {
                return false;
            }
        }
    }

    return true;
}

bool moveFileWithFallback(const QString& sourcePath, const QString& targetPath) {
    if (!ensureParentDirectory(targetPath)) {
        return false;
    }

    constexpr int kMaxRetries = 3;
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * attempt));
        }
        if (QFile::rename(sourcePath, targetPath)) {
            return true;
        }
    }

    if (!QFile::copy(sourcePath, targetPath)) {
        return false;
    }

    return QFile::remove(sourcePath);
}

bool moveDirectoryWithFallback(const QString& sourcePath, const QString& targetPath) {
    if (!ensureParentDirectory(targetPath)) {
        return false;
    }

    if (QDir().rename(sourcePath, targetPath)) {
        return true;
    }

    if (!copyDirectoryRecursively(sourcePath, targetPath)) {
        return false;
    }

    return QDir(sourcePath).removeRecursively();
}

bool removePath(const QString& path, bool isDirectory) {
    if (path.isEmpty()) {
        return true;
    }
    if (isDirectory) {
        QDir dir(path);
        return !dir.exists() || dir.removeRecursively();
    }
    return !QFile::exists(path) || QFile::remove(path);
}

bool movePathWithFallback(const QString& sourcePath, const QString& targetPath, bool isDirectory) {
    return isDirectory ? moveDirectoryWithFallback(sourcePath, targetPath)
                       : moveFileWithFallback(sourcePath, targetPath);
}

bool pathExists(const QString& path, bool isDirectory) {
    return isDirectory ? QDir(path).exists() : QFile::exists(path);
}

bool writeManifest(const QString& manifestPath,
                   const QString& transactionId,
                   const QString& outputRoot,
                   const QVector<BackupEntry>& entries,
                   bool committed) {
    QJsonObject root;
    root.insert(QStringLiteral("transactionId"), transactionId);
    root.insert(QStringLiteral("outputRoot"), outputRoot);
    root.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
    root.insert(QStringLiteral("committed"), committed);

    QJsonArray entryArray;
    for (const BackupEntry& entry : entries) {
        QJsonObject entryObject;
        entryObject.insert(QStringLiteral("finalPath"), entry.finalPath);
        entryObject.insert(QStringLiteral("backupPath"), entry.backupPath);
        entryObject.insert(QStringLiteral("tempPath"), entry.tempPath);
        entryObject.insert(QStringLiteral("isDirectory"), entry.isDirectory);
        entryObject.insert(QStringLiteral("existedBefore"), entry.existedBefore);
        entryArray.append(entryObject);
    }
    root.insert(QStringLiteral("entries"), entryArray);

    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "TempFileManager: Failed to write manifest:" << manifestPath << manifestFile.errorString();
        return false;
    }
    return manifestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) > 0;
}

QVector<BackupEntry> readManifestEntries(const QString& manifestPath, bool* committed) {
    QVector<BackupEntry> entries;
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qWarning() << "TempFileManager: Failed to read manifest:" << manifestPath << manifestFile.errorString();
        return entries;
    }

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    const QJsonObject root = document.object();
    if (committed != nullptr) {
        *committed = root.value(QStringLiteral("committed")).toBool(false);
    }

    const QJsonArray entryArray = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& value : entryArray) {
        const QJsonObject object = value.toObject();
        BackupEntry entry;
        entry.finalPath = object.value(QStringLiteral("finalPath")).toString();
        entry.backupPath = object.value(QStringLiteral("backupPath")).toString();
        entry.tempPath = object.value(QStringLiteral("tempPath")).toString();
        entry.isDirectory = object.value(QStringLiteral("isDirectory")).toBool(false);
        entry.existedBefore = object.value(QStringLiteral("existedBefore")).toBool(false);
        entries.append(entry);
    }
    return entries;
}

}  // namespace

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

    QString fileName = QFileInfo(finalPath).fileName();

    for (const QString& tempPath : m_tempFiles) {
        if (tempPath.endsWith(fileName)) {
            return tempPath;
        }
    }

    return tempDirectory() + QDir::separator() + fileName;
}

QString TempFileManager::createSymbolTempPath(const QString& libName, const QString& suffix) {
    QMutexLocker locker(&m_mutex);

    if (!ensureTempDirectory()) {
        return QString();
    }

    QString tempName = libName + suffix;
    const QString tempDir = tempDirectory();
    QString tempPath = tempDir + QDir::separator() + tempName;

    m_tempFiles.insert(tempPath);
    qDebug() << "TempFileManager: Created symbol temp path:" << tempPath;

    return tempPath;
}

QString TempFileManager::createTempDirectoryPath(const QString& dirName) {
    QMutexLocker locker(&m_mutex);

    if (!ensureTempDirectory()) {
        return QString();
    }

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

    const bool committed = commitBatchLocked({CommitItem{tempPath, finalPath, false}});

    locker.unlock();
    emit commitCompleted(finalPath, committed);

    return committed;
}

bool TempFileManager::commitDirectory(const QString& finalDirPath) {
    QMutexLocker locker(&m_mutex);

    if (finalDirPath.isEmpty()) {
        qWarning() << "TempFileManager: Cannot commit empty final directory path";
        return false;
    }

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

    const bool committed = commitBatchLocked({CommitItem{tempPath, finalDirPath, true}});

    locker.unlock();
    emit commitCompleted(finalDirPath, committed);

    return committed;
}

bool TempFileManager::commitWithBackup(const QString& tempPath, const QString& finalPath) {
    QMutexLocker locker(&m_mutex);
    const bool committed = commitBatchLocked({CommitItem{tempPath, finalPath, false}});

    locker.unlock();
    emit commitCompleted(finalPath, committed);

    return committed;
}

bool TempFileManager::commitDirectoryWithBackup(const QString& tempDirPath, const QString& finalDirPath) {
    QMutexLocker locker(&m_mutex);
    const bool committed = commitBatchLocked({CommitItem{tempDirPath, finalDirPath, true}});

    locker.unlock();
    emit commitCompleted(finalDirPath, committed);

    return committed;
}

bool TempFileManager::commitBatch(const QVector<CommitItem>& items) {
    QMutexLocker locker(&m_mutex);
    const bool committed = commitBatchLocked(items);

    locker.unlock();
    for (const CommitItem& item : items) {
        emit commitCompleted(item.finalPath, committed);
    }

    return committed;
}

bool TempFileManager::recoverIncompleteTransactions() {
    QDir backupRoot(backupRootPath());
    if (!backupRoot.exists()) {
        return true;
    }

    bool recoveredAll = true;
    QList<QFileInfo> committedTransactionDirs;
    const QFileInfoList transactionDirs = backupRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo& transactionDirInfo : transactionDirs) {
        const QString manifestPath =
            QDir(transactionDirInfo.absoluteFilePath()).filePath(QStringLiteral("manifest.json"));
        if (!QFile::exists(manifestPath)) {
            continue;
        }

        bool committed = false;
        const QVector<BackupEntry> entries = readManifestEntries(manifestPath, &committed);
        if (committed) {
            committedTransactionDirs.append(transactionDirInfo);
            continue;
        }

        for (const BackupEntry& entry : entries) {
            if (!entry.tempPath.isEmpty() && pathExists(entry.tempPath, entry.isDirectory)) {
                if (!removePath(entry.tempPath, entry.isDirectory)) {
                    qWarning() << "TempFileManager: Failed to remove orphaned temp path during recovery:"
                               << entry.tempPath;
                    recoveredAll = false;
                }
            }

            if (entry.existedBefore && !pathExists(entry.finalPath, entry.isDirectory) &&
                pathExists(entry.backupPath, entry.isDirectory)) {
                if (!movePathWithFallback(entry.backupPath, entry.finalPath, entry.isDirectory)) {
                    qWarning() << "TempFileManager: Failed to restore backup during recovery:" << entry.backupPath
                               << "to" << entry.finalPath;
                    recoveredAll = false;
                }
            }
        }
    }

    const QDateTime expirationCutoff = QDateTime::currentDateTimeUtc().addDays(-kDefaultBackupRetentionDays);
    for (int i = 0; i < committedTransactionDirs.size(); ++i) {
        const QFileInfo& transactionDirInfo = committedTransactionDirs.at(i);
        const bool exceedsMaxSets = i >= kDefaultMaxBackupSets;
        const bool isExpired = transactionDirInfo.lastModified().toUTC() < expirationCutoff;
        if ((exceedsMaxSets || isExpired) && !QDir(transactionDirInfo.absoluteFilePath()).removeRecursively()) {
            qWarning() << "TempFileManager: Failed to prune committed backup set:"
                       << transactionDirInfo.absoluteFilePath();
            recoveredAll = false;
        }
    }

    return recoveredAll;
}

bool TempFileManager::commitBatchLocked(const QVector<CommitItem>& items) {
    if (items.isEmpty()) {
        return true;
    }

    const QString transactionId = createTransactionId();
    const QString transactionDirPath = QDir(backupRootPath()).filePath(transactionId);
    if (!QDir().mkpath(transactionDirPath)) {
        qWarning() << "TempFileManager: Failed to create backup transaction directory:" << transactionDirPath;
        return false;
    }

    QVector<BackupEntry> entries;
    entries.reserve(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const CommitItem& item = items[i];
        if (item.tempPath.isEmpty() || item.finalPath.isEmpty()) {
            qWarning() << "TempFileManager: Cannot commit empty temp/final path";
            return false;
        }
        if (!pathExists(item.tempPath, item.isDirectory)) {
            qWarning() << "TempFileManager: Temp path does not exist:" << item.tempPath;
            m_tempFiles.remove(item.tempPath);
            return false;
        }
        if (!ensureParentDirectory(item.finalPath)) {
            qWarning() << "TempFileManager: Failed to create final parent directory:" << item.finalPath;
            return false;
        }

        const QString backupName =
            QStringLiteral("%1_%2").arg(i, 4, 10, QLatin1Char('0')).arg(QFileInfo(item.finalPath).fileName());
        BackupEntry entry;
        entry.finalPath = item.finalPath;
        entry.backupPath = QDir(transactionDirPath).filePath(backupName);
        entry.tempPath = item.tempPath;
        entry.isDirectory = item.isDirectory;
        entry.existedBefore = pathExists(item.finalPath, item.isDirectory);
        entries.append(entry);
    }

    const QString manifestPath = QDir(transactionDirPath).filePath(QStringLiteral("manifest.json"));
    if (!writeManifest(manifestPath, transactionId, m_outputPath, entries, false)) {
        return false;
    }

    QVector<BackupEntry> backedUpEntries;
    for (const BackupEntry& entry : entries) {
        if (!entry.existedBefore) {
            backedUpEntries.append(entry);
            continue;
        }

        if (!movePathWithFallback(entry.finalPath, entry.backupPath, entry.isDirectory)) {
            qWarning() << "TempFileManager: Failed to back up existing target:" << entry.finalPath << "to"
                       << entry.backupPath;
            for (auto it = backedUpEntries.crbegin(); it != backedUpEntries.crend(); ++it) {
                if (it->existedBefore && pathExists(it->backupPath, it->isDirectory) &&
                    !pathExists(it->finalPath, it->isDirectory)) {
                    movePathWithFallback(it->backupPath, it->finalPath, it->isDirectory);
                }
            }
            return false;
        }

        backedUpEntries.append(entry);
    }

    QVector<BackupEntry> promotedEntries;
    for (const BackupEntry& entry : entries) {
        if (!movePathWithFallback(entry.tempPath, entry.finalPath, entry.isDirectory)) {
            qWarning() << "TempFileManager: Failed to promote temp path:" << entry.tempPath << "to" << entry.finalPath;

            removePath(entry.finalPath, entry.isDirectory);
            for (auto it = promotedEntries.crbegin(); it != promotedEntries.crend(); ++it) {
                removePath(it->finalPath, it->isDirectory);
            }
            for (auto it = backedUpEntries.crbegin(); it != backedUpEntries.crend(); ++it) {
                if (it->existedBefore && pathExists(it->backupPath, it->isDirectory)) {
                    if (!movePathWithFallback(it->backupPath, it->finalPath, it->isDirectory)) {
                        qWarning() << "TempFileManager: Failed to restore backup:" << it->backupPath << "to"
                                   << it->finalPath;
                    }
                }
            }
            return false;
        }

        promotedEntries.append(entry);
    }

    if (!writeManifest(manifestPath, transactionId, m_outputPath, entries, true)) {
        qWarning() << "TempFileManager: Commit succeeded but manifest could not be finalized:" << manifestPath;
    }

    for (const BackupEntry& entry : entries) {
        m_tempFiles.remove(entry.tempPath);
        qDebug() << "TempFileManager: Committed with backup:" << entry.tempPath << "->" << entry.finalPath;
    }

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
    quint64 random = QRandomGenerator::global()->generate64();
    QString uuid = QString::number(random, 16);
    return prefix + QStringLiteral("_") + uuid + suffix;
}

bool TempFileManager::ensureTempDirectory() const {
    QString tempDir = tempDirectory();
    if (tempDir.isEmpty()) {
        return false;
    }

    if (QDir(tempDir).exists()) {
        return true;
    }

    return QDir().mkpath(tempDir);
}

bool TempFileManager::deleteFile(const QString& path) const {
    if (path.isEmpty()) {
        return true;
    }

    QFile file(path);
    if (!file.exists()) {
        return true;
    }

    if (file.remove()) {
        qDebug() << "TempFileManager: Deleted:" << path;
        return true;
    }

    qWarning() << "TempFileManager: Failed to delete:" << path;
    return false;
}

}  // namespace EasyKiConverter
