#include "LanguageManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocale>
#include <QTranslator>

namespace EasyKiConverter {

LanguageManager* LanguageManager::s_instance = nullptr;

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_currentLanguage("auto")
    , m_settings(new QSettings("EasyKiConverter", "EasyKiConverter", this))
    , m_translator(nullptr) {
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

    // 安装新的翻译器 (installTranslator 会处理旧翻译器的移除)
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
    QStringList uiLanguages = systemLocale.uiLanguages();
    
    qDebug() << "System UI languages:" << uiLanguages;
    qDebug() << "System locale name:" << systemLocale.name();

    // 优先检查 UI 语言列表
    if (!uiLanguages.isEmpty()) {
        QString firstLang = uiLanguages.first();
        if (firstLang.startsWith("zh", Qt::CaseInsensitive)) {
            return "zh_CN";
        }
    }
    
    // 回退到检查 locale 名称
    if (systemLocale.name().startsWith("zh", Qt::CaseInsensitive)) {
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

    // 如果已有翻译器，先移除并删除
    if (m_translator) {
        app->removeTranslator(m_translator);
        m_translator->deleteLater();
        m_translator = nullptr;
    }

    // Qt 6 qml_module 资源路径 - 使用 compiled .qm files
    QString translationPath =
        QString(":/qt/qml/EasyKiconverter_Cpp_Version/resources/translations/translations_easykiconverter_%1.qm")
            .arg(languageCode);

    m_translator = new QTranslator(this);

    // 尝试加载翻译文件（.qm 格式）
    if (m_translator->load(translationPath)) {
        app->installTranslator(m_translator);
        qDebug() << "Installed translator:" << translationPath;
        return;
    }

    // 如果资源文件不存在，尝试从文件系统加载
    QString localPath = QString("resources/translations/translations_easykiconverter_%1.qm").arg(languageCode);
    if (m_translator->load(localPath)) {
        app->installTranslator(m_translator);
        qDebug() << "Installed translator from file system:" << localPath;
        return;
    }

    qWarning() << "Failed to load translation file for language:" << languageCode;
    qWarning() << "Tried paths:" << translationPath << localPath;
    
    // 加载失败时，清理创建的翻译器对象
    m_translator->deleteLater();
    m_translator = nullptr;
}

}  // namespace EasyKiConverter