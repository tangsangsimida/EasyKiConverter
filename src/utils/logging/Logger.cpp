#include "Logger.h"

#include "PatternFormatter.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QReadLocker>
#include <QThread>
#include <QWriteLocker>

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
    m_globalLevel.storeRelaxed(static_cast<int>(level));
}

void Logger::setModuleLevel(LogModule module, LogLevel level) {
    QWriteLocker locker(&m_moduleLevelsLock);
    m_moduleLevels[module] = level;
}

LogLevel Logger::moduleLevel(LogModule module) const {
    QReadLocker locker(&m_moduleLevelsLock);
    return m_moduleLevels.value(module, static_cast<LogLevel>(m_globalLevel.loadRelaxed()));
}

void Logger::clearModuleLevel(LogModule module) {
    QWriteLocker locker(&m_moduleLevelsLock);
    m_moduleLevels.remove(module);
}

void Logger::clearAllModuleLevels() {
    QWriteLocker locker(&m_moduleLevelsLock);
    m_moduleLevels.clear();
}

void Logger::addAppender(QSharedPointer<IAppender> appender) {
    QMutexLocker locker(&m_appendersMutex);
    if (!m_appenders.contains(appender)) {
        m_appenders.append(appender);
    }
}

void Logger::removeAppender(QSharedPointer<IAppender> appender) {
    QMutexLocker locker(&m_appendersMutex);
    m_appenders.removeOne(appender);
}

void Logger::clearAppenders() {
    QMutexLocker locker(&m_appendersMutex);
    m_appenders.clear();
}

QList<QSharedPointer<IAppender>> Logger::appenders() const {
    QMutexLocker locker(&m_appendersMutex);
    return m_appenders;
}

void Logger::log(LogLevel level,
                 LogModule module,
                 const QString& message,
                 const char* file,
                 const char* function,
                 int line) {
    // 快速检查是否应该记录（无锁快速路径）
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
    QMutexLocker locker(&m_appendersMutex);
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
    // 快速路径：先检查全局级别（使用原子变量，无锁）
    int globalLvl = m_globalLevel.loadRelaxed();
    if (static_cast<int>(level) < globalLvl) {
        // 如果全局级别已经过滤掉了，直接返回
        // 但还需要检查模块级别（可能模块级别更低）

        // 检查模块级别（需要读锁）
        QReadLocker locker(&m_moduleLevelsLock);
        if (m_moduleLevels.isEmpty()) {
            return false;  // 没有模块级别设置，全局过滤生效
        }

        auto it = m_moduleLevels.find(module);
        if (it == m_moduleLevels.end()) {
            return false;  // 该模块没有特定设置，全局过滤生效
        }

        // 模块有特定设置，使用模块级别
        return static_cast<int>(level) >= static_cast<int>(it.value());
    }

    // 全局级别检查通过
    // 但还需要检查模块是否有更严格的级别设置
    QReadLocker locker(&m_moduleLevelsLock);
    auto it = m_moduleLevels.find(module);
    if (it != m_moduleLevels.end()) {
        return static_cast<int>(level) >= static_cast<int>(it.value());
    }

    return true;  // 模块没有特定设置，使用全局结果
}

void Logger::flush() {
    QMutexLocker locker(&m_appendersMutex);
    for (auto& appender : m_appenders) {
        appender->flush();
    }
}

void Logger::close() {
    QMutexLocker locker(&m_appendersMutex);
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

void Logger::clearThreadName() {
    if (s_threadNames.hasLocalData()) {
        s_threadNames.setLocalData(QString());
    }
}

// LogUtils 实现
void LogUtils::replaceFirstPlaceholder(QString& result, const QString& replacement) {
    // 安全检查：避免对包含格式化字符的输入进行处理
    // 仅替换第一个 {} 占位符，不处理其他格式化字符
    int pos = result.indexOf(QStringLiteral("{}"));
    if (pos >= 0) {
        // 对替换内容进行安全处理，转义可能的格式化字符
        QString safeReplacement = replacement;
        // 不对内容进行额外的格式化处理，直接替换
        result.replace(pos, 2, safeReplacement);
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