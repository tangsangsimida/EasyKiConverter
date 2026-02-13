#ifndef QTLOGADAPTER_H
#define QTLOGADAPTER_H

#include "LogMacros.h"
#include "Logger.h"

#include <QtGlobal>

namespace EasyKiConverter {

/**
 * @brief Qt 日志适配器
 * 
 * 将 Qt 的 qDebug/qWarning/qCritical 等日志重定向到我们的日志系统。
 * 安装后，原有的 qDebug() 调用会自动转换为新日志系统。
 */
class QtLogAdapter {
public:
    /**
     * @brief 安装 Qt 日志处理器
     * 
     * 安装后，Qt 的日志消息会被重定向到我们的日志系统：
     * - qDebugMsg -> LOG_DEBUG(LogModule::Core, ...)
     * - qInfoMsg  -> LOG_INFO(LogModule::Core, ...)
     * - qWarningMsg -> LOG_WARN(LogModule::Core, ...)
     * - qCriticalMsg -> LOG_ERROR(LogModule::Core, ...)
     * - qFatalMsg -> LOG_FATAL(LogModule::Core, ...)
     */
    static void install();
    
    /**
     * @brief 卸载 Qt 日志处理器
     */
    static void uninstall();
    
    /**
     * @brief 设置 Qt 日志的默认模块
     */
    static void setDefaultModule(LogModule module);
    
    /**
     * @brief 获取默认模块
     */
    static LogModule defaultModule();
    
    /**
     * @brief 设置是否保留原始 Qt 日志行为（同时输出到原目标）
     */
    static void setPreserveOriginal(bool preserve);
    
    /**
     * @brief 是否保留原始 Qt 日志行为
     */
    static bool preserveOriginal();

private:
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);
    
    static QtMessageHandler s_originalHandler;
    static LogModule s_defaultModule;
    static bool s_preserveOriginal;
    static bool s_installed;
};

} // namespace EasyKiConverter

#endif // QTLOGADAPTER_H
