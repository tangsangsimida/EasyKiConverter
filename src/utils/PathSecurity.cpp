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
    QFileInfo baseInfo(baseDir);
    QFileInfo targetInfo(fullPath);

    // 优先使用 canonicalFilePath (要求路径存在)，它能解析符号链接和短文件名，这是最准确的比较方式
    // 如果路径不存在，回退到 cleanPath(absoluteFilePath)
    QString absBase = baseInfo.exists() ? baseInfo.canonicalFilePath() : QDir::cleanPath(baseInfo.absoluteFilePath());
    QString absTarget =
        targetInfo.exists() ? targetInfo.canonicalFilePath() : QDir::cleanPath(targetInfo.absoluteFilePath());

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
