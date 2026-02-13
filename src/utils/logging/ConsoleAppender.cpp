#include "ConsoleAppender.h"

#include <QByteArray>
#include <QCoreApplication>

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

ConsoleAppender::ConsoleAppender(bool useColors) : m_useColors(useColors && supportsColors()) {
    // 设置默认颜色
    m_levelColors[LogLevel::Trace] = s_colorGray;
    m_levelColors[LogLevel::Debug] = s_colorCyan;
    m_levelColors[LogLevel::Info] = s_colorWhite;
    m_levelColors[LogLevel::Warn] = s_colorYellow;
    m_levelColors[LogLevel::Error] = s_colorRed;
    m_levelColors[LogLevel::Fatal] = s_colorWhite;

    // Fatal 级别使用红色背景
    m_levelBackgrounds[LogLevel::Fatal] = s_bgRed;
}

void ConsoleAppender::append(const LogRecord& record, const QString& formatted) {
    QMutexLocker locker(&m_mutex);

    QString output = formatted;

    if (m_useColors) {
        output = colorize(formatted, record.level);
    }

    output += "\n";

    // 使用 fprintf 直接输出到控制台（更可靠）
    QByteArray utf8 = output.toUtf8();
    FILE* out = (record.level >= LogLevel::Error) ? stderr : stdout;
    fprintf(out, "%s", utf8.constData());
    fflush(out);
}

void ConsoleAppender::flush() {
    QMutexLocker locker(&m_mutex);
    fflush(stdout);
    fflush(stderr);
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

}  // namespace EasyKiConverter
