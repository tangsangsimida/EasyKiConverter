
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QString>
#include <QtQuickTest>

class FakeLanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)

public:
    explicit FakeLanguageManager(QObject* parent = nullptr) : QObject(parent) {}

    QString currentLanguage() const {
        return m_currentLanguage;
    }

    Q_INVOKABLE void setLanguage(const QString& language) {
        if (m_currentLanguage == language) {
            return;
        }

        m_currentLanguage = language;
        emit languageChanged(m_currentLanguage);
    }

signals:
    void languageChanged(const QString& language);

private:
    QString m_currentLanguage{QStringLiteral("zh_CN")};
};

class Setup : public QObject {
    Q_OBJECT

public:
    Setup() {
        QQuickStyle::setStyle("Basic");
    }

public slots:

    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->addImportPath("qrc:/qt/qml");
        engine->rootContext()->setContextProperty("LanguageManager", new FakeLanguageManager(engine));

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
