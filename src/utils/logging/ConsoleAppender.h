#ifndef CONSOLEAPPENDER_H
#define CONSOLEAPPENDER_H

#include "IAppender.h"
#include "LogLevel.h"

#include <QAtomicInt>
#include <QHash>
#include <QMutex>
#include <QQueue>
#include <QString>
#include <QThread>
#include <QWaitCondition>

namespace EasyKiConverter {

/**
 * @brief 控制台日志输出器
 * 
 * 支持彩色输出的控制台 Appender，根据日志级别自动着色：
 * - Trace: 灰色
 * - Debug: 青色
 * - Info:  白色（默认）
 * - Warn:  黄色
 * - Error: 红色
 * - Fatal: 红底白字
 * 
 * 支持异步输出模式，减少对主线程的性能影响。
 */
class ConsoleAppender : public IAppender {
public:
    /**
     * @brief 构造函数
     * @param useColors 是否启用彩色输出
     * @param async 是否启用异步输出（默认 false）
     */
    explicit ConsoleAppender(bool useColors = true, bool async = false);

    /**
     * @brief 析构函数
     */
    ~ConsoleAppender() override;

    /**
     * @brief 输出日志到控制台
     */
    void append(const LogRecord& record, const QString& formatted) override;

    /**
     * @brief 刷新输出流
     */
    void flush() override;

    /**
     * @brief 关闭输出器
     */
    void close() override;

    /**
     * @brief 启用/禁用彩色输出
     */
    void setUseColors(bool useColors) {
        m_useColors = useColors;
    }

    /**
     * @brief 是否启用彩色输出
     */
    bool useColors() const {
        return m_useColors;
    }

    /**
     * @brief 设置特定级别的颜色
     */
    void setLevelColor(LogLevel level, const QString& colorCode);

    /**
     * @brief 设置特定级别的背景色（用于 Fatal）
     */
    void setLevelBackground(LogLevel level, const QString& bgColorCode);

    /**
     * @brief 设置异步队列最大大小
     */
    void setMaxQueueSize(int size) {
        m_maxQueueSize = size;
    }

private:
    bool m_useColors;
    bool m_async;
    QMutex m_mutex;

    // 异步写入相关
    QThread* m_writerThread = nullptr;
    QMutex m_queueMutex;
    QQueue<QByteArray> m_writeQueue;
    QWaitCondition m_queueCondition;
    QAtomicInt m_running;
    QAtomicInt m_flushRequested;
    QAtomicInt m_queueOverflow;
    int m_maxQueueSize = 1000;  // 最大队列大小

    // ANSI 颜色代码
    QHash<LogLevel, QString> m_levelColors;
    QHash<LogLevel, QString> m_levelBackgrounds;

    // ANSI 转义序列
    static const QString s_colorReset;
    static const QString s_colorGray;
    static const QString s_colorCyan;
    static const QString s_colorWhite;
    static const QString s_colorYellow;
    static const QString s_colorRed;
    static const QString s_bgRed;

    /**
     * @brief 检测终端是否支持颜色
     */
    static bool supportsColors();

    /**
     * @brief 获取带颜色的字符串
     */
    QString colorize(const QString& text, LogLevel level) const;

    /**
     * @brief 异步写入线程函数
     */
    void writerThreadFunc();

    /**
     * @brief 直接写入控制台
     */
    void writeDirect(const QByteArray& data, bool isError);
};

}  // namespace EasyKiConverter

#endif  // CONSOLEAPPENDER_H