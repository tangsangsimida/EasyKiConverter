#ifndef TEMPFILETRANSACTIONUTILS_H
#define TEMPFILETRANSACTIONUTILS_H

#include <QString>
#include <QVector>

namespace EasyKiConverter::TempFileTransaction {

/** @brief 记录一次备份事务中临时路径、最终路径和备份路径的映射。 */
struct BackupEntry {
    QString finalPath;
    QString backupPath;
    QString tempPath;
    bool isDirectory = false;
    bool existedBefore = false;
};

/** @brief 返回应用数据目录下的备份事务根目录。 */
QString backupRootPath();

/** @brief 创建具有时间和随机后缀的事务标识。 */
QString createTransactionId();

/** @brief 确保目标路径的父目录存在。 */
bool ensureParentDirectory(const QString& path);

/** @brief 按路径类型移动对象，并在跨卷或占用时使用复制回退。 */
bool movePathWithFallback(const QString& sourcePath, const QString& targetPath, bool isDirectory);

/** @brief 按路径类型删除对象，目标不存在时视为成功。 */
bool removePath(const QString& path, bool isDirectory);

/** @brief 按路径类型检查对象是否存在。 */
bool pathExists(const QString& path, bool isDirectory);

/** @brief 写入备份事务清单。 */
bool writeManifest(const QString& manifestPath,
                   const QString& transactionId,
                   const QString& outputRoot,
                   const QVector<BackupEntry>& entries,
                   bool committed);

/** @brief 读取备份事务清单，并返回提交状态和路径映射。 */
QVector<BackupEntry> readManifestEntries(const QString& manifestPath, bool* committed);

}  // namespace EasyKiConverter::TempFileTransaction

#endif  // TEMPFILETRANSACTIONUTILS_H
