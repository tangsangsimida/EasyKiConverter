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
        // 调试模式：使用 Debug 级别，以便所有 qDebug() 输出都能被记录
        // 注意：调试模式主要用于开发时查看错误和网络请求详情，不建议在生产环境使用
        logger->setGlobalLevel(LogLevel::Debug);

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

    // 在 Linux 系统上，自动创建桌面集成配置（如果不存在）
#ifdef __linux__
    // 设置桌面文件名称（使用小写，与安装的 desktop 文件保持一致）
    app.setDesktopFileName("com.tangsangsimida.easykiconverter");

    // 检查并创建桌面集成配置
    QString localAppsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QString iconsBaseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/hicolor";
    QString desktopFilePath = localAppsDir + "/com.tangsangsimida.easykiconverter.desktop";

    // 检查桌面文件是否存在
    if (!QFile::exists(desktopFilePath)) {
        qDebug() << "桌面文件不存在，自动创建：" << desktopFilePath;

        // 创建必要的目录
        QDir().mkpath(localAppsDir);

        // 获取可执行文件路径
        QString executablePath = QCoreApplication::applicationFilePath();

        // 创建桌面文件
        QFile desktopFile(desktopFilePath);
        if (desktopFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&desktopFile);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=EasyKiConverter\n";
            out << "Name[zh_CN]=EasyKiConverter\n";
            out << "Comment=Convert LCSC and EasyEDA components to KiCad libraries\n";
            out << "Comment[zh_CN]=将嘉立创和 EasyEDA 元件转换为 KiCad 库\n";
            out << "Exec=" << executablePath << " %F\n";
            out << "Icon=com.tangsangsimida.easykiconverter\n";
            out << "Terminal=false\n";
            out << "Categories=Development;Electronics;Engineering;\n";
            out << "Keywords=KiCad;LCSC;EasyEDA;Component;Converter;Electronics;\n";
            out << "StartupNotify=true\n";
            out << "StartupWMClass=easykiconverter\n";
            out << "MimeType=application/vnd.easyeda+json;\n";
            desktopFile.close();

            // 设置执行权限
            QFile::setPermissions(
                desktopFilePath,
                QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ReadOther);

            qDebug() << "桌面文件已创建：" << desktopFilePath;
        } else {
            qWarning() << "无法创建桌面文件：" << desktopFilePath;
        }

        // 创建多个尺寸的图标（支持高DPI显示）
        QList<int> iconSizes = {16, 32, 48, 64, 128, 256, 512, 1024};
        int iconsCreated = 0;

        // 查找 SVG 图标（可以生成任意尺寸）
        QString svgIconPath;
        for (const QString& iconPath : iconPaths) {
            if (iconPath.endsWith(".svg") && QFile::exists(iconPath)) {
                svgIconPath = iconPath;
                break;
            }
        }

        if (!svgIconPath.isEmpty()) {
            // 从 SVG 生成多个尺寸的图标
            QImage svgImage(svgIconPath);
            if (!svgImage.isNull()) {
                for (int size : iconSizes) {
                    QString sizeDir = QString("%1/%2x%3/apps").arg(iconsBaseDir).arg(size).arg(size);
                    QDir().mkpath(sizeDir);
                    QString iconFilePath = sizeDir + "/com.tangsangsimida.easykiconverter.png";

                    // 渲染 SVG 到指定尺寸
                    QImage scaledImage = svgImage.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    if (scaledImage.save(iconFilePath, "PNG")) {
                        iconsCreated++;
                        qDebug() << "图标已创建：" << iconFilePath << size << "x" << size;
                    }
                }
            } else {
                qWarning() << "无法加载 SVG 图标：" << svgIconPath;
            }
        }

        // 如果没有 SVG，使用 PNG 图标（复制到所有尺寸）
        if (iconsCreated == 0) {
            for (int size : iconSizes) {
                QString sizeDir = QString("%1/%2x%3/apps").arg(iconsBaseDir).arg(size).arg(size);
                QDir().mkpath(sizeDir);
                QString iconFilePath = sizeDir + "/com.tangsangsimida.easykiconverter.png";

                bool iconCopied = false;
                for (const QString& iconPath : iconPaths) {
                    if (iconPath.endsWith(".png") && QFile::exists(iconPath)) {
                        QImage sourceImage(iconPath);
                        if (!sourceImage.isNull()) {
                            QImage scaledImage =
                                sourceImage.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            if (scaledImage.save(iconFilePath, "PNG")) {
                                iconCopied = true;
                                break;
                            }
                        }
                    }
                }

                if (iconCopied) {
                    iconsCreated++;
                    qDebug() << "图标已创建：" << iconFilePath << size << "x" << size;
                }
            }
        }

        if (iconsCreated == 0) {
            qWarning() << "无法创建任何图标文件";
        } else {
            qDebug() << "已创建" << iconsCreated << "个尺寸的图标";
        }

        // 更新桌面数据库
        QProcess updateDesktopDb;
        updateDesktopDb.start("update-desktop-database", QStringList() << localAppsDir);
        updateDesktopDb.waitForFinished(5000);

        // 更新图标缓存
        QProcess updateIconCache;
        updateIconCache.start(
            "gtk-update-icon-cache",
            QStringList() << "-q" << "-t" << "-f"
                          << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/hicolor");
        updateIconCache.waitForFinished(5000);

        qDebug() << "桌面集成配置已自动创建";
    } else {
        qDebug() << "桌面文件已存在：" << desktopFilePath;
    }
#endif

    // 初始化配置服务
    EasyKiConverter::ConfigService::instance()->loadConfig();

    // 初始化语言管理器
    EasyKiConverter::LanguageManager::instance();

    // 创建 Service 实例（使用流水线架构，不设置 parent，手动管理生命周期）
    EasyKiConverter::ComponentService* componentService = new EasyKiConverter::ComponentService();
    EasyKiConverter::ExportServicePipeline* exportService = new EasyKiConverter::ExportServicePipeline();

    // 创建 ViewModel 实例（不设置 parent，手动管理生命周期）
    EasyKiConverter::ComponentListViewModel* componentListViewModel =
        new EasyKiConverter::ComponentListViewModel(componentService);
    EasyKiConverter::ExportSettingsViewModel* exportSettingsViewModel =
        new EasyKiConverter::ExportSettingsViewModel(exportService);
    EasyKiConverter::ExportProgressViewModel* exportProgressViewModel =
        new EasyKiConverter::ExportProgressViewModel(exportService, componentService, componentListViewModel);
    EasyKiConverter::ThemeSettingsViewModel* themeSettingsViewModel = new EasyKiConverter::ThemeSettingsViewModel();

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

    // 将 ViewModel 和 Service 注册到 QML 上下文
    engine.rootContext()->setContextProperty("componentListViewModel", componentListViewModel);
    engine.rootContext()->setContextProperty("exportSettingsViewModel", exportSettingsViewModel);
    engine.rootContext()->setContextProperty("exportProgressViewModel", exportProgressViewModel);
    engine.rootContext()->setContextProperty("themeSettingsViewModel", themeSettingsViewModel);
    engine.rootContext()->setContextProperty("configService", EasyKiConverter::ConfigService::instance());

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

    // 获取根窗口对象并设置窗口位置
    auto* rootObject = engine.rootObjects().first();
    if (auto* window = qobject_cast<QQuickWindow*>(rootObject)) {
        auto* configService = EasyKiConverter::ConfigService::instance();

        // 先隐藏窗口，等待 QML 完全加载后再显示
        window->hide();

        // 使用延迟执行，确保 QML 窗口已经完全初始化并获取到正确的尺寸
        QTimer::singleShot(100, [window, configService]() {
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

    // 运行事件循环
    int exitCode = app.exec();

    // === 重要：显式按顺序销毁对象，防止退出时崩溃 ===
    // 1. 首先清除 QML 引擎的上下文属性，防止 QML 组件访问已销毁的对象
    engine.rootContext()->setContextProperty("componentListViewModel", nullptr);
    engine.rootContext()->setContextProperty("exportSettingsViewModel", nullptr);
    engine.rootContext()->setContextProperty("exportProgressViewModel", nullptr);
    engine.rootContext()->setContextProperty("themeSettingsViewModel", nullptr);

    // 2. 销毁 QML 引擎（这会销毁所有 QML 组件）
    engine.deleteLater();

    // 3. 处理事件队列，确保 QML 引擎销毁完成
    QCoreApplication::processEvents();

    // 4. 销毁 ViewModel
    delete themeSettingsViewModel;
    delete exportProgressViewModel;
    delete exportSettingsViewModel;
    delete componentListViewModel;

    // 5. 销毁单例/核心服务
    delete exportService;
    delete componentService;

    // 3. 最后卸载日志适配器并关闭 Logger
    // 确保所有 worker 线程已在上述 delete 过程中停止后再关闭日志线程
    EasyKiConverter::QtLogAdapter::uninstall();
    auto* logger = EasyKiConverter::Logger::instance();
    if (logger) {
        logger->flush();
        logger->close();
    }

    return exitCode;
}
