#include "CommandLineParser.h"

#include <QCoreApplication>
#include <QTextStream>

namespace EasyKiConverter {

CommandLineParser::CommandLineParser(int argc, char* argv[])
    : m_debugOption(QStringList() << "d" << "debug", "启用调试模式（显示详细日志和控制台窗口）")
    , m_logLevelOption(QStringList() << "log-level",
                       "设置日志级别 (trace/debug/info/warn/error/fatal)",
                       "level",
                       "info")
    , m_logFileOption(QStringList() << "log-file", "指定日志文件路径", "path")
    , m_configOption(QStringList() << "config", "指定配置文件路径", "path")
    , m_languageOption(QStringList() << "language", "设置界面语言 (zh_CN/en)", "lang", "zh_CN")
    , m_themeOption(QStringList() << "theme", "设置界面主题 (dark/light)", "theme", "dark")
    , m_portableOption(QStringList() << "portable", "便携模式（配置文件保存在程序目录）")
    , m_syncLoggingOption(QStringList() << "sync-logging", "启用同步控制台日志输出（确保彩色日志显示，方便调试）")
    , m_inputOption(QStringList() << "i" << "input", "输入文件路径 (BOM 表或元器件列表文件)", "path")
    , m_outputOption(QStringList() << "o" << "output", "输出目录路径", "path")
    , m_componentOption(QStringList() << "c" << "component", "LCSC 元器件编号", "id")
    , m_symbolOption("symbol", "导出符号库 (默认: true)")
    , m_footprintOption("footprint", "导出封装库 (默认: true)")
    , m_3dModelOption("3d-model", "导出 3D 模型")
    , m_previewOption("preview", "导出预览图")
    , m_progressOption("progress", "显示进度条")
    , m_quietOption(QStringList() << "q" << "quiet", "安静模式，减少输出")
    , m_completionOption("completion", "生成 Shell 补全脚本 (bash/zsh/fish)", "shell")
    , m_completeOption("complete", "内部选项：输出动态补全数据", "type") {
    m_parser.setApplicationDescription(
        QCoreApplication::translate("main", "EasyKiConverter - LCSC/EasyEDA 元件转 KiCad 库工具"));

    // Qt 内置的帮助和版本选项
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    // 自定义选项
    setupOptions();

    // CLI 模式选项
    setupCliOptions();

    // 补全选项
    m_parser.addOption(m_completionOption);
    m_parser.addOption(m_completeOption);

    // 设置应用程序参数（用于帮助和版本信息）
    // 重要：在 QCoreApplication 创建前存储参数，以确保 CLI 模式检测可靠
    m_argc = argc;
    m_argv = argv;
}

void CommandLineParser::setupOptions() {
    // 所有自定义选项（帮助和版本选项已在构造函数中通过 addHelpOption 和 addVersionOption 添加）
    m_parser.addOption(m_debugOption);
    m_parser.addOption(m_logLevelOption);
    m_parser.addOption(m_logFileOption);
    m_parser.addOption(m_configOption);
    m_parser.addOption(m_languageOption);
    m_parser.addOption(m_themeOption);
    m_parser.addOption(m_portableOption);
    m_parser.addOption(m_syncLoggingOption);
}

void CommandLineParser::setupCliOptions() {
    m_parser.addOption(m_inputOption);
    m_parser.addOption(m_outputOption);
    m_parser.addOption(m_componentOption);
    m_parser.addOption(m_symbolOption);
    m_parser.addOption(m_footprintOption);
    m_parser.addOption(m_3dModelOption);
    m_parser.addOption(m_previewOption);
    m_parser.addOption(m_progressOption);
    m_parser.addOption(m_quietOption);
}

bool CommandLineParser::parse() {
    // 重要：使用存储的 argv 而非 QCoreApplication::arguments()
    // 因为此函数可能在 QCoreApplication 创建前被调用
    QStringList args;
    if (m_argv) {
        args.reserve(m_argc);
        for (int i = 0; i < m_argc; ++i) {
            args.append(QString::fromLocal8Bit(m_argv[i]));
        }
    }

    bool result = m_parser.parse(args);

    if (result) {
        // 检测 CLI 子命令
        // 使用 args 而非 QCoreApplication::arguments() 保证可靠性

        // 检查是否包含 "convert" 命令
        for (int i = 1; i < args.size(); ++i) {
            QString arg = args[i].toLower();
            if (arg == "convert") {
                m_hasConvertCommand = true;
                // 检查下一个参数
                if (i + 1 < args.size()) {
                    QString subcommand = args[i + 1].toLower();
                    if (subcommand == "bom") {
                        m_hasBomSubcommand = true;
                        m_cliMode = CliMode::ConvertBom;
                    } else if (subcommand == "component") {
                        m_hasComponentSubcommand = true;
                        m_cliMode = CliMode::ConvertComponent;
                    } else if (subcommand == "batch") {
                        m_hasBatchSubcommand = true;
                        m_cliMode = CliMode::ConvertBatch;
                    }
                }
                break;
            }
        }
    }

    return result;
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

bool CommandLineParser::isThemeSet() const {
    return m_parser.isSet(m_themeOption);
}

bool CommandLineParser::isPortableMode() const {
    return m_parser.isSet(m_portableOption);
}

bool CommandLineParser::isSyncLogging() const {
    return m_parser.isSet(m_syncLoggingOption);
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

bool CommandLineParser::validate() const {
    // 验证日志级别
    if (m_parser.isSet(m_logLevelOption)) {
        QString level = logLevel();
        QStringList validLevels = {"trace", "debug", "info", "warn", "error", "fatal"};
        if (!validLevels.contains(level)) {
            return false;
        }
    }

    // 验证语言设置
    if (m_parser.isSet(m_languageOption)) {
        QString lang = language();
        QStringList validLangs = {"zh_CN", "en"};
        if (!validLangs.contains(lang)) {
            return false;
        }
    }

    // 验证主题设置
    if (m_parser.isSet(m_themeOption)) {
        QString theme = this->theme();
        QStringList validThemes = {"dark", "light"};
        if (!validThemes.contains(theme)) {
            return false;
        }
    }

    // CLI 模式验证
    if (isCliMode()) {
        // 验证输出目录
        if (!m_parser.isSet(m_outputOption)) {
            return false;
        }

        // BOM 转换需要输入文件
        if (m_cliMode == CliMode::ConvertBom && !m_parser.isSet(m_inputOption)) {
            return false;
        }

        // 单个元器件转换需要组件编号
        if (m_cliMode == CliMode::ConvertComponent && !m_parser.isSet(m_componentOption)) {
            return false;
        }

        // 批量转换需要输入文件
        if (m_cliMode == CliMode::ConvertBatch && !m_parser.isSet(m_inputOption)) {
            return false;
        }
    }

    return true;
}

QString CommandLineParser::validationError() const {
    QStringList errors;

    // 检查日志级别
    if (m_parser.isSet(m_logLevelOption)) {
        QString level = logLevel();
        QStringList validLevels = {"trace", "debug", "info", "warn", "error", "fatal"};
        if (!validLevels.contains(level)) {
            errors.append(QString("无效的日志级别: %1（有效值: %2）").arg(level).arg(validLevels.join(", ")));
        }
    }

    // 检查语言设置
    if (m_parser.isSet(m_languageOption)) {
        QString lang = language();
        QStringList validLangs = {"zh_CN", "en"};
        if (!validLangs.contains(lang)) {
            errors.append(QString("无效的语言设置: %1（有效值: %2）").arg(lang).arg(validLangs.join(", ")));
        }
    }

    // 检查主题设置
    if (m_parser.isSet(m_themeOption)) {
        QString theme = this->theme();
        QStringList validThemes = {"dark", "light"};
        if (!validThemes.contains(theme)) {
            errors.append(QString("无效的主题设置: %1（有效值: %2）").arg(theme).arg(validThemes.join(", ")));
        }
    }

    // CLI 模式验证错误
    if (isCliMode()) {
        if (!m_parser.isSet(m_outputOption)) {
            errors.append("CLI 模式必须指定输出目录 (-o/--output)");
        }

        if (m_cliMode == CliMode::ConvertBom && !m_parser.isSet(m_inputOption)) {
            errors.append("BOM 表转换必须指定输入文件 (-i/--input)");
        }

        if (m_cliMode == CliMode::ConvertComponent && !m_parser.isSet(m_componentOption)) {
            errors.append("单个元器件转换必须指定 LCSC 编号 (-c/--component)");
        }

        if (m_cliMode == CliMode::ConvertBatch && !m_parser.isSet(m_inputOption)) {
            errors.append("批量转换必须指定输入文件 (-i/--input)");
        }
    }

    return errors.join("\n");
}

// ========== CLI 模式相关方法实现 ==========

bool CommandLineParser::isCliMode() const {
    return m_cliMode != CliMode::None;
}

CommandLineParser::CliMode CommandLineParser::cliMode() const {
    return m_cliMode;
}

QString CommandLineParser::inputFile() const {
    return m_parser.value(m_inputOption);
}

QString CommandLineParser::outputDir() const {
    return m_parser.value(m_outputOption);
}

QString CommandLineParser::componentId() const {
    return m_parser.value(m_componentOption);
}

bool CommandLineParser::exportSymbol() const {
    // 默认为 true，除非显式设置为 false
    return !m_parser.isSet(m_symbolOption) || m_parser.value(m_symbolOption).toLower() != "false";
}

bool CommandLineParser::exportFootprint() const {
    // 默认为 true，除非显式设置为 false
    return !m_parser.isSet(m_footprintOption) || m_parser.value(m_footprintOption).toLower() != "false";
}

bool CommandLineParser::export3DModel() const {
    // --3d-model 是 flag 选项，默认 false（未设置则不导出 3D 模型）
    return m_parser.isSet(m_3dModelOption);
}

bool CommandLineParser::exportPreview() const {
    return m_parser.isSet(m_previewOption);
}

bool CommandLineParser::showProgress() const {
    return m_parser.isSet(m_progressOption);
}

bool CommandLineParser::isQuietMode() const {
    return m_parser.isSet(m_quietOption);
}

QString CommandLineParser::cliHelpText() const {
    QString help;
    QTextStream stream(&help);

    stream << "EasyKiConverter CLI 模式\n\n";
    stream << "用法:\n";
    stream << "  easykiconverter convert <子命令> [选项]\n\n";
    stream << "子命令:\n";
    stream << "  bom        转换 BOM 表文件\n";
    stream << "  component  转换单个元器件（通过 LCSC 编号）\n";
    stream << "  batch      批量转换元器件（通过元器件列表文件）\n\n";
    stream << "选项:\n";
    stream << "  -i, --input <path>      输入文件路径（BOM 表或元器件列表文件）\n";
    stream << "  -o, --output <path>     输出目录路径（必需）\n";
    stream << "  -c, --component <id>    LCSC 元器件编号\n";
    stream << "  --symbol                导出符号库（默认: true）\n";
    stream << "  --footprint             导出封装库（默认: true）\n";
    stream << "  --3d-model              导出 3D 模型\n";
    stream << "  --preview               导出预览图\n";
    stream << "  --progress              显示进度条\n";
    stream << "  -q, --quiet             安静模式，减少输出\n\n";
    stream << "示例:\n";
    stream << "  # 转换 BOM 表\n";
    stream << "  easykiconverter convert bom -i my_project.xlsx -o ./kicad_libs\n\n";
    stream << "  # 转换单个元器件\n";
    stream << "  easykiconverter convert component -c C12345 -o ./output\n\n";
    stream << "  # 批量转换\n";
    stream << "  easykiconverter convert batch -i components.txt -o ./output --3d-model\n";

    return help;
}

// ========== 补全相关方法实现 ==========

bool CommandLineParser::isCompletionRequested() const {
    return m_parser.isSet(m_completionOption);
}

QString CommandLineParser::completionShell() const {
    return m_parser.value(m_completionOption).toLower();
}

bool CommandLineParser::isCompleteRequested() const {
    return m_parser.isSet(m_completeOption);
}

QString CommandLineParser::completeType() const {
    return m_parser.value(m_completeOption).toLower();
}

bool CommandLineParser::hasConvertCommand() const {
    return m_hasConvertCommand;
}

}  // namespace EasyKiConverter