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
    if (fullPath.isEmpty() || baseDir.isEmpty()) {
        qWarning() << "isSafePath: received empty path";
        return false;
    }

    // 1. 获取绝对路径并规范化
    // 注意：canonicalFilePath() 在应用程序退出时可能会崩溃，因为 Qt 内部对象可能已被销毁
    // 使用 QFile::exists() 先检查，避免调用 canonicalFilePath() 在不存在的路径上
    QFileInfo baseInfo(baseDir);
    QFileInfo targetInfo(fullPath);

    // 安全获取绝对路径 - 避免在退出时调用 canonicalFilePath()
    QString absBase;
    QString absTarget;

    // 检查路径是否存在且可访问
    if (!baseInfo.exists() || !baseInfo.isReadable()) {
        // 路径不存在时使用 cleanPath，这是安全的
        absBase = QDir::cleanPath(baseInfo.absoluteFilePath());
    } else {
        // 路径存在但避免使用 canonicalFilePath，改用 absoluteFilePath + cleanPath
        // canonicalFilePath() 会调用 Qt 内部文件引擎，在退出时可能崩溃
        absBase = QDir::cleanPath(baseInfo.absoluteFilePath());
    }

    if (!targetInfo.exists() || !targetInfo.isReadable()) {
        absTarget = QDir::cleanPath(targetInfo.absoluteFilePath());
    } else {
        absTarget = QDir::cleanPath(targetInfo.absoluteFilePath());
    }

    // 2. 统一分隔符为 '/' (Qt 默认)
    absBase = QDir::fromNativeSeparators(absBase);
    absTarget = QDir::fromNativeSeparators(absTarget);

    // 3. 确保基准路径以 '/' 结尾，用于目录前缀匹配
    if (!absBase.endsWith('/')) {
        absBase += '/';
    }

    // 4. 特殊情况处理：允许目标路径等于基准路径
    // 移除 target 可能存在的末尾斜杠
    QString targetNoSlash = absTarget;
    while (targetNoSlash.endsWith('/'))
        targetNoSlash.chop(1);

    // 如果 target + '/' == base，说明它们代表同一个目录
    if (targetNoSlash + '/' == absBase) {
        return true;
    }
    // 或者直接比较（忽略大小写，适应 Windows）
    if (targetNoSlash.compare(absBase.left(absBase.length() - 1), Qt::CaseInsensitive) == 0) {
        return true;
    }

    // 5. 前缀比较：检查 target 是否以 base 开头
    bool isSafe = absTarget.startsWith(absBase, Qt::CaseInsensitive);

    if (!isSafe) {
        qWarning() << "isSafePath: path is outside base directory";
        qWarning() << "  Target (norm):" << absTarget;
        qWarning() << "  Base   (norm):" << absBase;
        qWarning() << "  Raw Target:" << fullPath;
        qWarning() << "  Raw Base:  " << baseDir;
    }

    return isSafe;
}

bool PathSecurity::isSafePathCanonical(const QString& fullPath, const QString& baseDir) {
    if (fullPath.isEmpty() || baseDir.isEmpty()) {
        qWarning() << "isSafePathCanonical: received empty path";
        return false;
    }

    const QString canonBase = canonicalizePath(baseDir);
    const QString canonTarget = canonicalizePath(fullPath);

    if (canonBase.isEmpty() || canonTarget.isEmpty()) {
        qWarning() << "isSafePathCanonical: failed to canonicalize paths";
        return false;
    }

    // 确保基准路径以 '/' 结尾
    QString normalizedBase = canonBase;
    if (!normalizedBase.endsWith('/')) {
        normalizedBase += '/';
    }

    // 允许目标等于基准
    QString targetNoSlash = canonTarget;
    while (targetNoSlash.endsWith('/'))
        targetNoSlash.chop(1);

    if (targetNoSlash + '/' == normalizedBase) {
        return true;
    }
    if (targetNoSlash.compare(normalizedBase.left(normalizedBase.length() - 1), Qt::CaseInsensitive) == 0) {
        return true;
    }

    bool isSafe = canonTarget.startsWith(normalizedBase, Qt::CaseInsensitive);

    if (!isSafe) {
        qWarning() << "isSafePathCanonical: path is outside base directory";
        qWarning() << "  Target (canonical):" << canonTarget;
        qWarning() << "  Base   (canonical):" << normalizedBase;
        qWarning() << "  Raw Target:" << fullPath;
        qWarning() << "  Raw Base:  " << baseDir;
    }

    return isSafe;
}

QString PathSecurity::canonicalizePath(const QString& path) {
    QFileInfo info(path);

    // 如果路径存在，直接用 canonicalFilePath（解析符号链接）
    if (info.exists()) {
        QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty()) {
            return QDir::fromNativeSeparators(canonical);
        }
    }

    // 路径不存在：找到最近存在的父目录，对其 canonicalize，再拼接剩余部分
    QString remaining;
    QString current = QDir::cleanPath(path);

    // 逐级向上查找存在的父目录
    while (!QFileInfo::exists(current)) {
        QDir parent = QDir(current);
        if (!parent.cdUp()) {
            // 到达根目录仍不存在，回退到 cleanPath
            return QDir::fromNativeSeparators(QDir::cleanPath(path));
        }
        QString childName = QDir(current).dirName();
        remaining = remaining.isEmpty() ? childName : (childName + "/" + remaining);
        current = parent.path();
    }

    // current 现在是存在的路径，对其 canonicalize
    QFileInfo currentInfo(current);
    QString canonCurrent = currentInfo.canonicalFilePath();
    if (canonCurrent.isEmpty()) {
        canonCurrent = QDir::cleanPath(current);
    }

    if (remaining.isEmpty()) {
        return QDir::fromNativeSeparators(canonCurrent);
    }
    return QDir::fromNativeSeparators(canonCurrent + "/" + remaining);
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
    // 使用 QChar 移除这些特殊字符
    safeName.remove(QChar(0x200B));  // ZERO WIDTH SPACE
    safeName.remove(QChar(0x200C));  // ZERO WIDTH NON-JOINDER
    safeName.remove(QChar(0x200D));  // ZERO WIDTH JOINER
    safeName.remove(QChar(0xFEFF));  // ZERO WIDTH NO-BREAK SPACE

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
