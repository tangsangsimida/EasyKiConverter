#include "Logger.h"

#include "PatternFormatter.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QThread>

namespace EasyKiConverter {

QThreadStorage<QString> Logger::s_threadNames;

Logger* Logger::instance() {
    static Logger instance;
    return &instance;
}

Logger::Logger() {}

Logger::~Logger() {
    close();
}

void Logger::setGlobalLevel(LogLevel level) {
    QMutexLocker locker(&m_mutex);
    m_globalLevel = level;
}

void Logger::setModuleLevel(LogModule module, LogLevel level) {
    QMutexLocker locker(&m_mutex);
    m_moduleLevels[module] = level;
}

LogLevel Logger::moduleLevel(LogModule module) const {
    QMutexLocker locker(&m_mutex);
    return m_moduleLevels.value(module, m_globalLevel);
}

void Logger::clearModuleLevel(LogModule module) {
    QMutexLocker locker(&m_mutex);
    m_moduleLevels.remove(module);
}

void Logger::clearAllModuleLevels() {
    QMutexLocker locker(&m_mutex);
    m_moduleLevels.clear();
}

void Logger::addAppender(QSharedPointer<IAppender> appender) {
    QMutexLocker locker(&m_mutex);
    if (!m_appenders.contains(appender)) {
        m_appenders.append(appender);
    }
}

void Logger::removeAppender(QSharedPointer<IAppender> appender) {
    QMutexLocker locker(&m_mutex);
    m_appenders.removeOne(appender);
}

void Logger::clearAppenders() {
    QMutexLocker locker(&m_mutex);
    m_appenders.clear();
}

QList<QSharedPointer<IAppender>> Logger::appenders() const {
    QMutexLocker locker(&m_mutex);
    return m_appenders;
}

void Logger::log(LogLevel level,
                 LogModule module,
                 const QString& message,
                 const char* file,
                 const char* function,
                 int line) {
    // 快速检查是否应该记录
    if (!shouldLog(level, module)) {
        return;
    }

    // 构建日志记录
    LogRecord record;
    record.level = level;
    record.module = module;
    record.message = message;
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.threadId = QThread::currentThreadId();

    if (file) {
        record.fileName = QString::fromUtf8(file);
    }
    if (function) {
        record.functionName = QString::fromUtf8(function);
    }
    record.line = line;

    // 获取线程名称
    if (s_threadNames.hasLocalData()) {
        record.threadName = s_threadNames.localData();
    }

    // 发射信号
    emit logRecord(record);

    // 分发到各 Appender
    QMutexLocker locker(&m_mutex);
    for (auto& appender : m_appenders) {
        QString formatted;
        if (appender->formatter()) {
            formatted = appender->formatter()->format(record);
        } else {
            formatted = record.message;
        }
        appender->append(record, formatted);
    }
}

bool Logger::shouldLog(LogLevel level, LogModule module) const {
    QMutexLocker locker(&m_mutex);

    // 获取模块级别，如果没有设置则使用全局级别
    LogLevel effectiveLevel = m_moduleLevels.value(module, m_globalLevel);

    return static_cast<int>(level) >= static_cast<int>(effectiveLevel);
}

void Logger::flush() {
    QMutexLocker locker(&m_mutex);
    for (auto& appender : m_appenders) {
        appender->flush();
    }
}

void Logger::close() {
    QMutexLocker locker(&m_mutex);
    for (auto& appender : m_appenders) {
        appender->close();
    }
}

void Logger::setThreadName(const QString& name) {
    s_threadNames.setLocalData(name);
}

QString Logger::threadName() const {
    if (s_threadNames.hasLocalData()) {
        return s_threadNames.localData();
    }
    return QString();
}

// LogUtils 实现
void LogUtils::replaceFirstPlaceholder(QString& result, const QString& replacement) {
    int pos = result.indexOf(QStringLiteral("{}"));
    if (pos >= 0) {
        result.replace(pos, 2, replacement);
    }
}

// ScopedTracer 实现
ScopedTracer::ScopedTracer(LogModule module, const QString& operation)
    : m_module(module), m_operation(operation), m_startTime(QDateTime::currentMSecsSinceEpoch()) {
    Logger::instance()->log(
        LogLevel::Trace, m_module, LogUtils::format("[BEGIN] {}", m_operation), nullptr, nullptr, 0);
}

ScopedTracer::~ScopedTracer() {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    Logger::instance()->log(
        LogLevel::Trace, m_module, LogUtils::format("[END] {} ({} ms)", m_operation, elapsed), nullptr, nullptr, 0);
}

// TimeLogger 实现
TimeLogger::TimeLogger(LogModule module, const QString& operation)
    : m_module(module), m_operation(operation), m_startTime(QDateTime::currentMSecsSinceEpoch()), m_finished(false) {}

TimeLogger::~TimeLogger() {
    if (!m_finished) {
        finish();
    }
}

void TimeLogger::checkpoint(const QString& name) {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    Logger::instance()->log(LogLevel::Debug,
                            m_module,
                            LogUtils::format("[{}] {} - {} ms", m_operation, name, elapsed),
                            nullptr,
                            nullptr,
                            0);
}

void TimeLogger::finish() {
    if (!m_finished) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;
        Logger::instance()->log(LogLevel::Info,
                                m_module,
                                LogUtils::format("[FINISH] {} ({} ms)", m_operation, elapsed),
                                nullptr,
                                nullptr,
                                0);
        m_finished = true;
    }
}

}  // namespace EasyKiConverter
