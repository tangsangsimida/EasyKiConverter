#ifndef LOGMACROS_H
#define LOGMACROS_H

#include "Logger.h"
#include "LogLevel.h"
#include "LogModule.h"

namespace EasyKiConverter {

/**
 * @brief 日志宏详细实现
 * 
 * 这些宏提供便捷的日志记录接口，自动捕获源码位置信息。
 */

// 基础日志宏实现
#define _LOG_IMPL(level, module, message) \
    EasyKiConverter::Logger::instance()->log( \
        level, module, message, __FILE__, __FUNCTION__, __LINE__)

// 带格式化的日志宏实现
#define _LOG_FORMAT_IMPL(level, module, ...) \
    EasyKiConverter::Logger::instance()->log( \
        level, module, EasyKiConverter::LogUtils::format(__VA_ARGS__), \
        __FILE__, __FUNCTION__, __LINE__)

} // namespace EasyKiConverter

//=============================================================================
// 公开日志宏
//=============================================================================

/**
 * @brief 追踪级别日志（详细执行流程）
 * @param module 日志模块
 * @param ... 消息或格式化字符串+参数
 */
#define LOG_TRACE(module, ...) \
    do { \
        if (EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Trace, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Trace, module, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 调试级别日志
 * @param module 日志模块
 * @param ... 消息或格式化字符串+参数
 */
#define LOG_DEBUG(module, ...) \
    do { \
        if (EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Debug, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Debug, module, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 信息级别日志
 * @param module 日志模块
 * @param ... 消息或格式化字符串+参数
 */
#define LOG_INFO(module, ...) \
    do { \
        if (EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Info, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Info, module, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 警告级别日志
 * @param module 日志模块
 * @param ... 消息或格式化字符串+参数
 */
#define LOG_WARN(module, ...) \
    do { \
        if (EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Warn, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Warn, module, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 错误级别日志
 * @param module 日志模块
 * @param ... 消息或格式化字符串+参数
 */
#define LOG_ERROR(module, ...) \
    do { \
        if (EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Error, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Error, module, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 致命错误级别日志
 * @param module 日志模块
 * @param ... 消息或格式化字符串+参数
 */
#define LOG_FATAL(module, ...) \
    do { \
        if (EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Fatal, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Fatal, module, __VA_ARGS__); \
        } \
    } while(0)

//=============================================================================
// 条件日志宏
//=============================================================================

/**
 * @brief 条件调试日志（仅在条件为真时记录）
 */
#define LOG_DEBUG_IF(module, condition, ...) \
    do { \
        if ((condition) && EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Debug, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Debug, module, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 条件信息日志
 */
#define LOG_INFO_IF(module, condition, ...) \
    do { \
        if ((condition) && EasyKiConverter::Logger::instance()->shouldLog( \
            EasyKiConverter::LogLevel::Info, module)) { \
            _LOG_FORMAT_IMPL(EasyKiConverter::LogLevel::Info, module, __VA_ARGS__); \
        } \
    } while(0)

//=============================================================================
// 性能追踪宏
//=============================================================================

/**
 * @brief 作用域追踪（自动记录函数/代码块执行时间）
 * 
 * 示例：
 * void myFunction() {
 *     LOG_TRACE_SCOPE(LogModule::Network, "myFunction");
 *     // ... 函数体 ...
 *     // 函数退出时自动打印耗时
 * }
 */
#define LOG_TRACE_SCOPE(module, operation) \
    EasyKiConverter::ScopedTracer _scopedTracer_##__LINE__(module, operation)

/**
 * @brief 时间日志记录器（可记录多个检查点）
 * 
 * 示例：
 * void processData() {
 *     LOG_TIME(LogModule::Export, "processData");
 *     step1();
 *     _timeLogger.checkpoint("step1完成");
 *     step2();
 *     _timeLogger.checkpoint("step2完成");
 *     // 析构时自动打印总耗时
 * }
 */
#define LOG_TIME(module, operation) \
    EasyKiConverter::TimeLogger _timeLogger_##__LINE__(module, operation)

//=============================================================================
// 便捷宏
//=============================================================================

/**
 * @brief 获取 Logger 实例
 */
#define LOG_INSTANCE() EasyKiConverter::Logger::instance()

/**
 * @brief 设置全局日志级别
 */
#define LOG_SET_LEVEL(level) EasyKiConverter::Logger::instance()->setGlobalLevel(level)

/**
 * @brief 设置模块日志级别
 */
#define LOG_SET_MODULE_LEVEL(module, level) \
    EasyKiConverter::Logger::instance()->setModuleLevel(module, level)

/**
 * @brief 刷新日志
 */
#define LOG_FLUSH() EasyKiConverter::Logger::instance()->flush()

#endif // LOGMACROS_H
