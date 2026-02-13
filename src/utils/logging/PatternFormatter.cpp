#include "PatternFormatter.h"

#include <QDateTime>
#include <QRegularExpression>

namespace EasyKiConverter {

const QRegularExpression PatternFormatter::s_placeholderRegex(
    QStringLiteral("%\\{([a-zA-Z]+)\\}"));

PatternFormatter::PatternFormatter(const QString& pattern)
    : m_pattern(pattern)
{
}

QString PatternFormatter::format(const LogRecord& record) {
    QString result = m_pattern;
    
    QRegularExpressionMatchIterator it = s_placeholderRegex.globalMatch(m_pattern);
    QStringList processed;
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString placeholder = match.captured(1).toLower();
        
        // 避免重复处理相同的占位符
        QString fullMatch = match.captured(0);
        if (processed.contains(fullMatch)) {
            continue;
        }
        processed.append(fullMatch);
        
        QString replacement = replacePlaceholder(placeholder, record);
        result.replace(fullMatch, replacement);
    }
    
    // 处理 %% 转义
    result.replace(QStringLiteral("%%"), QStringLiteral("%"));
    
    return result;
}

QSharedPointer<IFormatter> PatternFormatter::clone() const {
    return QSharedPointer<PatternFormatter>::create(m_pattern);
}

void PatternFormatter::setPattern(const QString& pattern) {
    m_pattern = pattern;
}

QString PatternFormatter::replacePlaceholder(const QString& placeholder, const LogRecord& record) const {
    if (placeholder == "timestamp") {
        return record.formattedTimestamp();
    }
    if (placeholder == "time") {
        return QDateTime::fromMSecsSinceEpoch(record.timestamp).toString("HH:mm:ss.zzz");
    }
    if (placeholder == "date") {
        return QDateTime::fromMSecsSinceEpoch(record.timestamp).toString("yyyy-MM-dd");
    }
    if (placeholder == "level") {
        return logLevelToString(record.level);
    }
    if (placeholder == "module") {
        return logModuleToString(record.module);
    }
    if (placeholder == "thread") {
        return QString::number(reinterpret_cast<quintptr>(record.threadId), 16).toUpper().rightJustified(5, '0');
    }
    if (placeholder == "threadname") {
        return record.threadName.isEmpty() 
            ? QString::number(reinterpret_cast<quintptr>(record.threadId), 16).toUpper().rightJustified(5, '0')
            : record.threadName;
    }
    if (placeholder == "file") {
        return record.fileName;
    }
    if (placeholder == "shortfile") {
        return record.shortFileName();
    }
    if (placeholder == "line") {
        return QString::number(record.line);
    }
    if (placeholder == "function") {
        return record.functionName;
    }
    if (placeholder == "message") {
        return record.message;
    }
    if (placeholder == "newline") {
        return QStringLiteral("\n");
    }
    
    // 未知占位符，保持原样
    return QStringLiteral("%{") + placeholder + QStringLiteral("}");
}

} // namespace EasyKiConverter
