#include "ConsoleAppender.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

#include <cstdio>

#ifdef Q_OS_WIN
#    include <windows.h>
#endif

namespace EasyKiConverter {

// ANSI 转义序列定义
const QString ConsoleAppender::s_colorReset = QStringLiteral("\033[0m");
const QString ConsoleAppender::s_colorGray = QStringLiteral("\033[90m");
const QString ConsoleAppender::s_colorCyan = QStringLiteral("\033[36m");
const QString ConsoleAppender::s_colorWhite = QStringLiteral("\033[37m");
const QString ConsoleAppender::s_colorYellow = QStringLiteral("\033[33m");
const QString ConsoleAppender::s_colorRed = QStringLiteral("\033[31m");
const QString ConsoleAppender::s_bgRed = QStringLiteral("\033[41m");

ConsoleAppender::ConsoleAppender(bool useColors, bool async)
    : m_useColors(useColors && supportsColors())
    , m_async(async)
    , m_running(0)
    , m_flushRequested(0)
    , m_queueOverflow(0) {
    // 设置默认颜色
    m_levelColors[LogLevel::Trace] = s_colorGray;
    m_levelColors[LogLevel::Debug] = s_colorCyan;
    m_levelColors[LogLevel::Info] = s_colorWhite;
    m_levelColors[LogLevel::Warn] = s_colorYellow;
    m_levelColors[LogLevel::Error] = s_colorRed;
    m_levelColors[LogLevel::Fatal] = s_colorWhite;

    // Fatal 级别使用红色背景
    m_levelBackgrounds[LogLevel::Fatal] = s_bgRed;

    // 启动异步写入线程
    if (m_async) {
        m_running.storeRelaxed(1);
        m_writerThread = QThread::create([this]() { writerThreadFunc(); });
        m_writerThread->start();
    }
}

ConsoleAppender::~ConsoleAppender() {
    close();
}

void ConsoleAppender::append(const LogRecord& record, const QString& formatted) {
    QString output = formatted;

    if (m_useColors) {
        output = colorize(formatted, record.level);
    }

    output += "\n";
    QByteArray utf8 = output.toUtf8();
    bool isError = (record.level >= LogLevel::Error);

    if (m_async) {
        // 异步写入：加入队列（带大小限制）
        QMutexLocker locker(&m_queueMutex);

        // 检查队列大小
        if (m_maxQueueSize > 0 && m_writeQueue.size() >= m_maxQueueSize) {
            // 队列已满，丢弃最旧的消息
            m_queueOverflow.storeRelaxed(1);
            m_writeQueue.dequeue();
        }

        // 使用特殊前缀标记错误输出
        QByteArray data = isError ? QByteArray("\x01") + utf8 : QByteArray("\x00") + utf8;
        m_writeQueue.enqueue(data);
        m_queueCondition.wakeOne();
    } else {
        // 同步写入
        QMutexLocker locker(&m_mutex);
        writeDirect(utf8, isError);
    }
}

void ConsoleAppender::flush() {
    if (m_async) {
        // 请求刷新
        m_flushRequested.storeRelaxed(1);
        m_queueCondition.wakeAll();

        // 等待队列清空（带超时保护）
        int timeout = 3000;  // 3秒超时
        int waited = 0;
        while (waited < timeout) {
            QMutexLocker locker(&m_queueMutex);
            if (m_writeQueue.isEmpty()) {
                break;
            }
            locker.unlock();
            QThread::msleep(10);
            waited += 10;
        }
    }

    QMutexLocker locker(&m_mutex);
    fflush(stdout);
    fflush(stderr);
}

void ConsoleAppender::close() {
    // 停止异步线程
    if (m_async && m_writerThread) {
        m_running.storeRelaxed(0);
        m_queueCondition.wakeAll();

        // 等待线程结束（带超时保护）
        if (!m_writerThread->wait(2000)) {
            m_writerThread->terminate();
            m_writerThread->wait();
        }
        delete m_writerThread;
        m_writerThread = nullptr;
    }

    flush();
}

void ConsoleAppender::setLevelColor(LogLevel level, const QString& colorCode) {
    m_levelColors[level] = colorCode;
}

void ConsoleAppender::setLevelBackground(LogLevel level, const QString& bgColorCode) {
    m_levelBackgrounds[level] = bgColorCode;
}

bool ConsoleAppender::supportsColors() {
#ifdef Q_OS_WIN
    // Windows 10+ 支持 ANSI 颜色
    static bool checked = false;
    static bool supported = false;

    if (!checked) {
        checked = true;

        // 启用 Windows 控制台的 ANSI 转义序列处理
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (SetConsoleMode(hOut, dwMode)) {
                    supported = true;
                }
            }
        }
    }

    return supported;
#else
    // Unix/Linux/macOS 默认支持颜色（检测 TERM 环境变量）
    QByteArray term = qgetenv("TERM");
    return !term.isEmpty() && term != "dumb";
#endif
}

QString ConsoleAppender::colorize(const QString& text, LogLevel level) const {
    QString result;

    // 添加背景色（如果有）
    if (m_levelBackgrounds.contains(level)) {
        result += m_levelBackgrounds[level];
    }

    // 添加前景色
    if (m_levelColors.contains(level)) {
        result += m_levelColors[level];
    }

    result += text;
    result += s_colorReset;

    return result;
}

void ConsoleAppender::writerThreadFunc() {
    while (m_running.loadRelaxed()) {
        QByteArray data;
        bool overflowed = false;

        {
            QMutexLocker locker(&m_queueMutex);

            // 等待新消息或刷新请求
            while (m_writeQueue.isEmpty() && m_running.loadRelaxed() && !m_flushRequested.loadRelaxed()) {
                m_queueCondition.wait(&m_queueMutex, 100);
            }

            if (!m_writeQueue.isEmpty()) {
                data = m_writeQueue.dequeue();
            }

            // 检查是否有溢出发生过
            if (m_queueOverflow.loadRelaxed()) {
                overflowed = true;
                m_queueOverflow.storeRelaxed(0);
            }
        }

        // 如果发生过溢出，添加警告消息
        if (overflowed) {
            writeDirect("[WARNING] Log queue overflow, some messages were dropped\n", false);
        }

        if (!data.isEmpty()) {
            // 检查第一个字节判断输出流
            bool isError = (data.at(0) == '\x01');
            writeDirect(data.mid(1), isError);
        }

        // 处理刷新请求
        if (m_flushRequested.loadRelaxed()) {
            QMutexLocker locker(&m_mutex);
            fflush(stdout);
            fflush(stderr);
            m_flushRequested.storeRelaxed(0);
        }
    }

    // 写入剩余消息
    while (true) {
        QByteArray data;
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_writeQueue.isEmpty()) {
                break;
            }
            data = m_writeQueue.dequeue();
        }

        if (!data.isEmpty()) {
            bool isError = (data.at(0) == '\x01');
            writeDirect(data.mid(1), isError);
        }
    }
}

void ConsoleAppender::writeDirect(const QByteArray& data, bool isError) {
    FILE* out = isError ? stderr : stdout;
    fprintf(out, "%s", data.constData());
    // 不每次 flush，减少系统调用
}

}  // namespace EasyKiConverter