#include "LanguageManager.h"

#include "src/services/ConfigService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QTranslator>

namespace EasyKiConverter {

LanguageManager* LanguageManager::s_instance = nullptr;

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_currentLanguage("en")
    , m_translator(nullptr)
    , m_isInitializing(true)
    , m_loadedFromConfig(false) {
    loadLanguageSettings();

    // 延迟重置初始化标志，给 QML 组件时间完成加载
    QTimer::singleShot(500, this, [this]() {
        m_isInitializing = false;
        qInfo() << "[LanguageManager] Initialization protection period ended";
    });
}

LanguageManager::~LanguageManager() {}

LanguageManager* LanguageManager::instance() {
    if (!s_instance) {
        s_instance = new LanguageManager();
    }
    return s_instance;
}

void LanguageManager::setLanguage(const QString& languageCode) {
    qInfo() << "[LanguageManager] setLanguage() called with:" << languageCode << " (current:" << m_currentLanguage
            << ", initializing:" << m_isInitializing << ", loadedFromConfig:" << m_loadedFromConfig << ")";

    if (m_currentLanguage == languageCode) {
        qInfo() << "[LanguageManager] Language unchanged, returning";

        return;
    }

    // 如果正在初始化且已经从配置加载了语言，则拒绝覆盖

    // 这样可以防止 QML 组件在初始化时覆盖配置文件的语言设置

    if (m_isInitializing && m_loadedFromConfig) {
        qWarning() << "[LanguageManager] Refusing to override language during initialization after loading from "
                      "config. Requested:"
                   << languageCode << ", Keeping:" << m_currentLanguage;

        return;
    }

    m_currentLanguage = languageCode;

    qInfo() << "[LanguageManager] Language changed to" << m_currentLanguage;

    // 只在非初始化期间保存设置

    if (!m_isInitializing) {
        saveLanguageSettings();

    } else {
        qInfo() << "[LanguageManager] Skipping save during initialization";
    }

    // 安装新的翻译器

    installTranslator(languageCode);

    emit languageChanged(languageCode);

    emit refreshRequired();

    qInfo() << "[LanguageManager] Language changed signal emitted:" << languageCode;
}

QString LanguageManager::currentLanguage() const {
    return m_currentLanguage;
}

QString LanguageManager::detectSystemLanguage() const {
    // 不再使用系统语言检测，直接返回英文
    return "en";
}

void LanguageManager::loadLanguageSettings() {
    // 从 ConfigService 加载语言设置
    auto* configService = ConfigService::instance();
    if (configService) {
        m_currentLanguage = configService->getLanguage();
        qInfo() << "[LanguageManager] Loaded language settings from ConfigService:" << m_currentLanguage;
    } else {
        qWarning() << "[LanguageManager] ConfigService not available, using default language: en";
        m_currentLanguage = "en";
    }

    m_loadedFromConfig = true;

    // 安装翻译器
    qInfo() << "[LanguageManager] Installing translator for language:" << m_currentLanguage;
    installTranslator(m_currentLanguage);
}

void LanguageManager::saveLanguageSettings() {
    // 保存到 ConfigService
    auto* configService = ConfigService::instance();
    if (configService) {
        configService->setLanguage(m_currentLanguage);
        qInfo() << "[LanguageManager] Saved language settings to ConfigService:" << m_currentLanguage;
    } else {
        qWarning() << "[LanguageManager] ConfigService not available, failed to save language settings";
    }
}

void LanguageManager::installTranslator(const QString& languageCode) {
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        qWarning() << "[LanguageManager] QCoreApplication instance not available";
        return;
    }

    // 如果已有翻译器，先移除并删除
    if (m_translator) {
        app->removeTranslator(m_translator);
        m_translator->deleteLater();
        m_translator = nullptr;
        qInfo() << "[LanguageManager] Removed previous translator";
    }

    // Qt 6 qml_module 资源路径 - 使用 compiled .qm files
    QString translationPath =
        QString(":/qt/qml/EasyKiconverter_Cpp_Version/resources/translations/translations_easykiconverter_%1.qm")
            .arg(languageCode);

    m_translator = new QTranslator(this);

    // 尝试加载翻译文件（.qm 格式）
    if (m_translator->load(translationPath)) {
        app->installTranslator(m_translator);
        qInfo() << "[LanguageManager] [OK] 已从资源加载翻译器:" << translationPath;
        return;
    }

    // 如果资源文件不存在，尝试从文件系统加载
    QString localPath = QString("resources/translations/translations_easykiconverter_%1.qm").arg(languageCode);
    if (m_translator->load(localPath)) {
        app->installTranslator(m_translator);
        qInfo() << "[LanguageManager] [OK] 已从文件系统加载翻译器:" << localPath;
        return;
    }

    qWarning() << "[LanguageManager] [FAIL] 无法加载翻译文件，语言:" << languageCode;
    qWarning() << "[LanguageManager] Tried paths:";
    qWarning() << "  1. Resource path:" << translationPath;
    qWarning() << "  2. File system path:" << localPath;

    // 加载失败时，清理创建的翻译器对象
    m_translator->deleteLater();
    m_translator = nullptr;
}

}  // namespace EasyKiConverter