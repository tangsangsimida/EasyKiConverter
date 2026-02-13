#ifndef CONSOLEAPPENDER_H
#define CONSOLEAPPENDER_H

#include "IAppender.h"
#include "LogLevel.h"

#include <QHash>
#include <QMutex>
#include <QString>

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
 */
class ConsoleAppender : public IAppender {
public:
    /**
     * @brief 构造函数
     * @param useColors 是否启用彩色输出
     */
    explicit ConsoleAppender(bool useColors = true);
    
    /**
     * @brief 输出日志到控制台
     */
    void append(const LogRecord& record, const QString& formatted) override;
    
    /**
     * @brief 刷新输出流
     */
    void flush() override;
    
    /**
     * @brief 启用/禁用彩色输出
     */
    void setUseColors(bool useColors) { m_useColors = useColors; }
    
    /**
     * @brief 是否启用彩色输出
     */
    bool useColors() const { return m_useColors; }
    
    /**
     * @brief 设置特定级别的颜色
     */
    void setLevelColor(LogLevel level, const QString& colorCode);
    
    /**
     * @brief 设置特定级别的背景色（用于 Fatal）
     */
    void setLevelBackground(LogLevel level, const QString& bgColorCode);

private:
    bool m_useColors;
    QMutex m_mutex;
    
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
};

} // namespace EasyKiConverter

#endif // CONSOLEAPPENDER_H
