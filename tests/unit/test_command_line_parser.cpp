#include "utils/CommandLineParser.h"
#include "utils/cli/CompletionGenerator.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QStringList>
#include <QTest>

using namespace EasyKiConverter;

class TestCommandLineParser : public QObject {
    Q_OBJECT

private slots:
    // CLI 模式检测（核心修复：parse() 使用存储的 argv，不依赖 QCoreApplication::arguments()）
    void testParseStoredArgv();  // 验证 argc/argv 被正确存储和使用
    void testParseConvertBom();  // convert bom → CliMode::ConvertBom
    void testParseConvertComponent();  // convert component → CliMode::ConvertComponent
    void testParseConvertBatch();  // convert batch → CliMode::ConvertBatch
    void testParseConvertInvalid();  // convert abc → hasConvertCommand=true 但 isCliMode=false

    // --symbol / --footprint / --3d-model 默认语义
    void testExportSymbolDefaultTrue();  // 默认 true，未设置时不导出符号库
    void testExportFootprintDefaultTrue();  // 默认 true，未设置时不导出封装库
    void testExport3dModelDefaultFalse();  // 默认 false（opt-in），未设置时不导出 3D 模型
    void testExportSymbolExplicitTrue();  // --symbol=true 显式启用
    void testExport3dModelExplicitTrue();  // --3d-model 显式启用

    // 补全脚本契约（验证不再暴露不存在的命令）
    void testCompletionScriptNoHelpVersionSubcommands();
    void testCompletionScriptNoBoolValueHints();

    // hasConvertCommand() 辅助方法
    void testHasConvertCommand();

private:
    // Helper: 构建 argc/argv 并构造 CommandLineParser
    CommandLineParser* makeParser(const QStringList& args);

    int m_argc = 0;
    char** m_argv = nullptr;
    QList<QByteArray> m_argStorage;  // 存储字符串数据（生命周期与 parser 同长）
    QList<char*> m_argvPtrs;  // argv 指针数组（指向 m_argStorage）
};

CommandLineParser* TestCommandLineParser::makeParser(const QStringList& args) {
    // 清理上一次的 argv
    m_argStorage.clear();
    m_argc = args.size();

    // 将 QStringList 转换为 char* []（存储在 m_argStorage 中保证生命周期）
    for (const QString& arg : args) {
        m_argStorage.append(arg.toUtf8());
    }

    // 构建 argv 指针数组（指向 m_argStorage 中的数据，保证与 parser 生命周期一致）
    m_argvPtrs.clear();
    for (const QByteArray& a : m_argStorage) {
        m_argvPtrs.append(const_cast<char*>(a.constData()));
    }
    m_argv = m_argvPtrs.isEmpty() ? nullptr : m_argvPtrs.data();

    return new CommandLineParser(m_argc, m_argv);
}

// ========== CLI 模式检测 ==========

void TestCommandLineParser::testParseStoredArgv() {
    // 验证 parse() 使用存储的 argv 而非 QCoreApplication::arguments()
    // 构造一个带有明确标记的 argv，parse() 解析后检查状态
    QStringList args = {"easykiconverter", "convert", "bom", "-i", "input.xlsx", "-o", "output"};
    auto* parser = makeParser(args);

    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->hasConvertCommand());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertBom);

    delete parser;
}

void TestCommandLineParser::testParseConvertBom() {
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "bom.xlsx", "-o", "out"});
    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertBom);
    QVERIFY(parser->hasConvertCommand());
    delete parser;
}

void TestCommandLineParser::testParseConvertComponent() {
    auto* parser = makeParser({"easykiconverter", "convert", "component", "-c", "C12345", "-o", "out"});
    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertComponent);
    QVERIFY(parser->hasConvertCommand());
    delete parser;
}

void TestCommandLineParser::testParseConvertBatch() {
    auto* parser = makeParser({"easykiconverter", "convert", "batch", "-i", "list.txt", "-o", "out"});
    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertBatch);
    QVERIFY(parser->hasConvertCommand());
    delete parser;
}

void TestCommandLineParser::testParseConvertInvalid() {
    // 关键回归测试：convert 后面跟无效子命令，hasConvertCommand=true 但 isCliMode=false
    auto* parser = makeParser({"easykiconverter", "convert", "bmo", "-o", "out"});

    QVERIFY(parser->parse());
    QVERIFY(parser->hasConvertCommand());  // convert 命令存在
    QVERIFY2(!parser->isCliMode(), "convert 后跟无效子命令不应进入 CLI 模式");  // 但没有合法子命令

    delete parser;
}

// ========== 导出选项默认语义 ==========

void TestCommandLineParser::testExportSymbolDefaultTrue() {
    // --symbol 未传时默认为 true（核心产物默认开启）
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY(parser->validate());  // 有 output 就验证通过
    QVERIFY2(parser->exportSymbol() == true, "--symbol 未设置时默认导出符号库");
    delete parser;
}

void TestCommandLineParser::testExportFootprintDefaultTrue() {
    // --footprint 未传时默认为 true（核心产物默认开启）
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY2(parser->exportFootprint() == true, "--footprint 未设置时默认导出封装库");
    delete parser;
}

void TestCommandLineParser::testExport3dModelDefaultFalse() {
    // --3d-model 未传时默认为 false（opt-in，不默认导出 3D）
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY2(parser->export3DModel() == false, "--3d-model 未设置时不应导出 3D 模型（opt-in）");
    delete parser;
}

void TestCommandLineParser::testExportSymbolExplicitTrue() {
    // --symbol 显式设置 true
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--symbol"});
    parser->parse();
    QVERIFY(parser->exportSymbol() == true);
    delete parser;
}

void TestCommandLineParser::testExport3dModelExplicitTrue() {
    // --3d-model 显式启用
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-model"});
    parser->parse();
    QVERIFY(parser->export3DModel() == true);
    delete parser;
}

// ========== 补全脚本契约 ==========

void TestCommandLineParser::testCompletionScriptNoHelpVersionSubcommands() {
    // 验证补全脚本不再暴露 help/version 子命令（它们是 --help/--version 选项）
    QString bashScript = CompletionGenerator::generate(CompletionGenerator::Shell::Bash);
    QString zshScript = CompletionGenerator::generate(CompletionGenerator::Shell::Zsh);
    QString fishScript = CompletionGenerator::generate(CompletionGenerator::Shell::Fish);

    // Bash: subcommands 变量应只包含 "convert"
    QVERIFY2(bashScript.contains("local subcommands=\"convert\""), "Bash 补全的 subcommands 不应包含 help/version");

    // Zsh: commands 数组应只包含 convert
    QVERIFY2(!zshScript.contains("'help:显示帮助信息'"), "Zsh 补全不应暴露 help 子命令");
    QVERIFY2(!zshScript.contains("'version:显示版本信息'"), "Zsh 补全不应暴露 version 子命令");

    // Fish: 不应有 help/version 子命令补全
    QVERIFY2(!fishScript.contains("'__fish_use_subcommand' -a help"), "Fish 补全不应暴露 help 子命令");
    QVERIFY2(!fishScript.contains("'__fish_use_subcommand' -a version"), "Fish 补全不应暴露 version 子命令");
}

void TestCommandLineParser::testCompletionScriptNoBoolValueHints() {
    // 验证补全脚本不再为 --symbol/--footprint/--3d-model 补全 true/false
    QString bashScript = CompletionGenerator::generate(CompletionGenerator::Shell::Bash);
    QString zshScript = CompletionGenerator::generate(CompletionGenerator::Shell::Zsh);
    QString fishScript = CompletionGenerator::generate(CompletionGenerator::Shell::Fish);

    // Bash: --symbol|--footprint|--3d-model 后不应跟 compgen -W "true false"
    QVERIFY2(!bashScript.contains("compgen -W \"true false\""), "Bash 补全不应为 flag 选项提供 true/false 补全");

    // Zsh: --symbol/--footprint/--3d-model 的补全描述中不应有 :(true false)
    QVERIFY2(!zshScript.contains(":value:(true false)"), "Zsh 补全不应为 flag 选项提供 :(true false)");

    // Fish: -a 'true false' 不应出现在 symbol/footprint/3d-model 补全中
    QVERIFY2(!fishScript.contains("-a 'true false'"), "Fish 补全不应为 flag 选项提供 true/false 补全");
}

// ========== 辅助方法 ==========

void TestCommandLineParser::testHasConvertCommand() {
    // 有 convert 命令（无论子命令是否有效）
    auto* p1 = makeParser({"easykiconverter", "convert", "-o", "out"});
    QVERIFY(p1->parse());
    QVERIFY(p1->hasConvertCommand());
    delete p1;

    // 无 convert 命令
    auto* p2 = makeParser({"easykiconverter", "--version"});
    QVERIFY(p2->parse());
    QVERIFY2(!p2->hasConvertCommand(), "--version 不应设置 hasConvertCommand");
    delete p2;
}

QTEST_MAIN(TestCommandLineParser)
#include "test_command_line_parser.moc"