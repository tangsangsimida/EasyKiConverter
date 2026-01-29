#include "ConfigService.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

namespace EasyKiConverter {

// 静态成员初始化
ConfigService* ConfigService::s_instance = nullptr;
QMutex ConfigService::s_mutex;

ConfigService::ConfigService(QObject* parent) : QObject(parent) {
    // 初始化默认配�?
    initializeDefaultConfig();
}

ConfigService::~ConfigService() {
    // 自动保存配置
    saveConfig();
}

ConfigService* ConfigService::instance() {
    QMutexLocker locker(&s_mutex);

    if (!s_instance) {
        s_instance = new ConfigService();
    }

    return s_instance;
}

bool ConfigService::loadConfig(const QString& path) {
    QString configPath = path.isEmpty() ? getDefaultConfigPath() : path;

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << configPath;
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse config file:" << parseError.errorString();
        return false;
    }

    QMutexLocker locker(&m_configMutex);
    m_config = doc.object();
    m_configPath = configPath;

    qDebug() << "Config loaded from:" << configPath;
    emit configChanged();

    return true;
}

bool ConfigService::saveConfig(const QString& path) {
    QString configPath = path.isEmpty() ? (m_configPath.isEmpty() ? getDefaultConfigPath() : m_configPath) : path;

    QMutexLocker locker(&m_configMutex);

    QJsonDocument doc(m_config);

    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << configPath;
        return false;
    }

    file.write(doc.toJson());
    file.close();

    qDebug() << "Config saved to:" << configPath;
    return true;
}

void ConfigService::resetToDefaults() {
    QMutexLocker locker(&m_configMutex);
    initializeDefaultConfig();
    emit configChanged();
    
    // 释放锁后保存
    locker.unlock();
    saveConfig();

    qDebug() << "Config reset to defaults";
}

QString ConfigService::getOutputPath() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["outputPath"].toString(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
}

void ConfigService::setOutputPath(const QString& path) {
    QMutexLocker locker(&m_configMutex);
    m_config["outputPath"] = path;
    emit configChanged();

    // 释放锁后保存（因为 saveConfig 内部也会加锁，避免死锁）
    locker.unlock();
    saveConfig();
}

QString ConfigService::getLibName() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["libName"].toString("easyeda_convertlib");
}

void ConfigService::setLibName(const QString& name) {
    QMutexLocker locker(&m_configMutex);
    m_config["libName"] = name;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getExportSymbol() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportSymbol"].toBool(true);
}

void ConfigService::setExportSymbol(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportSymbol"] = enabled;
    emit configChanged();
    
    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getExportFootprint() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportFootprint"].toBool(true);
}

void ConfigService::setExportFootprint(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportFootprint"] = enabled;
    emit configChanged();
    
    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getExportModel3D() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportModel3D"].toBool(true);
}

void ConfigService::setExportModel3D(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportModel3D"] = enabled;
    emit configChanged();
    
    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getOverwriteExistingFiles() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["overwriteExistingFiles"].toBool(false);
}

void ConfigService::setOverwriteExistingFiles(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["overwriteExistingFiles"] = enabled;
    emit configChanged();
    
    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getDarkMode() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["darkMode"].toBool(false);
}

void ConfigService::setDarkMode(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["darkMode"] = enabled;
    emit configChanged();
}

bool ConfigService::getDebugMode() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["debugMode"].toBool(false);
}

void ConfigService::setDebugMode(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["debugMode"] = enabled;
    emit configChanged();
    
    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

void ConfigService::initializeDefaultConfig() {
    m_config["outputPath"] = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    m_config["libName"] = "easyeda_convertlib";
    m_config["exportSymbol"] = true;
    m_config["exportFootprint"] = true;
    m_config["exportModel3D"] = true;
    m_config["overwriteExistingFiles"] = false;
    m_config["darkMode"] = false;
    m_config["debugMode"] = false;
}

QString ConfigService::getDefaultConfigPath() const {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(configDir);

    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return configDir + "/config.json";
}

}  // namespace EasyKiConverter
