// 我发现我在学校，不谈恋爱也不学习，不打游戏也不逃课，
// 每天就是处于认真听课，然后因为听不懂走神了，
// 反应过来以后再继续认真听课的循环，进入这种状态的时候，
// 我一般就是左脑和右脑已经聊美了，完全就是一个放飞自我，
// 意淫我有多少多少家产，然后怕班里有人会读心术紧急撤回一条意淫，
// 那这在恋爱小说里我不就是那种…背景板吗，
// 就每天在学校挂机任务就完成的npc
#include "core/LanguageManager.h"
#include "core/network/NetworkClient.h"
#include "services/ComponentCacheService.h"
#include "services/ConfigService.h"
#include "services/UpdateCheckerService.h"
#include "services/export/ExportProgress.h"
#include "services/export/ParallelExportService.h"
#include "ui/viewmodels/ComponentListViewModel.h"
#include "ui/viewmodels/ExportProgressViewModel.h"
#include "ui/viewmodels/ExportSettingsViewModel.h"
#include "ui/viewmodels/ThemeSettingsViewModel.h"
#include "utils/CommandLineParser.h"
#include "utils/cli/CliConverter.h"
#include "utils/cli/CompletionGenerator.h"
#include "utils/logging/Log.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPoint>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSize>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#ifdef _WIN32
#    include <conio.h>
#    include <fcntl.h>
#    include <io.h>
#    include <windows.h>
#endif

namespace {

bool isDebugMode(const EasyKiConverter::CommandLineParser& parser) {
    // 优先检查命令行参数
    if (parser.isDebugMode()) {
        return true;
    }

    // 检查环境变量（向后兼容）
    if (qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE")) {
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        return (debugValue == "true" || debugValue == "1" || debugValue == "yes");
    }
    return false;
}

#ifdef _WIN32
bool reopenConsoleStream(FILE* stream, const char* device, const char* mode, const char* streamName) {
    FILE* reopened = nullptr;
    errno_t err = freopen_s(&reopened, device, mode, stream);
    if (err != 0 || reopened == nullptr) {
        qCritical() << "Failed to reopen stream" << streamName << "to" << device << "error:" << err;
        return false;
    }
    return true;
}
#endif

void setupLogging(bool debugMode, const QString& logLevelStr, const QString& logFilePath, bool syncLogging) {
    using namespace EasyKiConverter;

#ifdef _WIN32
    // 如果启用调试模式，动态分配控制台（仅对 GUI 应用有效）
    if (debugMode) {
        // 尝试分配控制台
        if (AllocConsole()) {
            // 重定向标准输出
            (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");
            (void)reopenConsoleStream(stderr, "CONOUT$", "w", "stderr");
            (void)reopenConsoleStream(stdin, "CONIN$", "r", "stdin");

            // 设置控制台标题
            SetConsoleTitleA("EasyKiConverter - Debug Console");

            // 启用 ANSI 转义序列支持（彩色输出）
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD mode = 0;
                if (GetConsoleMode(hOut, &mode)) {
                    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    SetConsoleMode(hOut, mode);
                }
            }
        }
    }
#endif

    auto logger = Logger::instance();

    // 解析日志级别

    LogLevel logLevel = LogLevel::Info;

    if (debugMode) {
        logLevel = LogLevel::Debug;

    } else if (!logLevelStr.isEmpty()) {
        if (logLevelStr == "trace") {
            logLevel = LogLevel::Trace;

        } else if (logLevelStr == "debug") {
            logLevel = LogLevel::Debug;

        } else if (logLevelStr == "info") {
            logLevel = LogLevel::Info;

        } else if (logLevelStr == "warn") {
            logLevel = LogLevel::Warn;

        } else if (logLevelStr == "error") {
            logLevel = LogLevel::Error;

        } else if (logLevelStr == "fatal") {
            logLevel = LogLevel::Fatal;
        }
    }

    logger->setGlobalLevel(logLevel);

    // 控制台输出（彩色）
    // 默认使用异步模式（更好的性能），--sync-logging 参数可启用同步模式
    auto consoleAppender = QSharedPointer<ConsoleAppender>::create(true, !syncLogging);

    consoleAppender->setFormatter(QSharedPointer<PatternFormatter>::create(PatternFormatter::simplePattern()));

    logger->addAppender(consoleAppender);

    // 文件输出（仅在调试模式或明确指定日志文件路径时启用）

    if (debugMode || !logFilePath.isEmpty()) {
        QString logPath;

        if (!logFilePath.isEmpty()) {
            logPath = logFilePath;

        } else {
            // 在调试模式下，将日志文件保存到 debug 文件夹
            QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/debug";
            QDir().mkpath(logDir);

            // 使用时间戳创建日志文件名
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
            logPath = logDir + QString("/easykiconverter_debug_%1.log").arg(timestamp);
        }

        auto fileAppender =

            QSharedPointer<FileAppender>::create(logPath, 10 * 1024 * 1024, 5, true);  // 10MB, 5 files, async

        fileAppender->setFormatter(QSharedPointer<PatternFormatter>::create(PatternFormatter::simplePattern()));

        logger->addAppender(fileAppender);

        LOG_INFO(LogModule::Core,
                 "日志系统已初始化 - 级别: {}, 日志文件: {}",
                 logLevelStr.isEmpty() ? "default" : logLevelStr,
                 logPath);

    } else {
        LOG_INFO(LogModule::Core,
                 "日志系统已初始化 - 级别: {} (仅控制台输出)",
                 logLevelStr.isEmpty() ? "default" : logLevelStr);
    }

    // 安装 Qt 日志适配器（将 qDebug/qWarning/qCritical 重定向到新系统）
    QtLogAdapter::install();
}

QString normalizeMountedAppDir(QString appDir) {
    if (appDir.endsWith("/usr/bin")) {
        return appDir.chopped(8);  // 去掉 /usr/bin
    }
    if (appDir.endsWith("/bin")) {
        return appDir.chopped(4);  // 去掉 /bin
    }
    return appDir;
}

QString resolveRuntimeAppDir(const QString& appImagePath) {
    if (!appImagePath.isEmpty()) {
        return QFileInfo(appImagePath).absolutePath();
    }
    return QCoreApplication::applicationDirPath();
}

QString resolveProjectRootFromAppDir(QString appDir) {
    appDir = normalizeMountedAppDir(std::move(appDir));
    if (appDir.endsWith("/build/bin")) {
        return appDir.chopped(10);  // 去掉 /build/bin
    }
    if (appDir.endsWith("/build")) {
        return appDir.chopped(6);  // 去掉 /build
    }
    if (appDir.endsWith("/bin")) {
        return appDir.chopped(4);  // 去掉 /bin
    }
    return appDir;
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);
    QQuickWindow::setDefaultAlphaBuffer(true);

#ifdef _WIN32
    // Windows 透明无边框窗口在 Vulkan 后端下可能出现圆角透明区域被合成为黑色。
    // Direct3D 11 是 Qt Quick 在 Windows 上更稳定的默认路径。
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    qInfo() << "已将 Windows 渲染后端设置为 Direct3D 11，并启用 alpha buffer";
#else
    // 非 Windows 平台保留 Vulkan 优先策略。
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    qInfo() << "已将渲染后端优先设置为 Vulkan";
#endif

    // 设置 stdout 为无缓冲模式，确保日志颜色正常显示
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    // 在 QApplication 构造函数之前检查命令行参数
    bool showHelp = false;
    bool showVersion = false;
    bool cliModeRequested = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        // 支持 Unix 风格（-h, --help）和 Windows 风格（/h, /help, /?, ?）的参数
        if (arg == "--help" || arg == "-h" || arg == "/h" || arg == "/help" || arg == "/?" || arg == "?") {
            showHelp = true;
        } else if (arg == "--version" || arg == "-v" || arg == "/v" || arg == "/version") {
            showVersion = true;
        }
    }

    // 创建命令行参数解析器（在 QApplication 之前，以支持纯 CLI 模式）
    EasyKiConverter::CommandLineParser cmdParser(argc, argv);

    // 早期解析：检查是否为纯 CLI 模式（不需要 GUI）
    bool parsed = cmdParser.parse();
    bool isCompletionMode = parsed && cmdParser.isCompletionRequested();
    bool isCompleteMode = parsed && cmdParser.isCompleteRequested();
    bool isCliMode = parsed && cmdParser.isCliMode();
    bool isConvertCommand = parsed && cmdParser.hasConvertCommand();

    // 纯 CLI 模式：--completion, --complete, --help, --version, convert 子命令
    // 这些模式不需要 QApplication，只需要 QCoreApplication
    cliModeRequested = isCompletionMode || isCompleteMode || showHelp || showVersion || isCliMode || isConvertCommand;

    if (cliModeRequested) {
        // 纯 CLI 模式：使用 QCoreApplication
        QCoreApplication app(argc, argv);
        app.setApplicationName("EasyKiConverter");
        app.setApplicationVersion("3.1.5");
        app.setOrganizationName("EasyKiConverter");
        app.setOrganizationDomain("easykiconverter.com");

        if (showHelp) {
            QString helpText = cmdParser.helpText();
            QTextStream consoleOut(stdout);
            consoleOut << helpText;
            return 0;
        }

        if (showVersion) {
            QString versionText = "EasyKiConverter " + app.applicationVersion() + "\n";
            QTextStream consoleOut(stdout);
            consoleOut << versionText;
            return 0;
        }

        if (!parsed) {
            QTextStream err(stderr);
            err << "错误: 无效的命令行参数\n";
            err << cmdParser.helpText();
            return 1;
        }

        if (!cmdParser.validate()) {
            QTextStream err(stderr);
            err << "错误: 参数值无效\n";
            err << cmdParser.validationError() << "\n\n";
            err << cmdParser.helpText();
            return 1;
        }

        // 处理补全请求
        if (isCompletionMode) {
            QString shell = cmdParser.completionShell();
            auto shellType = EasyKiConverter::CompletionGenerator::parseShell(shell);
            QTextStream out(stdout);
            out << EasyKiConverter::CompletionGenerator::generate(shellType);
            return 0;
        }

        // 处理动态补全请求
        if (isCompleteMode) {
            QString completeType = cmdParser.completeType();
            QTextStream out(stdout);

            if (completeType == "lcsc-id") {
                // 输出缓存中的 LCSC ID 列表
                auto* cache = EasyKiConverter::ComponentCacheService::instance();
                if (cache) {
                    QStringList ids = cache->getCachedComponentIds();
                    for (const QString& id : ids) {
                        out << id << "\n";
                    }
                }
            }

            return 0;
        }

        // CLI 转换模式
        if (isCliMode) {
#ifdef _WIN32
            // 尝试附加到父进程的控制台
            if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                AllocConsole();
                SetConsoleTitleA("EasyKiConverter - CLI");
            }
            (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");
            (void)reopenConsoleStream(stderr, "CONOUT$", "w", "stderr");
#endif
            // 初始化日志系统（CLI 模式也需要日志）
            bool debugMode = isDebugMode(cmdParser);
            setupLogging(debugMode, cmdParser.logLevel(), cmdParser.logFile(), cmdParser.isSyncLogging());

            // 创建并执行 CLI 转换器
            EasyKiConverter::CliConverter cliConverter(cmdParser);
            bool success = cliConverter.execute();

            if (!success) {
                QTextStream err(stderr);
                err << "错误: " << cliConverter.errorMessage() << "\n";
            }

            // 清理日志
            EasyKiConverter::QtLogAdapter::uninstall();
            auto* logger = EasyKiConverter::Logger::instance();
            if (logger) {
                logger->flush();
                logger->close();
            }

            return success ? 0 : 1;
        }

        // convert 命令存在但无效子命令（如 easykiconverter convert abc）
        if (isConvertCommand && !isCliMode) {
            QTextStream err(stderr);
            err << "错误: 无效的 convert 子命令\n";
            err << "有效的子命令: bom, component, batch\n\n";
            err << cmdParser.cliHelpText();
            return 1;
        }

        // 到达此处的唯一可能是 isConvertCommand 但 isCliMode=false（无效子命令）
        // 该路径已在上面返回 1，此处不会执行到此
        // 防御性断言：确保所有 CLI 模式请求都在上面正确处理
        Q_ASSERT_X(false, "main", "Unhandled CLI mode path");
        return 1;
    }

    // GUI 模式：使用完整的 QApplication
    // 设置 QML 样式为 Basic，以消除原生样式自定义警告
    QQuickStyle::setStyle("Basic");

    QApplication app(argc, argv);

    // 自动检测并记录实际生效的渲染后端
    QTimer::singleShot(0, []() {
        QQuickWindow tempWindow;
        const char* apiName = "Unknown";
        switch (tempWindow.graphicsApi()) {
            case QSGRendererInterface::Unknown:
                apiName = "Unknown";
                break;
            case QSGRendererInterface::Software:
                apiName = "Software";
                break;
            case QSGRendererInterface::OpenVG:
                apiName = "OpenVG";
                break;
            case QSGRendererInterface::OpenGL:
                apiName = "OpenGL";
                break;
            case QSGRendererInterface::Direct3D11:
                apiName = "Direct3D 11";
                break;
            case QSGRendererInterface::Vulkan:
                apiName = "Vulkan";
                break;
            case QSGRendererInterface::Metal:
                apiName = "Metal";
                break;
            case QSGRendererInterface::Null:
                apiName = "Null (headless)";
                break;
            case QSGRendererInterface::Direct3D12:
                apiName = "Direct3D 12 (DX12)";
                break;
            default:
                apiName = "Other/Unknown";
                break;
        }
        qInfo() << "当前实际运行的渲染 API:" << apiName;
    });

    // 设置应用程序信息
    app.setApplicationName("EasyKiConverter");
    app.setApplicationVersion("3.1.5");
    app.setOrganizationName("EasyKiConverter");
    app.setOrganizationDomain("easykiconverter.com");

    // 注意：所有 CLI 模式处理（--help, --version, --completion, --complete, convert 子命令）
    // 已在 QCoreApplication 阶段处理完毕。如果到达此处，说明是纯 GUI 启动。
    // 命令行参数已在 CLI 路径解析并验证，无需重复解析。

    // 检查调试模式（命令行参数优先，环境变量向后兼容）
    bool debugMode = isDebugMode(cmdParser);

    // 初始化日志系统（在 QApplication 创建后立即初始化）
    setupLogging(debugMode, cmdParser.logLevel(), cmdParser.logFile(), cmdParser.isSyncLogging());

    // 尝试设置应用程序图标（使用 QGuiApplication::setWindowIcon 确保可靠性）
    QString appImagePath = qgetenv("APPIMAGE");
    QString appDir = resolveRuntimeAppDir(appImagePath);

    qDebug() << "应用目录:" << appDir;

    // 处理命令行参数 - 配置文件路径
    QString configFilePath;
    if (!cmdParser.configFile().isEmpty()) {
        configFilePath = cmdParser.configFile();
        qDebug() << "使用命令行指定的配置文件:" << configFilePath;
    } else if (cmdParser.isPortableMode()) {
        // 便携模式：配置文件保存在程序目录
        QString appDir = QCoreApplication::applicationDirPath();
        configFilePath = appDir + "/easykiconverter_config.json";
        qDebug() << "便携模式：配置文件保存在程序目录:" << configFilePath;
    }

    // 初始化配置服务（必须在加载图标之前，以便获取主题设置）
    EasyKiConverter::ConfigService::instance()->loadConfig(configFilePath);

    // 处理命令行参数 - 调试模式设置（优先于环境变量）
    // 调试模式只通过命令行参数和环境变量控制，不会保存到配置文件
    if (cmdParser.isDebugMode()) {
        auto* configService = EasyKiConverter::ConfigService::instance();
        // 设置内存中的调试模式（不保存到配置文件）
        configService->setDebugMode(true, false);
        qDebug() << "通过命令行参数启用调试模式（仅当前会话有效）";
    }

    // 处理命令行参数 - 语言设置
    if (!cmdParser.language().isEmpty()) {
        auto* langManager = EasyKiConverter::LanguageManager::instance();
        QString lang = cmdParser.language();
        if (lang == "zh_CN" || lang == "en") {
            langManager->setLanguage(lang);
            qDebug() << "通过命令行设置语言为:" << lang;
        } else {
            qWarning() << "无效的语言设置:" << lang << "，支持的选项: zh_CN, en";
        }
    } else {
        // 初始化语言管理器（使用默认语言）
        EasyKiConverter::LanguageManager::instance();
    }

    // 处理命令行参数 - 主题设置（仅在用户显式指定时应用）
    if (cmdParser.isThemeSet()) {
        QString theme = cmdParser.theme();
        auto* configService = EasyKiConverter::ConfigService::instance();
        if (theme == "dark") {
            configService->setDarkMode(true);
            qDebug() << "通过命令行设置主题为: dark";
        } else if (theme == "light") {
            configService->setDarkMode(false);
            qDebug() << "通过命令行设置主题为: light";
        } else {
            qWarning() << "无效的主题设置:" << theme << "，支持的选项: dark, light";
        }
    }

    // 根据主题设置确定图标目录
    bool isDarkMode = EasyKiConverter::ConfigService::instance()->getDarkMode();
    QString iconThemeDir = isDarkMode ? "hicolor-dark" : "hicolor";
    bool isAppImage = !appImagePath.isEmpty();
    qDebug() << "主题模式:" << (isDarkMode ? "dark" : "light") << ", 图标目录:" << iconThemeDir;
    qDebug() << "AppImage 模式:" << isAppImage;

    // 加载应用图标（根据主题选择不同的图标目录）
    QStringList iconPaths;

    if (isAppImage) {
        // AppImage: 优先使用内部图标路径
        iconPaths = {
            appDir + "/io.github.tangsangsimida.easykiconverter.png",
            appDir + "/usr/share/icons/" + iconThemeDir + "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            appDir + "/usr/share/icons/hicolor/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            appDir + "/resources/icons/" + iconThemeDir + "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            ":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/" + iconThemeDir +
                "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
        };
    } else {
        // 非AppImage: 优先使用资源路径和开发路径
        iconPaths = {
            ":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/" + iconThemeDir +
                "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            ":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png",
            ":/resources/icons/" + iconThemeDir + "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            ":/resources/icons/app_icon.png",
            appDir + "/resources/icons/" + iconThemeDir + "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            appDir + "/resources/icons/app_icon.png",
            appDir + "/io.github.tangsangsimida.easykiconverter.png",
            appDir + "/usr/share/icons/" + iconThemeDir + "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            appDir + "/usr/share/icons/hicolor/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
        };
    }

    for (const QString& iconPath : iconPaths) {
        qDebug() << "尝试加载图标:" << iconPath << "存在:" << QFile::exists(iconPath);
        QIcon icon(iconPath);
        if (!icon.isNull()) {
            QGuiApplication::setWindowIcon(icon);
            app.setWindowIcon(icon);
            qDebug() << "应用程序图标已设置：" << iconPath;
            break;
        }
    }

    // 创建 Service 实例（使用流水线架构，不设置 parent，手动管理生命周期）
    // 预先初始化缓存服务，避免首次添加组件时阻塞UI
    (void)EasyKiConverter::ComponentCacheService::instance();
    // 预先初始化统一网络客户端，避免首次请求时的冷启动抖动
    (void)EasyKiConverter::NetworkClient::instance();
    EasyKiConverter::ComponentService* componentService = new EasyKiConverter::ComponentService();
    EasyKiConverter::ParallelExportService* exportService = new EasyKiConverter::ParallelExportService();
    exportService->setComponentService(componentService);

    // 注册自定义类型以支持跨线程信号槽的 QueuedConnection
    qRegisterMetaType<EasyKiConverter::ExportItemStatus>();
    qRegisterMetaType<EasyKiConverter::ExportTypeProgress>();
    qRegisterMetaType<EasyKiConverter::PreloadProgress>();
    qRegisterMetaType<EasyKiConverter::ExportOverallProgress>();

    // 创建 ViewModel 实例（不设置 parent，手动管理生命周期）
    EasyKiConverter::ComponentListViewModel* componentListViewModel =
        new EasyKiConverter::ComponentListViewModel(componentService);
    EasyKiConverter::ExportSettingsViewModel* exportSettingsViewModel =
        new EasyKiConverter::ExportSettingsViewModel(exportService);
    EasyKiConverter::ExportProgressViewModel* exportProgressViewModel =
        new EasyKiConverter::ExportProgressViewModel(exportService, componentService, componentListViewModel);
    EasyKiConverter::ThemeSettingsViewModel* themeSettingsViewModel = new EasyKiConverter::ThemeSettingsViewModel();
    EasyKiConverter::UpdateCheckerService* updateCheckerService = new EasyKiConverter::UpdateCheckerService();

    // 创建 QML 引擎
    auto* engine = new QQmlApplicationEngine();

    // 注册 LanguageManager 到 QML
    qmlRegisterSingletonType<QObject>(
        "EasyKiconverter_Cpp_Version", 1, 0, "LanguageManager", [](QQmlEngine*, QJSEngine*) -> QObject* {
            return EasyKiConverter::LanguageManager::instance();
        });

    // 连接语言管理器的刷新信号到引擎的重新翻译
    QObject::connect(
        EasyKiConverter::LanguageManager::instance(),
        &EasyKiConverter::LanguageManager::refreshRequired,
        engine,
        [engine]() { engine->retranslate(); },
        Qt::QueuedConnection);

    // 将 ViewModel 和 Service 注册到 QML 上下文
    engine->rootContext()->setContextProperty("componentListViewModel", componentListViewModel);
    engine->rootContext()->setContextProperty("exportSettingsViewModel", exportSettingsViewModel);
    engine->rootContext()->setContextProperty("exportProgressViewModel", exportProgressViewModel);
    engine->rootContext()->setContextProperty("themeSettingsViewModel", themeSettingsViewModel);
    engine->rootContext()->setContextProperty("updateCheckerService", updateCheckerService);
    engine->rootContext()->setContextProperty("configService", EasyKiConverter::ConfigService::instance());
    engine->rootContext()->setContextProperty("componentCacheService",
                                              EasyKiConverter::ComponentCacheService::instance());

    // 连接对象创建失败信号
    QObject::connect(
        engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl& url) {
            qCritical() << "创建QML对象失败：" << url;
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    // 加载 QML 文件
    const QUrl url("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/Main.qml");
    engine->load(url);

    qWarning() << "===== QML 加载完成，开始设置图标 =====";

    if (engine->rootObjects().isEmpty()) {
        qCritical() << "严重错误：QML引擎根对象加载后为空！";
        delete engine;
        return -1;
    }

    // 获取根窗口对象并设置窗口位置和图标
    auto* rootObject = engine->rootObjects().first();
    if (auto* window = qobject_cast<QQuickWindow*>(rootObject)) {
        // 调试：显示所有可能的图标路径
        QString appImagePath = qgetenv("APPIMAGE");
        qWarning() << "===== 图标调试信息 =====";
        qWarning() << "APPIMAGE 环境变量:" << appImagePath;
        qWarning() << "applicationDirPath:" << QCoreApplication::applicationDirPath();
        qWarning() << "XDG_DATA_DIRS:" << qgetenv("XDG_DATA_DIRS");

        // AppImage 挂载点在 /tmp/.mount_XXX/，需要获取父目录
        QString appDir = normalizeMountedAppDir(QCoreApplication::applicationDirPath());
        qWarning() << "使用 appDir:" << appDir;

        bool isDarkModeForIcon = EasyKiConverter::ConfigService::instance()->getDarkMode();
        QString iconThemeDir = isDarkModeForIcon ? "hicolor-dark" : "hicolor";
        bool isAppImage = !qgetenv("APPIMAGE").isEmpty();

        // AppImage 内部的图标路径（优先级最高）
        QStringList appImageIconPaths = {
            appDir + "/usr/share/icons/" + iconThemeDir +
                "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",  // 优先使用 256x256
            appDir +
                "/usr/share/icons/hicolor/256x256/apps/io.github.tangsangsimida.easykiconverter.png",  // fallback 到 hicolor
            appDir + "/io.github.tangsangsimida.easykiconverter.png",  // AppDir 根目录（可能是 64x64，作为 fallback）
            appDir + "/usr/share/icons/" + iconThemeDir +
                "/128x128/apps/io.github.tangsangsimida.easykiconverter.png",  // 128x128
            appDir + "/usr/share/icons/hicolor/128x128/apps/io.github.tangsangsimida.easykiconverter.png",  // 128x128
            appDir + "/usr/share/icons/" + iconThemeDir +
                "/64x64/apps/io.github.tangsangsimida.easykiconverter.png",  // 64x64
            appDir + "/usr/share/icons/hicolor/64x64/apps/io.github.tangsangsimida.easykiconverter.png",  // 64x64
        };

        // 计算项目根目录（用于开发模式）
        QString projectRoot = resolveProjectRootFromAppDir(appDir);

        // 开发时的图标路径（优先级较低）
        QStringList devIconPaths = {
            projectRoot + "/resources/icons/" + iconThemeDir +
                "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            projectRoot + "/resources/icons/" + iconThemeDir +
                "/64x64/apps/io.github.tangsangsimida.easykiconverter.png",
            projectRoot + "/resources/icons/" + iconThemeDir +
                "/128x128/apps/io.github.tangsangsimida.easykiconverter.png",
            projectRoot + "/resources/icons/" + iconThemeDir +
                "/32x32/apps/io.github.tangsangsimida.easykiconverter.png",
            QCoreApplication::applicationDirPath() + "/io.github.tangsangsimida.easykiconverter.png",
            projectRoot + "/io.github.tangsangsimida.easykiconverter.png",
        };

        // 系统图标路径（fallback）
        QStringList systemIconPaths = {
            "io.github.tangsangsimida.easykiconverter",  // 让 Qt 在标准路径查找
            "/usr/share/icons/hicolor/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
            "/usr/share/icons/" + iconThemeDir + "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
        };

        QStringList iconPaths;
        if (isAppImage) {
            // AppImage: 优先使用内部图标
            iconPaths = appImageIconPaths + devIconPaths + systemIconPaths;
        } else {
            // 非AppImage: 优先使用开发路径
            iconPaths = devIconPaths + appImageIconPaths + systemIconPaths;
        }

        bool iconSet = false;
        for (const QString& path : iconPaths) {
            qWarning() << "尝试图标路径:" << path << "存在:" << QFile::exists(path);
            QIcon icon(path);
            if (!icon.isNull()) {
                QWindow* qwindow = window;
                qwindow->setIcon(icon);
                QGuiApplication::setWindowIcon(icon);

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
                qwindow->raise();
                qwindow->requestActivate();
                QCoreApplication::processEvents();
#endif

                qWarning() << "[OK] 任务栏图标已设置:" << path;
                qWarning() << "  图标可用尺寸:" << icon.availableSizes();
                qWarning() << "  图标是否为空:" << icon.isNull();
                iconSet = true;
                break;
            }
        }

        if (!iconSet) {
            qWarning() << "[FAIL] 未能设置任务栏图标";
            qWarning() << "  尝试了" << iconPaths.size() << "个路径，但都失败";
        }

        qWarning() << "===== 图标调试信息结束 =====";

        QTimer::singleShot(100, [window]() {
            qDebug() << "Startup window state:" << "visible=" << window->isVisible()
                     << "visibility=" << window->visibility() << "position=" << window->position()
                     << "size=" << window->size()
                     << "screen=" << (window->screen() ? window->screen()->name() : QStringLiteral("null"));

            if (!window->isVisible()) {
                qWarning() << "检测到根窗口仍为隐藏状态，执行 C++ 侧兜底显示";
                window->show();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
                window->raise();
                window->requestActivate();
#endif
                QCoreApplication::processEvents();
                qDebug() << "Fallback display state:" << "visible=" << window->isVisible()
                         << "visibility=" << window->visibility() << "position=" << window->position()
                         << "size=" << window->size();
            }

            // 窗口显示后再次设置任务栏图标
            QString appDir = normalizeMountedAppDir(QCoreApplication::applicationDirPath());
            QString projectRoot = resolveProjectRootFromAppDir(appDir);

            bool isDarkModeForIcon = EasyKiConverter::ConfigService::instance()->getDarkMode();
            QString iconThemeDir = isDarkModeForIcon ? "hicolor-dark" : "hicolor";
            bool isAppImage = !qgetenv("APPIMAGE").isEmpty();

            // 定义图标路径列表（与初始化时相同，优先使用 256x256）
            QStringList iconPaths;
            if (isAppImage) {
                iconPaths = {
                    appDir + "/usr/share/icons/" + iconThemeDir +
                        "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/usr/share/icons/hicolor/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/usr/share/icons/" + iconThemeDir +
                        "/128x128/apps/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/usr/share/icons/hicolor/128x128/apps/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/resources/icons/" + iconThemeDir +
                        "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
                };
            } else {
                iconPaths = {
                    projectRoot + "/resources/icons/" + iconThemeDir +
                        "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
                    projectRoot + "/resources/icons/" + iconThemeDir +
                        "/128x128/apps/io.github.tangsangsimida.easykiconverter.png",
                    projectRoot + "/resources/icons/" + iconThemeDir +
                        "/64x64/apps/io.github.tangsangsimida.easykiconverter.png",
                    projectRoot + "/resources/icons/" + iconThemeDir +
                        "/32x32/apps/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/io.github.tangsangsimida.easykiconverter.png",
                    projectRoot + "/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/usr/share/icons/" + iconThemeDir +
                        "/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
                    appDir + "/usr/share/icons/hicolor/256x256/apps/io.github.tangsangsimida.easykiconverter.png",
                };
            }

            bool iconSet = false;
            for (const QString& path : iconPaths) {
                if (QFile::exists(path)) {
                    QIcon icon(path);
                    window->setIcon(icon);

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
                    // 对于 FramelessWindowHint，强制刷新任务栏图标
                    window->raise();
                    window->requestActivate();
#endif

                    qWarning() << "窗口显示后重新设置任务栏图标:" << path;
                    iconSet = true;
                    break;
                }
            }

            if (!iconSet) {
                qWarning() << "[FAIL] 窗口显示后未能重新设置任务栏图标";
            }
        });
    }

    // 点击关闭按钮时直接退出应用程序
    app.setQuitOnLastWindowClosed(true);

    QTimer::singleShot(2000, updateCheckerService, &EasyKiConverter::UpdateCheckerService::checkForUpdates);

    // 连接应用程序的 aboutToQuit 信号，确保退出前清理所有资源
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        qDebug() << "Application is about to quit, performing cleanup...";

        // 1. 取消导出服务（如果正在导出）
        if (exportService) {
            qDebug() << "Cancelling export service...";
            exportService->cancelExport();
        }

        // 2. 取消所有挂起的组件请求
        if (componentService) {
            qDebug() << "Cancelling all pending component requests...";
            componentService->cancelAllPendingRequests();
        }

        qDebug() << "Cleanup completed, application can now exit.";
    });

    // 运行事件循环
    int exitCode = app.exec();

    // === 重要：显式按顺序销毁对象，防止退出时崩溃 ===

    // 1. 清除 QML 引擎的上下文属性，防止 QML 组件访问已销毁的对象
    qDebug() << "Clearing QML context properties...";
    engine->rootContext()->setContextProperty("componentListViewModel", nullptr);
    engine->rootContext()->setContextProperty("exportSettingsViewModel", nullptr);
    engine->rootContext()->setContextProperty("exportProgressViewModel", nullptr);
    engine->rootContext()->setContextProperty("themeSettingsViewModel", nullptr);
    engine->rootContext()->setContextProperty("updateCheckerService", nullptr);
    engine->rootContext()->setContextProperty("componentCacheService", nullptr);

    // 2. 先销毁 QML 引擎，阻止事件继续投递到 QML/QObject 图树
    qDebug() << "Destroying QML engine...";
    delete engine;
    engine = nullptr;

    // 3. 在销毁 C++ 对象前清空挂起事件
    qDebug() << "Draining pending events before object teardown...";
    QCoreApplication::processEvents();

    // 4. 销毁 ViewModel
    qDebug() << "Destroying ViewModels...";
    delete updateCheckerService;
    updateCheckerService = nullptr;
    delete themeSettingsViewModel;
    themeSettingsViewModel = nullptr;
    delete exportProgressViewModel;
    exportProgressViewModel = nullptr;
    delete exportSettingsViewModel;
    exportSettingsViewModel = nullptr;
    delete componentListViewModel;
    componentListViewModel = nullptr;

    // 5. 销毁服务（析构函数会自动调用 cancelExport 和 cleanup）
    qDebug() << "Destroying services...";
    delete exportService;
    exportService = nullptr;
    delete componentService;
    componentService = nullptr;

    qDebug() << "Destroying network client singleton...";
    EasyKiConverter::NetworkClient::destroyInstance();

    // 6. 最后清理日志
    qDebug() << "Cleaning up logging...";
    EasyKiConverter::QtLogAdapter::uninstall();
    auto* logger = EasyKiConverter::Logger::instance();
    if (logger) {
        logger->flush();
        logger->close();
    }

    qDebug() << "Cleanup completed, exiting...";
    return exitCode;
}
