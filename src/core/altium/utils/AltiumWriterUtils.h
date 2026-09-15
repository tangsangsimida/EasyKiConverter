#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief Altium 写入器共享工具
 * @details 提供 SchLib 和 PcbLib 写入器共用的工具函数。
 */
namespace AltiumWriterUtils {

/**
 * @brief 校验嵌入图片在 SchLib Storage 中使用的文件名。
 * @param name 不包含目录的文件名
 * @return 文件名是否符合 Storage 和 Windows 文件名约束
 */
inline bool isValidImageStorageName(const QString& name) {
    if (name.trimmed() != name || name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..") ||
        name.size() > 255)
        return false;
    if (name.contains(QChar::Null) || name.contains(QChar('\n')) || name.contains(QChar('\r')) ||
        name.contains(QChar('\t')))
        return false;
    for (const QChar ch : name) {
        if (ch.unicode() < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(ch))
            return false;
    }
    if (name.endsWith(QChar(' ')) || name.endsWith(QChar('.')))
        return false;
    if (name.contains(QChar(0x200B)) || name.contains(QChar(0x200C)) || name.contains(QChar(0x200D)) ||
        name.contains(QChar(0xFEFF)))
        return false;

    QString deviceName = name;
    const qsizetype extensionStart = deviceName.indexOf(QChar('.'));
    if (extensionStart >= 0)
        deviceName.truncate(extensionStart);
    static const QStringList reservedNames = {
        QStringLiteral("CON"),  QStringLiteral("PRN"),  QStringLiteral("AUX"),  QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")};
    if (reservedNames.contains(deviceName.trimmed(), Qt::CaseInsensitive))
        return false;

    return name.toLocal8Bit().size() <= 255;
}

/**
 * @brief 获取元件的 Section Key（OLE 存储键）
 * @param name 元件名称
 * @return 符合 CFB 31 字符限制的安全键名
 *
 * @details Altium OLE 复合文档中，每个元件存储在以 SectionKey 命名的存储区中。
 *          本项目在 IR 名称与物理 storage 名称之间建立显式映射：保留常用可读字符，
 *          其余字符归一化，并由 makeUniqueSectionKeys() 按 CFB 的大小写规则消除冲突。
 *
 * @code
 * QString key = getSectionKey("Resistor/0402");
 * // key = "Resistor_0402"
 * @endcode
 */
inline QString getSectionKey(const QString& name) {
    QString key;
    const QString trimmed = name.trimmed();
    for (const QChar ch : trimmed) {
        if (key.size() >= 31) {
            break;
        }
        const bool asciiAlphaNumeric = ch.unicode() < 128 && ch.isLetterOrNumber();
        key += asciiAlphaNumeric || ch == '_' || ch == '-' || ch == '.' ? ch : QChar('_');
    }
    if (key.isEmpty()) {
        key = "_";
    }
    return key;
}

/**
 * @brief 为一组库条目生成互不冲突的 CFB storage 名称
 * @details CFB 名称比较不区分大小写；截断后的冲突通过 _2、_3 后缀消解。
 */
inline QStringList makeUniqueSectionKeys(const QStringList& names) {
    QStringList result;
    QSet<QString> used;
    for (const QString& name : names) {
        const QString base = getSectionKey(name);
        QString candidate = base;
        int suffixNumber = 2;
        while (used.contains(candidate.toCaseFolded())) {
            const QString suffix = QString("_%1").arg(suffixNumber++);
            candidate = base.left(qMax(1, 31 - suffix.size())) + suffix;
        }
        used.insert(candidate.toCaseFolded());
        result.append(candidate);
    }
    return result;
}

}  // namespace AltiumWriterUtils
}  // namespace EasyKiConverter
