// 我发现我在学校，不谈恋爱也不学习，不打游戏也不逃课，
// 每天就是处于认真听课，然后因为听不懂走神了，
// 反应过来以后再继续认真听课的循环，进入这种状态的时候，
// 我一般就是左脑和右脑已经聊美了，完全就是一个放飞自我，
// 意淫我有多少多少家产，然后怕班里有人会读心术紧急撤回一条意淫，
// 那这在恋爱小说里我不就是那种…背景板吗，
// 就每天在学校挂机任务就完成的npc
#include "src/core/LanguageManager.h"
#include "src/core/network/NetworkClient.h"
#include "src/services/ConfigService.h"
#include "src/services/export/ParallelExportService.h"
#include "src/services/export/ExportProgress.h"
#include "src/ui/viewmodels/ComponentListViewModel.h"
#include "src/ui/viewmodels/ExportProgressViewModel.h"
#include "src/ui/viewmodels/ExportSettingsViewModel.h"
#include "src/ui/viewmodels/ThemeSettingsViewModel.h"
#include "src/utils/CommandLineParser.h"
#include "src/utils/logging/Log.h"

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
    // 设置 stdout 为无缓冲模式，确保日志颜色正常显示
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    // 在 QApplication 构造函数之前检查命令行参数
    bool showHelp = false;
    bool showVersion = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        // 支持 Unix 风格（-h, --help）和 Windows 风格（/h, /help, /?, ?）的参数
        if (arg == "--help" || arg == "-h" || arg == "/h" || arg == "/help" || arg == "/?" || arg == "?") {
            showHelp = true;
        } else if (arg == "--version" || arg == "-v" || arg == "/v" || arg == "/version") {
            showVersion = true;
        }
    }

    // 设置 QML 样式为 Basic，以消除原生样式自定义警告
    // 注意：样式必须在创建应用程序实例之前设置
    QQuickStyle::setStyle("Basic");

    QApplication app(argc, argv);

    // 设置应用程序信息（必须在解析命令行参数之前）
    app.setApplicationName("EasyKiConverter");
    app.setApplicationVersion("3.1.3");
    app.setOrganizationName("EasyKiConverter");
    app.setOrganizationDomain("easykiconverter.com");

    // 创建命令行参数解析器（必须在检查showHelp之前）
    EasyKiConverter::CommandLineParser cmdParser(argc, argv);

    if (showHelp) {
        // 使用 CommandLineParser 生成的帮助文本
        QString helpText = cmdParser.helpText();

#ifdef _WIN32
        // 尝试附加到父进程的控制台（如果是从命令行启动的）
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            // 如果附加失败，创建新的控制台窗口
            AllocConsole();
            SetConsoleTitleA("EasyKiConverter - Help");
        }
        // 重定向标准输出
        (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");
        (void)reopenConsoleStream(stderr, "CONOUT$", "w", "stderr");

        // 输出帮助信息到控制台
        QTextStream consoleOut(stdout);
        consoleOut << helpText;
#else
        QTextStream consoleOut(stdout);
        consoleOut << helpText;
#endif
        return 0;
    }

    if (showVersion) {
        QString versionText = "EasyKiConverter " + app.applicationVersion() + "\n";

#ifdef _WIN32
        // 尝试附加到父进程的控制台（如果是从命令行启动的）
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            // 如果附加失败，创建新的控制台窗口
            AllocConsole();
            SetConsoleTitleA("EasyKiConverter - Version");
        }
        // 重定向标准输出
        (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");

        // 输出版本信息到控制台
        QTextStream consoleOut(stdout);
        consoleOut << versionText;
#else
        QTextStream consoleOut(stdout);
        consoleOut << versionText;
#endif
        return 0;
    }

    // 解析所有命令行参数
    if (!cmdParser.parse()) {
#ifdef _WIN32
        // 尝试附加到父进程的控制台（如果是从命令行启动的）
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            // 如果附加失败，创建新的控制台窗口
            AllocConsole();
            SetConsoleTitleA("EasyKiConverter - Error");
        }
        // 重新打开标准输出
        (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");
        (void)reopenConsoleStream(stderr, "CONOUT$", "w", "stderr");
#endif
        QTextStream err(stderr);
        err << "错误: 无效的命令行参数\n";
        err << cmdParser.helpText();
        return 1;
    }

    // 验证参数值
    if (!cmdParser.validate()) {
#ifdef _WIN32
        // 尝试附加到父进程的控制台（如果是从命令行启动的）
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            // 如果附加失败，创建新的控制台窗口
            AllocConsole();
            SetConsoleTitleA("EasyKiConverter - Error");
        }
        // 重新打开标准输出
        (void)reopenConsoleStream(stdout, "CONOUT$", "w", "stdout");
        (void)reopenConsoleStream(stderr, "CONOUT$", "w", "stderr");
#endif
        QTextStream err(stderr);
        err << "错误: 参数值无效\n";
        err << cmdParser.validationError() << "\n\n";
        err << cmdParser.helpText();
        return 1;
    }

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
    // 预先初始化网络客户端，确保在主线程创建 QNetworkAccessManager
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

        auto* configService = EasyKiConverter::ConfigService::instance();

        // 先隐藏窗口，等待 QML 完全加载后再显示
        window->hide();

        // 使用延迟执行，确保 QML 窗口已经完全初始化并获取到正确的尺寸
        QTimer::singleShot(100, [window, configService]() {
            // 窗口显示后再次设置任务栏图标
            QString appDir = normalizeMountedAppDir(QCoreApplication::applicationDirPath());
            QString projectRoot = resolveProjectRootFromAppDir(appDir);

            bool isDarkModeForIcon = configService->getDarkMode();
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

            int savedX = configService->getWindowX();
            int savedY = configService->getWindowY();

            int posX, posY;
            bool useSavedPosition = false;

            // 如果配置中保存了有效位置（不是默认值 (0,0)），则使用保存的位置
            if (savedX > 0 && savedY > 0) {
                posX = savedX;
                posY = savedY;
                useSavedPosition = true;
                qDebug() << "使用保存的窗口位置:" << posX << posY;
            } else {
                // 否则居中显示
                QScreen* screen = window->screen();
                if (screen) {
                    QRect screenGeometry = screen->availableGeometry();
                    int screenWidth = screenGeometry.width();
                    int screenHeight = screenGeometry.height();

                    // 使用窗口的当前尺寸
                    int windowWidth = window->width();
                    int windowHeight = window->height();

                    // 如果窗口尺寸不合理（小于最小尺寸），使用默认尺寸
                    if (windowWidth < 800)
                        windowWidth = 800;
                    if (windowHeight < 600)
                        windowHeight = 600;

                    // 计算居中位置
                    posX = (screenWidth - windowWidth) / 2;
                    posY = (screenHeight - windowHeight) / 2;

                    qDebug() << "屏幕尺寸:" << screenWidth << "x" << screenHeight << "窗口尺寸:" << windowWidth << "x"
                             << windowHeight << "计算居中位置:" << posX << "," << posY;

                    // 保存居中位置到配置，避免下次启动时又回到左上角
                    configService->setWindowX(posX);
                    configService->setWindowY(posY);
                    configService->saveConfig();
                    qDebug() << "已保存居中位置到配置";
                } else {
                    posX = 100;
                    posY = 100;
                }
            }

            qDebug() << "设置窗口位置到:" << posX << posY;

            // 使用 setPosition 设置窗口位置
            window->setPosition(posX, posY);

            // 显示窗口
            window->show();

            // 等待窗口实际显示，然后检查实际位置
            QTimer::singleShot(100, [window, posX, posY, useSavedPosition]() {
                QScreen* screen = window->screen();
                if (screen) {
                    QRect screenGeometry = screen->availableGeometry();
                    QRect windowGeometry = window->geometry();

                    qDebug() << "窗口已显示:";
                    qDebug() << "  期望位置: (" << posX << "," << posY << ")";
                    qDebug() << "  实际位置: (" << windowGeometry.x() << "," << windowGeometry.y() << ")";
                    qDebug() << "  窗口大小: (" << windowGeometry.width() << "x" << windowGeometry.height() << ")";
                    qDebug() << "  屏幕大小: (" << screenGeometry.width() << "x" << screenGeometry.height() << ")";

                    // 如果使用保存位置但实际位置不对，说明保存的位置有问题
                    if (useSavedPosition && (windowGeometry.x() <= 0 || windowGeometry.y() <= 0)) {
                        qDebug() << "警告: 保存的窗口位置无效，下次启动将重新居中";
                    }
                }
            });
        });
    }

    // 点击关闭按钮时直接退出应用程序
    app.setQuitOnLastWindowClosed(true);

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
