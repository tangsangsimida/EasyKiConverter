#include "LanguageManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocale>
#include <QTranslator>
#include <QCoreApplication>

namespace EasyKiConverter {

LanguageManager* LanguageManager::s_instance = nullptr;

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_currentLanguage("auto")
    , m_settings(new QSettings("EasyKiConverter", "EasyKiConverter", this)) {
    loadLanguageSettings();
}

LanguageManager::~LanguageManager() {}

LanguageManager* LanguageManager::instance() {
    if (!s_instance) {
        s_instance = new LanguageManager();
    }
    return s_instance;
}

void LanguageManager::setLanguage(const QString& languageCode) {
    if (m_currentLanguage == languageCode) {
        return;
    }

    m_currentLanguage = languageCode;
    saveLanguageSettings();

    QString actualLanguage = languageCode;
    if (languageCode == "auto") {
        actualLanguage = detectSystemLanguage();
    }

    // 移除旧的翻译器
    QCoreApplication* app = QCoreApplication::instance();
    if (app) {
        // 移除所有翻译器
        auto translators = app->findChildren<QTranslator*>();
        for (QTranslator* trans : translators) {
            app->removeTranslator(trans);
            trans->deleteLater();
        }
    }

    // 安装新的翻译器
    installTranslator(actualLanguage);

    emit languageChanged(languageCode);
    emit refreshRequired();
    qDebug() << "Language changed to:" << languageCode << "(actual:" << actualLanguage << ")";
}

QString LanguageManager::currentLanguage() const {
    return m_currentLanguage;
}

QString LanguageManager::detectSystemLanguage() const {
    QLocale systemLocale = QLocale::system();
    QString languageCode = systemLocale.name();

    qDebug() << "System locale:" << languageCode;

    if (languageCode.startsWith("zh")) {
        return "zh_CN";
    }

    return "en";
}

void LanguageManager::loadLanguageSettings() {
    QString savedLanguage = m_settings->value("language", "auto").toString();
    m_currentLanguage = savedLanguage;

    QString actualLanguage = savedLanguage;
    if (savedLanguage == "auto") {
        actualLanguage = detectSystemLanguage();
    }

    installTranslator(actualLanguage);
    qDebug() << "Loaded language settings:" << savedLanguage << "(actual:" << actualLanguage << ")";
}

void LanguageManager::saveLanguageSettings() {
    m_settings->setValue("language", m_currentLanguage);
    m_settings->sync();
}

void LanguageManager::installTranslator(const QString& languageCode) {
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        qWarning() << "QCoreApplication instance not available";
        return;
    }

    // Qt 6 qml_module 资源路径
    QString qmPath = QString(":/qt/qml/EasyKiconverter_Cpp_Version/resources/translations/translations_easykiconverter_%1.qm").arg(languageCode);

    QTranslator* translator = new QTranslator(this);

    // 尝试加载 .qm 文件
    if (translator->load(qmPath)) {
        app->installTranslator(translator);
        qDebug() << "Installed translator:" << qmPath;
        return;
    }

    // 如果 .qm 文件不存在，尝试从文件系统加载
    QString localQmPath = QString("resources/translations/translations_easykiconverter_%1.qm").arg(languageCode);
    if (translator->load(localQmPath)) {
        app->installTranslator(translator);
        qDebug() << "Installed translator from file system:" << localQmPath;
        return;
    }

    qWarning() << "Failed to load translation file for language:" << languageCode;
    qWarning() << "Tried paths:" << qmPath << localQmPath;
    qWarning() << "Note: .qm files must be generated using lrelease tool from .ts files";
}

}  // namespace EasyKiConverter