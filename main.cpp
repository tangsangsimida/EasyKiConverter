#include "src/core/LanguageManager.h"
#include "src/services/ConfigService.h"
#include "src/services/ExportService_Pipeline.h"
#include "src/ui/viewmodels/ComponentListViewModel.h"
#include "src/ui/viewmodels/ExportProgressViewModel.h"
#include "src/ui/viewmodels/ExportSettingsViewModel.h"
#include "src/ui/viewmodels/ThemeSettingsViewModel.h"

#include <QDebug>
#include <QFile>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("EasyKiConverter");
    app.setApplicationVersion("3.0.2");
    app.setOrganizationName("EasyKiConverter");
    app.setOrganizationDomain("easykiconverter.com");

    // 设置 QML 样式为 Basic，以消除原生样式自定义警告
    QQuickStyle::setStyle("Basic");

    // 尝试设置应用程序图标
    QStringList iconPaths = {":/resources/app_icon.png", "resources/app_icon.png", "../resources/app_icon.png"};

    for (const QString& iconPath : iconPaths) {
        if (QFile::exists(iconPath)) {
            app.setWindowIcon(QIcon(iconPath));
            qDebug() << "Application icon set:" << iconPath;
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

    // 注册 AppStyle 单例类型
    qmlRegisterSingletonType(QUrl("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/styles/AppStyle.qml"),
                             "EasyKiconverter_Cpp_Version.src.ui.qml.styles",
                             1,
                             0,
                             "AppStyle");

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
        [&engine]() {
            // 重新翻译 QML 引擎
            engine.retranslate();
            qDebug() << "QML engine retranslated";
        },
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
        []() {
            qCritical() << "Failed to create QML objects";
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    // 加载 QML 模块
    engine.loadFromModule("EasyKiconverter_Cpp_Version", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML module";
        return -1;
    }

    qDebug() << "EasyKiConverter started successfully with Pipeline Architecture";

    return app.exec();
}