#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QIcon>
#include <QFile>
#include <QDebug>
#include <QQuickStyle>
#include <QUrl>
#include "src/ui/controllers/MainController.h"

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

    // 创建主控制器
    EasyKiConverter::MainController *controller = new EasyKiConverter::MainController(&app);

    // 创建 QML 引擎
    QQmlApplicationEngine engine;

    // 注册 AppStyle 单例类型
    qmlRegisterSingletonType(QUrl("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/styles/AppStyle.qml"), 
                          "EasyKiconverter_Cpp_Version.src.ui.qml.styles", 
                          1, 0, "AppStyle");

    // 将控制器注册到 QML 上下文
    engine.rootContext()->setContextProperty("mainController", controller);

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