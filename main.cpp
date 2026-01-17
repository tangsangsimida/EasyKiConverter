#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QIcon>
#include <QFile>
#include <QDebug>
#include <QQuickStyle>
#include <QUrl>
#include "src/ui/viewmodels/ComponentListViewModel.h"
#include "src/ui/viewmodels/ExportSettingsViewModel.h"
#include "src/ui/viewmodels/ExportProgressViewModel.h"
#include "src/ui/viewmodels/ThemeSettingsViewModel.h"
#include "src/services/ConfigService.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("EasyKiConverter");
    app.setApplicationVersion("3.0.0");
    app.setOrganizationName("EasyKiConverter");
    app.setOrganizationDomain("easykiconverter.com");

    // 设置 QML 样式为 Basic，以消除原生样式自定义警告
    QQuickStyle::setStyle("Basic");

    // 尝试设置应用程序图标
    QStringList iconPaths = {
        ":/resources/app_icon.png",
        "resources/app_icon.png",
        "../resources/app_icon.png"
    };

    for (const QString &iconPath : iconPaths) {
        if (QFile::exists(iconPath)) {
            app.setWindowIcon(QIcon(iconPath));
            qDebug() << "Application icon set:" << iconPath;
            break;
        }
    }

    // 初始化配置服务
    EasyKiConverter::ConfigService::instance()->loadConfig();

    // 创建 Service 实例
    EasyKiConverter::ComponentService *componentService = new EasyKiConverter::ComponentService(&app);
    EasyKiConverter::ExportService *exportService = new EasyKiConverter::ExportService(&app);

    // 创建 ViewModel 实例
    EasyKiConverter::ComponentListViewModel *componentListViewModel = new EasyKiConverter::ComponentListViewModel(componentService, &app);
    EasyKiConverter::ExportSettingsViewModel *exportSettingsViewModel = new EasyKiConverter::ExportSettingsViewModel(&app);
    EasyKiConverter::ExportProgressViewModel *exportProgressViewModel = new EasyKiConverter::ExportProgressViewModel(exportService, componentService, &app);
    EasyKiConverter::ThemeSettingsViewModel *themeSettingsViewModel = new EasyKiConverter::ThemeSettingsViewModel(&app);

    // 创建 QML 引擎
    QQmlApplicationEngine engine;

    // 注册 AppStyle 单例类型
    qmlRegisterSingletonType(QUrl("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/styles/AppStyle.qml"),
                          "EasyKiconverter_Cpp_Version.src.ui.qml.styles",
                          1, 0, "AppStyle");

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

    qDebug() << "EasyKiConverter started successfully";

    return app.exec();
}