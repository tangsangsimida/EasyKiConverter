#include "QtLogAdapter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QThreadStorage>

namespace EasyKiConverter {

QtMessageHandler QtLogAdapter::s_originalHandler = nullptr;
LogModule QtLogAdapter::s_defaultModule = LogModule::Core;
bool QtLogAdapter::s_preserveOriginal = false;
bool QtLogAdapter::s_installed = false;
QAtomicInt QtLogAdapter::s_inHandler(0);

// 线程本地存储，用于检测递归调用
static QThreadStorage<bool*> t_recursionGuard;

void QtLogAdapter::install() {
    if (s_installed) {
        return;
    }

    s_originalHandler = qInstallMessageHandler(qtMessageHandler);
    s_installed = true;
}

void QtLogAdapter::uninstall() {
    if (!s_installed) {
        return;
    }

    qInstallMessageHandler(s_originalHandler);
    s_originalHandler = nullptr;
    s_installed = false;
}

void QtLogAdapter::setDefaultModule(LogModule module) {
    s_defaultModule = module;
}

LogModule QtLogAdapter::defaultModule() {
    return s_defaultModule;
}

void QtLogAdapter::setPreserveOriginal(bool preserve) {
    s_preserveOriginal = preserve;
}

bool QtLogAdapter::preserveOriginal() {
    return s_preserveOriginal;
}

void QtLogAdapter::qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    // === 递归调用检测 ===
    // 检查是否已经在处理日志（防止死锁）
    if (s_inHandler.loadRelaxed()) {
        // 递归调用检测到，使用原始处理器或直接输出到 stderr
        if (s_originalHandler) {
            s_originalHandler(type, context, msg);
        } else {
            // 后备方案：直接输出到 stderr
            fprintf(stderr, "[RECURSIVE] %s\n", msg.toUtf8().constData());
        }
        return;
    }

    // 设置递归保护标志
    s_inHandler.storeRelaxed(1);

    // 使用 RAII 确保标志在退出时被清除
    struct RecursionGuardReset {
        ~RecursionGuardReset() {
            s_inHandler.storeRelaxed(0);
        }
    } guardReset;

    // 将 Qt 日志类型映射到我们的日志级别
    LogLevel level;
    switch (type) {
        case QtDebugMsg:
            level = LogLevel::Debug;
            break;
        case QtInfoMsg:
            level = LogLevel::Info;
            break;
        case QtWarningMsg:
            level = LogLevel::Warn;
            break;
        case QtCriticalMsg:
            level = LogLevel::Error;
            break;
        case QtFatalMsg:
            level = LogLevel::Fatal;
            break;
        default:
            level = LogLevel::Info;
            break;
    }

    // 安全地获取 Logger 实例并检查是否应该记录
    Logger* logger = Logger::instance();
    if (!logger) {
        return;
    }

    if (!logger->shouldLog(level, s_defaultModule)) {
        return;
    }

    // 构建日志消息（安全地处理字符串）
    QString message;

    // 避免格式化字符串攻击：不对用户消息进行额外格式化
    // 仅在消息中添加分类信息
    if (context.category && strlen(context.category) > 0) {
        QByteArray category = context.category;
        if (category != "default") {
            message = QString("[%1] %2").arg(QString::fromUtf8(category), msg);
        } else {
            message = msg;
        }
    } else {
        message = msg;
    }

    // 记录日志（使用安全的文件名和函数名）
    const char* safeFile = context.file ? context.file : "";
    const char* safeFunction = context.function ? context.function : "";
    int safeLine = context.line >= 0 ? context.line : 0;

    logger->log(level, s_defaultModule, message, safeFile, safeFunction, safeLine);

    // 如果需要保留原始行为
    if (s_preserveOriginal && s_originalHandler) {
        // 临时清除递归标志以允许原始处理器工作
        s_inHandler.storeRelaxed(0);
        s_originalHandler(type, context, msg);
        s_inHandler.storeRelaxed(1);
    }

    // Qt 的 qFatal 默认会终止程序，这里保持这个行为
    if (type == QtFatalMsg) {
        logger->flush();
        // 不调用 abort()，让程序正常退出
        // 递归保护会在函数退出时自动清除
    }
}

}  // namespace EasyKiConverter