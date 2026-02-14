#ifndef LOGGER_H
#define LOGGER_H

#include "IAppender.h"
#include "IFormatter.h"
#include "LogLevel.h"
#include "LogModule.h"
#include "LogRecord.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QSharedPointer>
#include <QThreadStorage>

namespace EasyKiConverter {

/**
 * @brief 日志管理器
 * 
 * 单例模式的日志管理器，负责：
 * - 管理全局日志级别和模块级别
 * - 分发日志到各个 Appender
 * - 提供线程安全的日志接口
 * 
 * 性能优化：
 * - 使用原子变量存储全局日志级别，避免锁竞争
 * - 使用读写锁保护模块级别映射
 * - shouldLog() 快速路径无需获取锁
 */
class Logger : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static Logger* instance();

    /**
     * @brief 设置全局日志级别
     */
    void setGlobalLevel(LogLevel level);

    /**
     * @brief 获取全局日志级别
     */
    LogLevel globalLevel() const {
        return static_cast<LogLevel>(m_globalLevel.loadRelaxed());
    }

    /**
     * @brief 设置特定模块的日志级别
     */
    void setModuleLevel(LogModule module, LogLevel level);

    /**
     * @brief 获取特定模块的日志级别
     */
    LogLevel moduleLevel(LogModule module) const;

    /**
     * @brief 清除模块级别设置（恢复使用全局级别）
     */
    void clearModuleLevel(LogModule module);

    /**
     * @brief 清除所有模块级别设置
     */
    void clearAllModuleLevels();

    /**
     * @brief 添加 Appender
     */
    void addAppender(QSharedPointer<IAppender> appender);

    /**
     * @brief 移除 Appender
     */
    void removeAppender(QSharedPointer<IAppender> appender);

    /**
     * @brief 清除所有 Appender
     */
    void clearAppenders();

    /**
     * @brief 获取所有 Appender
     */
    QList<QSharedPointer<IAppender>> appenders() const;

    /**
     * @brief 核心日志方法
     * @param level 日志级别
     * @param module 日志模块
     * @param message 日志消息
     * @param file 源文件名（可选）
     * @param function 函数名（可选）
     * @param line 行号（可选）
     */
    void log(LogLevel level,
             LogModule module,
             const QString& message,
             const char* file = nullptr,
             const char* function = nullptr,
             int line = 0);

    /**
     * @brief 检查是否应该记录日志（用于条件求值优化）
     * 
     * 性能优化：快速路径使用原子变量，无需获取锁
     */
    bool shouldLog(LogLevel level, LogModule module) const;

    /**
     * @brief 刷新所有 Appender
     */
    void flush();

    /**
     * @brief 关闭所有 Appender
     */
    void close();

    /**
     * @brief 设置线程名称（用于日志输出）
     */
    void setThreadName(const QString& name);

    /**
     * @brief 获取当前线程名称
     */
    QString threadName() const;

    /**
     * @brief 清除当前线程的名称设置
     */
    void clearThreadName();

signals:
    /**
     * @brief 日志记录信号（可用于 UI 集成）
     */
    void logRecord(const LogRecord& record);

private:
    Logger();
    ~Logger();

    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 使用原子变量存储全局日志级别，实现无锁快速检查
    mutable QAtomicInt m_globalLevel{static_cast<int>(LogLevel::Info)};

    // 模块级别使用读写锁保护
    QHash<LogModule, LogLevel> m_moduleLevels;
    mutable QReadWriteLock m_moduleLevelsLock;

    // Appender 列表使用互斥锁保护
    QList<QSharedPointer<IAppender>> m_appenders;
    mutable QMutex m_appendersMutex;

    // 线程本地存储的线程名称
    static QThreadStorage<QString> s_threadNames;
};

/**
 * @brief 日志工具类
 */
class LogUtils {
public:
    /**
     * @brief 格式化字符串（支持 {} 占位符）
     * 
     * 示例：
     * format("Hello {}", "World") -> "Hello World"
     * format("Count: {}, Value: {}", 42, 3.14) -> "Count: 42, Value: 3.14"
     */
    template <typename... Args>
    static QString format(const QString& pattern, const Args&... args) {
        QString result = pattern;
        formatHelper(result, args...);
        return result;
    }

    /**
     * @brief 格式化字符串（单参数版本）
     */
    static QString format(const QString& pattern) {
        return pattern;
    }

private:
    template <typename T, typename... Args>
    static void formatHelper(QString& result, const T& first, const Args&... rest) {
        replaceFirstPlaceholder(result, toString(first));
        formatHelper(result, rest...);
    }

    static void formatHelper(QString& /*result*/) {
        // 递归终止
    }

    template <typename T>
    static QString toString(const T& value) {
        if constexpr (std::is_same_v<T, QString>) {
            return value;
        } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
            return QString::fromUtf8(value);
        } else if constexpr (std::is_arithmetic_v<T>) {
            return QString::number(value);
        } else {
            return QString(value);
        }
    }

    static void replaceFirstPlaceholder(QString& result, const QString& replacement);
};

/**
 * @brief 性能追踪辅助类
 * 
 * 用于自动记录代码块的执行时间
 */
class ScopedTracer {
public:
    ScopedTracer(LogModule module, const QString& operation);
    ~ScopedTracer();

private:
    LogModule m_module;
    QString m_operation;
    qint64 m_startTime;
};

/**
 * @brief 时间日志记录器
 */
class TimeLogger {
public:
    TimeLogger(LogModule module, const QString& operation);
    ~TimeLogger();

    void checkpoint(const QString& name);
    void finish();

private:
    LogModule m_module;
    QString m_operation;
    qint64 m_startTime;
    bool m_finished;
};

}  // namespace EasyKiConverter

#endif  // LOGGER_H