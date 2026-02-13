#ifndef LOG_H
#define LOG_H

/**
 * @file Log.h
 * @brief EasyKiConverter 日志系统统一入口
 * 
 * 这是日志系统的主入口头文件，包含了所有必要的组件。
 * 
 * 使用方法：
 * @code
 * #include "utils/logging/Log.h"
 * 
 * // 在 main.cpp 中初始化
 * void setupLogging() {
 *     using namespace EasyKiConverter;
 *     
 *     // 设置全局日志级别
 *     Logger::instance()->setGlobalLevel(LogLevel::Debug);
 *     
 *     // 添加控制台输出
 *     auto console = QSharedPointer<ConsoleAppender>::create(true);
 *     console->setFormatter(QSharedPointer<PatternFormatter>::create());
 *     Logger::instance()->addAppender(console);
 *     
 *     // 添加文件输出
 *     auto file = QSharedPointer<FileAppender>::create("logs/app.log");
 *     Logger::instance()->addAppender(file);
 *     
 *     // 安装 Qt 日志适配器
 *     QtLogAdapter::install();
 * }
 * 
 * // 在代码中使用
 * void myFunction() {
 *     LOG_INFO(LogModule::Network, "Connecting to {}", host);
 *     LOG_DEBUG(LogModule::Network, "Connection established");
 *     
 *     if (error) {
 *         LOG_ERROR(LogModule::Network, "Failed to connect: {}", errorMsg);
 *     }
 * }
 * @endcode
 */

// 核心类型
#include "LogLevel.h"
#include "LogModule.h"
#include "LogRecord.h"

// 格式化器
#include "IFormatter.h"
#include "PatternFormatter.h"

// 输出器
#include "ConsoleAppender.h"
#include "FileAppender.h"
#include "IAppender.h"

// 日志管理器
#include "Logger.h"

// 日志宏
#include "LogMacros.h"

// Qt 日志适配器
#include "QtLogAdapter.h"

#endif  // LOG_H
