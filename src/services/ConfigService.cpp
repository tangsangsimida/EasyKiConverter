#include "ConfigService.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

namespace EasyKiConverter {

namespace {
QString defaultCacheDir() {
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache");
}
}  // namespace

// 静态成员初始化
ConfigService* ConfigService::s_instance = nullptr;
QMutex ConfigService::s_mutex;

ConfigService::ConfigService(QObject* parent) : QObject(parent) {
    // 初始化默认配
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
        qWarning() << "Config file not found, using defaults:" << configPath;
        saveConfig();
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

    // 合并配置：只更新文件中存在的字段，保留文件中缺失字段的默认值
    QJsonObject loadedConfig = doc.object();
    for (auto it = loadedConfig.begin(); it != loadedConfig.end(); ++it) {
        m_config[it.key()] = it.value();
    }

    // 移除 debugMode 字段，因为它不应该从配置文件加载
    // 调试模式完全由命令行参数和环境变量控制
    m_config.remove("debugMode");

    m_configPath = configPath;

    qDebug() << "Config loaded from:" << configPath;
    emit configChanged();

    // 释放锁后保存，确保补全缺失的字段到文件中（但不包含 debugMode）
    locker.unlock();
    saveConfig();

    return true;
}

bool ConfigService::saveConfig(const QString& path) {
    QString configPath = path.isEmpty() ? (m_configPath.isEmpty() ? getDefaultConfigPath() : m_configPath) : path;

    QMutexLocker locker(&m_configMutex);

    // 创建配置的副本，移除 debugMode 字段
    QJsonObject configToSave = m_config;
    configToSave.remove("debugMode");

    QJsonDocument doc(configToSave);

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

int ConfigService::getExportModel3DFormat() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportModel3DFormat"].toInt(3);  // 默认 3=Both (WRL+STEP)
}

void ConfigService::setExportModel3DFormat(int format) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportModel3DFormat"] = format;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getExportPreviewImages() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportPreviewImages"].toBool(false);
}

void ConfigService::setExportPreviewImages(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportPreviewImages"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getExportDatasheet() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exportDatasheet"].toBool(false);
}

void ConfigService::setExportDatasheet(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["exportDatasheet"] = enabled;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getWeakNetworkSupport() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["weakNetworkSupport"].toBool(false);
}

void ConfigService::setWeakNetworkSupport(bool enabled) {
    QMutexLocker locker(&m_configMutex);
    m_config["weakNetworkSupport"] = enabled;
    emit configChanged();

    locker.unlock();
    saveConfig();
}

int ConfigService::getValidationConcurrentCount() const {
    return getWeakNetworkSupport() ? 5 : 10;
}

int ConfigService::getPreviewConcurrentCount() const {
    return getWeakNetworkSupport() ? 3 : 6;
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

    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

bool ConfigService::getDebugMode() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["debugMode"].toBool(false);
}

void ConfigService::setDebugMode(bool enabled, bool save) {
    QMutexLocker locker(&m_configMutex);
    m_config["debugMode"] = enabled;
    emit configChanged();

    // 注意：调试模式不应该保存到配置文件
    // 它完全由命令行参数和环境变量控制
    // 如果 save=true，我们需要从配置中移除 debugMode 字段
    if (save) {
        m_config.remove("debugMode");
        locker.unlock();
        saveConfig();
    }
}

QVariantMap ConfigService::buildWindowState_locked() const {
    return {
        {"x", m_config["windowX"].toInt(DEFAULT_WINDOW_X)},
        {"y", m_config["windowY"].toInt(DEFAULT_WINDOW_Y)},
        {"width", m_config["windowWidth"].toInt(DEFAULT_WINDOW_WIDTH)},
        {"height", m_config["windowHeight"].toInt(DEFAULT_WINDOW_HEIGHT)},
        {"maximized", m_config["windowMaximized"].toBool(false)},
    };
}

QVariantMap ConfigService::getWindowState() const {
    QMutexLocker locker(&m_configMutex);
    return buildWindowState_locked();
}

void ConfigService::setWindowState(const QVariantMap& state) {
    QMutexLocker locker(&m_configMutex);
    const QVariantMap currentState = buildWindowState_locked();

    m_config["windowX"] = state.value("x", currentState.value("x")).toInt();
    m_config["windowY"] = state.value("y", currentState.value("y")).toInt();
    m_config["windowWidth"] = state.value("width", currentState.value("width")).toInt();
    m_config["windowHeight"] = state.value("height", currentState.value("height")).toInt();
    m_config["windowMaximized"] = state.value("maximized", currentState.value("maximized")).toBool();
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

QString ConfigService::getExitPreference() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["exitPreference"].toString("");
}

void ConfigService::setExitPreference(const QString& preference) {
    QMutexLocker locker(&m_configMutex);
    m_config["exitPreference"] = preference;
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
    m_config["exportModel3DFormat"] = 3;  // 默认 3=Both (WRL+STEP)
    m_config["exportPreviewImages"] = false;
    m_config["exportDatasheet"] = false;
    m_config["overwriteExistingFiles"] = false;
    m_config["weakNetworkSupport"] = false;
    m_config["darkMode"] = false;
    // 注意：debugMode 不在这里设置，因为它不应该保存在配置文件中
    // 它完全由命令行参数和环境变量控制

    // 窗口配置默认值（-1 或 -9999 表示使用默认值）
    m_config["windowWidth"] = DEFAULT_WINDOW_WIDTH;
    m_config["windowHeight"] = DEFAULT_WINDOW_HEIGHT;
    m_config["windowX"] = DEFAULT_WINDOW_X;
    m_config["windowY"] = DEFAULT_WINDOW_Y;
    m_config["windowMaximized"] = false;
    // 退出偏好默认值（空字符串表示未记住）
    m_config["exitPreference"] = "";
    // 语言设置默认值（英文）
    m_config["language"] = "en";
    m_config["cacheDir"] = defaultCacheDir();
    m_config["diskCacheLimitMB"] = DEFAULT_DISK_CACHE_LIMIT_MB;
}

QString ConfigService::getDefaultConfigPath() const {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(configDir);

    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return configDir + "/config.json";
}

QString ConfigService::getLanguage() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["language"].toString("en");
}

void ConfigService::setLanguage(const QString& languageCode) {
    QMutexLocker locker(&m_configMutex);
    m_config["language"] = languageCode;
    emit configChanged();

    // 释放锁后保存
    locker.unlock();
    saveConfig();
}

QString ConfigService::getCacheDir() const {
    QMutexLocker locker(&m_configMutex);
    return m_config["cacheDir"].toString(defaultCacheDir());
}

void ConfigService::setCacheDir(const QString& path) {
    const QString normalizedPath = QDir::cleanPath(path);

    QMutexLocker locker(&m_configMutex);
    m_config["cacheDir"] = normalizedPath;
    emit configChanged();

    locker.unlock();
    saveConfig();
}

int ConfigService::getDiskCacheLimitMB() const {
    QMutexLocker locker(&m_configMutex);
    return qBound(1, m_config["diskCacheLimitMB"].toInt(DEFAULT_DISK_CACHE_LIMIT_MB), 1048576);
}

void ConfigService::setDiskCacheLimitMB(int maxSizeMB) {
    QMutexLocker locker(&m_configMutex);
    m_config["diskCacheLimitMB"] = qBound(1, maxSizeMB, 1048576);
    emit configChanged();

    locker.unlock();
    saveConfig();
}

}  // namespace EasyKiConverter

// 考试没过怎么办？别急别急，你先双击你的太阳穴打开你的个人面板，
// 找到你的上一个存档，直接读档，你就回到考试之前了。
// 但是注意啊兄弟们，每天睡觉会自动存档的，而且同时只能存在两个存档，
// 别不小心睡过去了给弄死档了，不然就只能等考试活动返场了