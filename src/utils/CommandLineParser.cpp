#include "CommandLineParser.h"

#include <QCoreApplication>
#include <QTextStream>

namespace EasyKiConverter {

CommandLineParser::CommandLineParser(int argc, char* argv[])
    : m_debugOption(QStringList() << "d"
                                  << "debug",
                    "启用调试模式（显示详细日志和控制台窗口）")
    , m_logLevelOption(QStringList() << "log-level",
                       "设置日志级别 (trace/debug/info/warn/error/fatal)",
                       "level",
                       "info")
    , m_logFileOption(QStringList() << "log-file", "指定日志文件路径", "path")
    , m_configOption(QStringList() << "config", "指定配置文件路径", "path")
    , m_languageOption(QStringList() << "language", "设置界面语言 (zh_CN/en)", "lang", "zh_CN")
    , m_themeOption(QStringList() << "theme", "设置界面主题 (dark/light)", "theme", "dark")
    , m_portableOption(QStringList() << "portable", "便携模式（配置文件保存在程序目录）") {
    m_parser.setApplicationDescription(
        QCoreApplication::translate("main", "EasyKiConverter - LCSC/EasyEDA 元件转 KiCad 库工具"));

    // 添加 Qt 内置的帮助和版本选项
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    // 添加自定义选项
    setupOptions();

    // 设置应用程序参数（用于帮助和版本信息）
    Q_UNUSED(argc);
    Q_UNUSED(argv);
}

void CommandLineParser::setupOptions() {
    // 添加所有自定义选项（帮助和版本选项已在构造函数中通过 addHelpOption 和 addVersionOption 添加）
    m_parser.addOption(m_debugOption);
    m_parser.addOption(m_logLevelOption);
    m_parser.addOption(m_logFileOption);
    m_parser.addOption(m_configOption);
    m_parser.addOption(m_languageOption);
    m_parser.addOption(m_themeOption);
    m_parser.addOption(m_portableOption);
}

bool CommandLineParser::parse() {
    return m_parser.parse(QCoreApplication::arguments());
}

bool CommandLineParser::isDebugMode() const {
    return m_parser.isSet(m_debugOption);
}

QString CommandLineParser::logLevel() const {
    return m_parser.value(m_logLevelOption).toLower();
}

QString CommandLineParser::logFile() const {
    return m_parser.value(m_logFileOption);
}

QString CommandLineParser::configFile() const {
    return m_parser.value(m_configOption);
}

QString CommandLineParser::language() const {
    return m_parser.value(m_languageOption);
}

QString CommandLineParser::theme() const {
    return m_parser.value(m_themeOption).toLower();
}

bool CommandLineParser::isPortableMode() const {
    return m_parser.isSet(m_portableOption);
}

QString CommandLineParser::helpText() const {
    return m_parser.helpText();
}

bool CommandLineParser::isHelpRequested() const {
    // QCommandLineParser 的 addHelpOption() 会添加一个帮助选项
    // 我们需要检查该选项是否被设置
    return m_parser.isSet("help");
}

bool CommandLineParser::isVersionRequested() const {
    // QCommandLineParser 的 addVersionOption() 会添加一个版本选项
    // 我们需要检查该选项是否被设置
    return m_parser.isSet("version");
}

QStringList CommandLineParser::positionalArguments() const {
    return m_parser.positionalArguments();
}

}  // namespace EasyKiConverter