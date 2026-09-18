#include "TempFileManager.h"

#include "TempFileTransactionUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QThread>

namespace EasyKiConverter {

namespace {

constexpr int kDefaultBackupRetentionDays = 3;
constexpr int kDefaultMaxBackupSets = 3;

QMutex g_tempDirectoryUsersMutex;
QHash<QString, int> g_tempDirectoryUsers;

// 记录共享临时目录的使用者数量。
void registerTempDirectoryUser(const QString& path) {
    if (path.isEmpty())
        return;
    QMutexLocker locker(&g_tempDirectoryUsersMutex);
    ++g_tempDirectoryUsers[path];
}

// 释放共享临时目录的一个使用者引用。
void unregisterTempDirectoryUser(const QString& path) {
    if (path.isEmpty())
        return;
    QMutexLocker locker(&g_tempDirectoryUsersMutex);
    auto it = g_tempDirectoryUsers.find(path);
    if (it == g_tempDirectoryUsers.end())
        return;
    if (--it.value() <= 0)
        g_tempDirectoryUsers.erase(it);
}

// 判断临时目录是否仍有任意阶段在使用。
bool hasTempDirectoryUsers(const QString& path) {
    QMutexLocker locker(&g_tempDirectoryUsersMutex);
    return g_tempDirectoryUsers.value(path, 0) > 0;
}

// 判断临时目录是否由当前阶段之外的阶段使用。
bool hasOtherTempDirectoryUsers(const QString& path) {
    QMutexLocker locker(&g_tempDirectoryUsersMutex);
    return g_tempDirectoryUsers.value(path, 0) > 1;
}

using TempFileTransaction::BackupEntry;
using TempFileTransaction::backupRootPath;
using TempFileTransaction::createTransactionId;
using TempFileTransaction::ensureParentDirectory;
using TempFileTransaction::movePathWithFallback;
using TempFileTransaction::pathExists;
using TempFileTransaction::readManifestEntries;
using TempFileTransaction::removePath;
using TempFileTransaction::writeManifest;

}  // namespace

// 初始化临时文件管理器。
TempFileManager::TempFileManager(QObject* parent) : QObject(parent) {}

TempFileManager::~TempFileManager() {
    // 析构时不调用 rollbackAll()，避免删除其他 Stage 共享的 .tmp 目录
    // 只清理本实例注册的临时文件（如果还存在的话）
    QMutexLocker locker(&m_mutex);
    for (const QString& tempPath : m_tempFiles) {
        if (QFileInfo(tempPath).isDir()) {
            QDir(tempPath).removeRecursively();
        } else {
            QFile::remove(tempPath);
        }
    }
    m_tempFiles.clear();
    const QString releasedTempDirectory = m_registeredTempDirectory;
    unregisterTempDirectoryUser(releasedTempDirectory);
    m_registeredTempDirectory.clear();
    if (!releasedTempDirectory.isEmpty() && !hasTempDirectoryUsers(releasedTempDirectory)) {
        QDir tempDir(releasedTempDirectory);
        if (tempDir.exists() && tempDir.isEmpty())
            QDir().rmdir(releasedTempDirectory);
    }
}

// 设置导出输出目录并更新共享临时目录引用。
void TempFileManager::setOutputPath(const QString& outputPath) {
    QMutexLocker locker(&m_mutex);
    const QString newTempDirectory = outputPath.isEmpty() ? QString() : QDir(outputPath).filePath(m_tempDirName);
    if (newTempDirectory != m_registeredTempDirectory) {
        unregisterTempDirectoryUser(m_registeredTempDirectory);
        registerTempDirectoryUser(newTempDirectory);
        m_registeredTempDirectory = newTempDirectory;
    }
    m_outputPath = outputPath;
}

// 返回当前输出目录下的共享临时目录。
QString TempFileManager::tempDirectory() const {
    if (m_outputPath.isEmpty()) {
        return QString();
    }
    // 使用与 setOutputPath() 注册引用时相同的路径构造方式，避免 Windows
    // 下不同路径分隔符导致共享临时目录引用计数无法匹配。
    return QDir(m_outputPath).filePath(m_tempDirName);
}

// 创建并登记单个元器件的临时文件路径。
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

// 根据最终文件名查找已登记的临时文件路径。
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

// 创建并登记符号库文件的临时路径。
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

// 创建并登记目录型导出结果的临时路径。
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

// 将匹配的临时文件提交到最终文件路径。
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

// 将匹配的临时目录提交到最终目录路径。
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

// 带备份地提交单个临时文件。
bool TempFileManager::commitWithBackup(const QString& tempPath, const QString& finalPath) {
    QMutexLocker locker(&m_mutex);
    const bool committed = commitBatchLocked({CommitItem{tempPath, finalPath, false}});

    locker.unlock();
    emit commitCompleted(finalPath, committed);

    return committed;
}

// 带备份地提交单个临时目录。
bool TempFileManager::commitDirectoryWithBackup(const QString& tempDirPath, const QString& finalDirPath) {
    QMutexLocker locker(&m_mutex);
    const bool committed = commitBatchLocked({CommitItem{tempDirPath, finalDirPath, true}});

    locker.unlock();
    emit commitCompleted(finalDirPath, committed);

    return committed;
}

// 原子提交一组文件或目录，并为每项发出提交结果信号。
bool TempFileManager::commitBatch(const QVector<CommitItem>& items) {
    QMutexLocker locker(&m_mutex);
    const bool committed = commitBatchLocked(items);

    locker.unlock();
    for (const CommitItem& item : items) {
        emit commitCompleted(item.finalPath, committed);
    }

    return committed;
}

// 扫描并恢复未完成的备份事务，同时清理过期备份。
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

// 在持有互斥锁时执行带备份的批量提交事务。
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
    cleanupEmptyTempDirectoryLocked();

    return true;
}

// 删除本实例登记的全部临时文件并回收空临时目录。
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
    cleanupEmptyTempDirectoryLocked();

    locker.unlock();
    emit cleanupCompleted(deletedCount);

    qDebug() << "TempFileManager: Rolled back" << deletedCount << "temp files";
}

// 清理当前临时目录中的普通临时文件。
void TempFileManager::cleanupTempDirectory() {
    QMutexLocker locker(&m_mutex);

    QString tempDir = tempDirectory();
    if (tempDir.isEmpty() || !QDir(tempDir).exists()) {
        return;
    }
    // 共享临时目录仍被其他导出阶段使用时，不能清理其中的文件。
    if (hasOtherTempDirectoryUsers(tempDir)) {
        qDebug() << "TempFileManager: Skip cleanup while another stage uses" << tempDir;
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

// 清理临时目录中未登记的孤立文件。
void TempFileManager::cleanupOrphanedTempFiles() {
    QString tempDir = tempDirectory();
    if (tempDir.isEmpty() || !QDir(tempDir).exists()) {
        return;
    }
    // 孤立文件清理同样必须尊重共享目录引用，避免破坏并行导出任务。
    if (hasOtherTempDirectoryUsers(tempDir)) {
        qDebug() << "TempFileManager: Skip orphan cleanup while another stage uses" << tempDir;
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

// 登记一个由本实例负责回收的临时路径。
void TempFileManager::registerTempFile(const QString& tempPath) {
    QMutexLocker locker(&m_mutex);
    m_tempFiles.insert(tempPath);
}

// 返回当前实例登记的临时路径集合快照。
QSet<QString> TempFileManager::registeredTempFiles() const {
    QMutexLocker locker(&m_mutex);
    return m_tempFiles;
}

// 根据前缀、后缀和随机值生成临时文件名。
QString TempFileManager::generateUniqueTempName(const QString& prefix, const QString& suffix) const {
    quint64 random = QRandomGenerator::global()->generate64();
    QString uuid = QString::number(random, 16);
    return prefix + QStringLiteral("_") + uuid + suffix;
}

// 确保当前共享临时目录存在。
bool TempFileManager::ensureTempDirectory() const {
    QString tempDir = tempDirectory();
    if (tempDir.isEmpty()) {
        qWarning() << "TempFileManager::ensureTempDirectory: tempDir is empty, m_outputPath:" << m_outputPath;
        return false;
    }

    if (QDir(tempDir).exists()) {
        return true;
    }

    bool result = QDir().mkpath(tempDir);
    qDebug() << "TempFileManager::ensureTempDirectory: Created" << tempDir << "result:" << result;
    return result;
}

// 删除指定文件，并将不存在的文件视为已完成清理。
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

// 在持锁状态下清理空的共享临时目录。
bool TempFileManager::cleanupEmptyTempDirectoryLocked() const {
    const QString tempDir = tempDirectory();
    if (tempDir.isEmpty()) {
        return true;
    }

    QDir dir(tempDir);
    if (!dir.exists()) {
        return true;
    }

    // 多个导出阶段共享同一个 .tmp 根目录，不能由其中一个阶段提前删除。
    if (hasOtherTempDirectoryUsers(tempDir)) {
        return false;
    }

    const QFileInfoList entries =
        dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    if (!entries.isEmpty()) {
        return false;
    }

    if (QDir().rmdir(tempDir)) {
        qDebug() << "TempFileManager: Removed empty temp directory:" << tempDir;
        return true;
    }

    qWarning() << "TempFileManager: Failed to remove empty temp directory:" << tempDir;
    return false;
}

}  // namespace EasyKiConverter
