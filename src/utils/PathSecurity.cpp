#include "PathSecurity.h"

#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

namespace EasyKiConverter {

bool PathSecurity::isValidPathComponent(const QString& name) {
    if (name.isEmpty())
        return false;
    // 检查是否包含路径分隔符 (正斜杠和反斜杠)
    // 使用 QChar(0x5C) 表示反斜杠，避免字面量转义问题
    if (name.contains('/') || name.contains(QChar(0x5C)))
        return false;
    // 检查是否包含路径遍历
    if (name == ".." || name == ".")
        return false;
    return true;
}

bool PathSecurity::isSafePath(const QString& fullPath, const QString& baseDir) {
    QDir base(baseDir);
    QString canonicalBase = QFileInfo(base.absolutePath()).canonicalFilePath();
    if (canonicalBase.isEmpty()) {
        canonicalBase = QDir::cleanPath(base.absolutePath());
    }

    QString absTarget = QFileInfo(fullPath).absoluteFilePath();
    QString cleanTarget = QDir::cleanPath(absTarget);

    if (!canonicalBase.endsWith('/')) {
        canonicalBase += '/';
    }

    if (!cleanTarget.endsWith('/') && QFileInfo(cleanTarget).isDir()) {
        cleanTarget += '/';
    }

    return cleanTarget.startsWith(canonicalBase, Qt::CaseInsensitive);
}

QString PathSecurity::sanitizeFilename(const QString& name) {
    QString safeName = name;

    // 替换非法字符: \ / : * ? " < > |
    // 使用十六进制转义确保安全: \x5C 是反斜杠, \x22 是双引号
    static const QRegularExpression illegalChars(QStringLiteral("[\\x5C/:*?\\x22<>|]"));
    safeName.replace(illegalChars, "_");

    // 替换控制字符
    static const QRegularExpression controlChars(R"([\x00-\x1f])");
    safeName.replace(controlChars, "");

    // 替换 Unicode 零宽字符 (可能导致显示欺骗)
    static const QRegularExpression zeroWidth(R"([\u200B-\u200D\uFEFF])");
    safeName.replace(zeroWidth, "");

    safeName = safeName.trimmed();

    if (safeName.isEmpty()) {
        safeName = "unnamed_component";
    }

    static const QStringList reservedNames = {"CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
                                              "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
                                              "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

    if (reservedNames.contains(safeName, Qt::CaseInsensitive)) {
        safeName += "_safe";
    }

    return safeName;
}

bool PathSecurity::safeRemoveRecursively(const QString& path, int maxFileCount) {
    QDir dir(path);
    if (!dir.exists())
        return true;

    int count = countFilesRecursively(path, maxFileCount + 1);
    if (count > maxFileCount) {
        qCritical() << "Security Alert: Recursive delete aborted. File count" << count << "exceeds limit of"
                    << maxFileCount << "in path:" << path;
        return false;
    }

    qDebug() << "Safe delete: Removing directory with" << count << "items:" << path;
    return dir.removeRecursively();
}

int PathSecurity::countFilesRecursively(const QString& path, int limit, int currentCount) {
    if (currentCount >= limit)
        return currentCount;

    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    for (const QFileInfo& info : list) {
        currentCount++;
        if (currentCount >= limit)
            return limit;

        if (info.isDir()) {
            if (!info.isSymLink()) {
                currentCount = countFilesRecursively(info.absoluteFilePath(), limit, currentCount);
            } else {
                // 注意：我们跳过符号链接指向的内容，不计入文件总数，也不深入检查
                // removeRecursively 会删除符号链接本身，但不会删除目标
                // 这是预期的安全行为
                currentCount++;
            }
            if (currentCount >= limit)
                return limit;
        } else {
            currentCount++;
        }
    }
    return currentCount;
}

}  // namespace EasyKiConverter
