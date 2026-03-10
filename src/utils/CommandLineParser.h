#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 命令行参数解析器
 *
 * 提供命令行参数解析功能，支持以下参数：
 * - --debug, -d: 启用调试模式
 * - --log-level: 设置日志级别 (trace/debug/info/warn/error/fatal)
 * - --log-file: 指定日志文件路径
 * - --config: 指定配置文件路径
 * - --language: 设置语言 (zh_CN/en)
 * - --theme: 设置主题 (dark/light)
 * - --help, -h: 显示帮助信息
 * - --version, -v: 显示版本信息
 * - --portable: 便携模式（配置文件保存在程序目录）
 */
class CommandLineParser {
public:
    /**
     * @brief 构造函数
     * @param argc 参数数量
     * @param argv 参数列表
     */
    CommandLineParser(int argc, char* argv[]);

    /**
     * @brief 解析命令行参数
     * @return 解析成功返回 true，否则返回 false
     */
    bool parse();

    /**
     * @brief 是否启用调试模式
     * @return 启用返回 true，否则返回 false
     */
    bool isDebugMode() const;

    /**
     * @brief 获取日志级别
     * @return 日志级别字符串
     */
    QString logLevel() const;

    /**
     * @brief 获取日志文件路径
     * @return 日志文件路径
     */
    QString logFile() const;

    /**
     * @brief 获取配置文件路径
     * @return 配置文件路径
     */
    QString configFile() const;

    /**
     * @brief 获取语言设置
     * @return 语言代码 (zh_CN/en)
     */
    QString language() const;

    /**
     * @brief 获取主题设置
     * @return 主题名称 (dark/light)
     */
    QString theme() const;

    /**
     * @brief 是否为便携模式
     * @return 便携模式返回 true，否则返回 false
     */
    bool isPortableMode() const;

    /**
     * @brief 获取帮助文本
     * @return 帮助文本
     */
    QString helpText() const;

    /**
     * @brief 是否显示帮助信息
     * @return 显示返回 true，否则返回 false
     */
    bool isHelpRequested() const;

    /**
     * @brief 是否显示版本信息
     * @return 显示返回 true，否则返回 false
     */
    bool isVersionRequested() const;

    /**
     * @brief 获取剩余的未解析参数
     * @return 未解析参数列表
     */
    QStringList positionalArguments() const;

private:
    void setupOptions();

    QCommandLineParser m_parser;
    QCommandLineOption m_debugOption;
    QCommandLineOption m_logLevelOption;
    QCommandLineOption m_logFileOption;
    QCommandLineOption m_configOption;
    QCommandLineOption m_languageOption;
    QCommandLineOption m_themeOption;
    QCommandLineOption m_portableOption;
};

}  // namespace EasyKiConverter

#endif  // COMMANDLINEPARSER_H