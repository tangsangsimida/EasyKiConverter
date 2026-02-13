#include "QtLogAdapter.h"

#include <QByteArray>
#include <QCoreApplication>

namespace EasyKiConverter {

QtMessageHandler QtLogAdapter::s_originalHandler = nullptr;
LogModule QtLogAdapter::s_defaultModule = LogModule::Core;
bool QtLogAdapter::s_preserveOriginal = false;
bool QtLogAdapter::s_installed = false;

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

    // 检查是否应该记录
    if (!Logger::instance()->shouldLog(level, s_defaultModule)) {
        return;
    }

    // 构建日志消息
    QString message = msg;

    // 如果有分类信息，添加到消息中
    if (context.category && strlen(context.category) > 0) {
        QByteArray category = context.category;
        if (category != "default") {
            message = QString("[%1] %2").arg(QString::fromUtf8(category), msg);
        }
    }

    // 记录日志
    Logger::instance()->log(level, s_defaultModule, message, context.file, context.function, context.line);

    // 如果需要保留原始行为
    if (s_preserveOriginal && s_originalHandler) {
        s_originalHandler(type, context, msg);
    }

    // Qt 的 qFatal 默认会终止程序，这里保持这个行为
    if (type == QtFatalMsg) {
        Logger::instance()->flush();
        // 不调用 abort()，让程序正常退出
    }
}

}  // namespace EasyKiConverter
