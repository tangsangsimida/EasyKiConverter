#include "src/core/LanguageManager.h"
#include "src/services/ConfigService.h"
#include "src/services/ExportService_Pipeline.h"
#include "src/ui/viewmodels/ComponentListViewModel.h"
#include "src/ui/viewmodels/ExportProgressViewModel.h"
#include "src/ui/viewmodels/ExportSettingsViewModel.h"
#include "src/ui/viewmodels/ThemeSettingsViewModel.h"
#include "src/utils/logging/Log.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QUrl>

#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#    include <windows.h>
#endif

namespace {

bool isDebugMode() {
    // 检查环境变量
    if (qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE")) {
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        return (debugValue == "true" || debugValue == "1" || debugValue == "yes");
    }
    return false;
}

void setupLogging(bool debugMode) {
    using namespace EasyKiConverter;

#ifdef _WIN32
    // 如果启用调试模式，动态分配控制台（仅对 GUI 应用有效）
    if (debugMode) {
        // 尝试分配控制台
        if (AllocConsole()) {
            // 重定向标准输出
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
            freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
            freopen_s((FILE**)stdin, "CONIN$", "r", stdin);

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

    if (debugMode) {
        // 调试模式：使用 Info 级别（避免 Debug 级别产生过多日志影响性能和成功率）
        // 注意：调试模式主要用于开发时查看错误和网络请求详情，不建议在生产环境使用
        logger->setGlobalLevel(LogLevel::Info);

        // 控制台输出（彩色，异步模式减少性能影响）
        auto consoleAppender = QSharedPointer<ConsoleAppender>::create(true, true);
        consoleAppender->setFormatter(QSharedPointer<PatternFormatter>::create(PatternFormatter::simplePattern()));
        logger->addAppender(consoleAppender);

        // 文件输出（仅记录错误和警告）
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(logDir);
        QString logPath = logDir + "/easykiconverter_debug.log";

        auto fileAppender =
            QSharedPointer<FileAppender>::create(logPath, 10 * 1024 * 1024, 5, true);  // 10MB, 5 files, async
        fileAppender->setFormatter(QSharedPointer<PatternFormatter>::create(PatternFormatter::simplePattern()));
        logger->addAppender(fileAppender);

        LOG_INFO(LogModule::Core, "调试模式已启用 - 日志文件: {}", logPath);
    } else {
        // 正常模式：仅输出 Info 及以上级别
        logger->setGlobalLevel(LogLevel::Info);

        // 仅控制台输出（异步模式）
        auto consoleAppender = QSharedPointer<ConsoleAppender>::create(true, true);
        consoleAppender->setFormatter(QSharedPointer<PatternFormatter>::create(PatternFormatter::simplePattern()));
        logger->addAppender(consoleAppender);
    }

    // 安装 Qt 日志适配器（将 qDebug/qWarning/qCritical 重定向到新系统）
    QtLogAdapter::install();
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    // 检查调试模式（在 QApplication 创建前检查）
    bool debugMode = isDebugMode();

    // 设置 QML 样式为 Basic，以消除原生样式自定义警告
    // 注意：样式必须在创建应用程序实例之前设置
    QQuickStyle::setStyle("Basic");

    QApplication app(argc, argv);

    // 初始化日志系统（在 QApplication 创建后立即初始化）
    setupLogging(debugMode);

    // 设置应用程序信息
    app.setApplicationName("EasyKiConverter");
    app.setApplicationVersion("3.0.7");
    app.setOrganizationName("EasyKiConverter");
    app.setOrganizationDomain("easykiconverter.com");

    // 尝试设置应用程序图标
    QStringList iconPaths = {":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png",
                             ":/resources/icons/app_icon.png",
                             "resources/icons/app_icon.png",
                             "../resources/icons/app_icon.png"};

    for (const QString& iconPath : iconPaths) {
        QIcon icon(iconPath);
        if (!icon.isNull()) {
            app.setWindowIcon(icon);
            qDebug() << "应用程序图标已设置：" << iconPath;
            break;
        }
    }

    // 初始化配置服务
    EasyKiConverter::ConfigService::instance()->loadConfig();

    // 初始化语言管理器
    EasyKiConverter::LanguageManager::instance();

    // 创建 Service 实例（使用流水线架构）
    EasyKiConverter::ComponentService* componentService = new EasyKiConverter::ComponentService(&app);
    EasyKiConverter::ExportServicePipeline* exportService = new EasyKiConverter::ExportServicePipeline(&app);

    // 创建 ViewModel 实例
    EasyKiConverter::ComponentListViewModel* componentListViewModel =
        new EasyKiConverter::ComponentListViewModel(componentService, &app);
    EasyKiConverter::ExportSettingsViewModel* exportSettingsViewModel =
        new EasyKiConverter::ExportSettingsViewModel(exportService, &app);
    EasyKiConverter::ExportProgressViewModel* exportProgressViewModel =
        new EasyKiConverter::ExportProgressViewModel(exportService, componentService, componentListViewModel, &app);
    EasyKiConverter::ThemeSettingsViewModel* themeSettingsViewModel = new EasyKiConverter::ThemeSettingsViewModel(&app);

    // 创建 QML 引擎
    QQmlApplicationEngine engine;

    // 注册 LanguageManager 到 QML
    qmlRegisterSingletonType<QObject>(
        "EasyKiconverter_Cpp_Version", 1, 0, "LanguageManager", [](QQmlEngine*, QJSEngine*) -> QObject* {
            return EasyKiConverter::LanguageManager::instance();
        });

    // 连接语言管理器的刷新信号到引擎的重新翻译
    QObject::connect(
        EasyKiConverter::LanguageManager::instance(),
        &EasyKiConverter::LanguageManager::refreshRequired,
        &engine,
        [&engine]() { engine.retranslate(); },
        Qt::QueuedConnection);

    // 将 ViewModel 注册到 QML 上下文
    engine.rootContext()->setContextProperty("componentListViewModel", componentListViewModel);
    engine.rootContext()->setContextProperty("exportSettingsViewModel", exportSettingsViewModel);
    engine.rootContext()->setContextProperty("exportProgressViewModel", exportProgressViewModel);
    engine.rootContext()->setContextProperty("themeSettingsViewModel", themeSettingsViewModel);

    // 连接对象创建失败信号
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl& url) {
            qCritical() << "创建QML对象失败：" << url;
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    // 加载 QML 文件
    const QUrl url("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/Main.qml");
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "严重错误：QML引擎根对象加载后为空！";
        return -1;
    }

    // 点击关闭按钮时直接退出应用程序
    app.setQuitOnLastWindowClosed(true);

    return app.exec();
}
