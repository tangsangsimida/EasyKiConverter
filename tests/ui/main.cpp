
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QtQuickTest>

class Setup : public QObject {
    Q_OBJECT

public:
    Setup() {
        QQuickStyle::setStyle("Basic");
    }

public slots:

    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->addImportPath("qrc:/qt/qml");

        // 注册 AppStyle 单例类型 (必须手动注册以匹配 ModernButton.qml 中的导入 URI)
        qmlRegisterSingletonType(QUrl("qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/styles/AppStyle.qml"),
                                 "EasyKiconverter_Cpp_Version.src.ui.qml.styles",
                                 1,
                                 0,
                                 "AppStyle");
    }
};

QUICK_TEST_MAIN_WITH_SETUP(ui_tests, Setup)

#include "main.moc"
