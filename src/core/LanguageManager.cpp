#include "LanguageManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocale>
#include <QMap>
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

    // 支持的语言映射表
    const QMap<QString, QString> supportedLanguages = {
        {"zh", "zh_CN"},       // 中文（通用）
        {"zh-CN", "zh_CN"},    // 简体中文
        {"zh_CN", "zh_CN"},    // 简体中文
        {"zh-SG", "zh_CN"},    // 新加坡中文
        {"zh-MO", "zh_CN"},    // 澳门中文
        {"zh-Hans", "zh_CN"},  // 简体中文（脚本）
        {"zh-TW", "zh_CN"},    // 繁体中文（映射到简体）
        {"zh-HK", "zh_CN"},    // 香港中文（映射到简体）
        {"en", "en"},          // 英语（通用）
        {"en-US", "en"},       // 美式英语
        {"en-GB", "en"},       // 英式英语
        {"en-AU", "en"},       // 澳大利亚英语
        {"en-CA", "en"},       // 加拿大英语
    };

    // 优先检查 UI 语言列表
    if (!uiLanguages.isEmpty()) {
        QString firstLang = uiLanguages.first();
        // 完全匹配
        if (supportedLanguages.contains(firstLang)) {
            QString detectedLang = supportedLanguages.value(firstLang);
            qDebug() << "Detected language from UI list (exact match):" << firstLang << "->" << detectedLang;
            return detectedLang;
        }

        // 基础语言匹配（例如 "zh-CN" 匹配 "zh"）
        QString baseLang = firstLang.split('-').first();
        if (supportedLanguages.contains(baseLang)) {
            QString detectedLang = supportedLanguages.value(baseLang);
            qDebug() << "Detected language from UI list (base match):" << firstLang << "->" << detectedLang;
            return detectedLang;
        }
    }

    // 回退到检查 locale 名称
    QString localeName = systemLocale.name();
    if (supportedLanguages.contains(localeName)) {
        QString detectedLang = supportedLanguages.value(localeName);
        qDebug() << "Detected language from locale (exact match):" << localeName << "->" << detectedLang;
        return detectedLang;
    }

    // 检查 locale 的基础语言
    QString baseLocaleLang = localeName.split('_').first();
    if (supportedLanguages.contains(baseLocaleLang)) {
        QString detectedLang = supportedLanguages.value(baseLocaleLang);
        qDebug() << "Detected language from locale (base match):" << localeName << "->" << detectedLang;
        return detectedLang;
    }

    // 默认使用英语
    qDebug() << "No supported language detected, defaulting to English";
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