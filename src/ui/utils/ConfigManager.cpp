#include "ConfigManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace EasyKiConverter
{

    ConfigManager::ConfigManager(QObject *parent)
        : QObject(parent), m_settings(nullptr)
    {
        // 创建 QSettings 对象
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configPath);

        QString configFile = configPath + "/config.ini";
        m_settings = new QSettings(configFile, QSettings::IniFormat, this);

        // 初始化默认配�?
        initDefaults();

        // 加载配置
        load();
    }

    ConfigManager::~ConfigManager()
    {
        if (m_settings)
        {
            delete m_settings;
        }
    }

    bool ConfigManager::save()
    {
        try
        {
            // 保存导出路径
            QJsonValue exportPathValue = m_config.value("export_path");
            m_settings->setValue("export/export_path", exportPathValue.isUndefined() ? "" : exportPathValue.toString());

            QJsonValue libNameValue = m_config.value("lib_name");
            m_settings->setValue("export/lib_name", libNameValue.isUndefined() ? "easyeda_convertlib" : libNameValue.toString());

            // 保存导出选项
            QJsonValue exportOptionsValue = m_config.value("export_options");
            QJsonObject exportOptions = exportOptionsValue.isUndefined() ? QJsonObject() : exportOptionsValue.toObject();

            QJsonValue symbolValue = exportOptions.value("symbol");
            m_settings->setValue("export/export_symbol", symbolValue.isUndefined() ? true : symbolValue.toBool());

            QJsonValue footprintValue = exportOptions.value("footprint");
            m_settings->setValue("export/export_footprint", footprintValue.isUndefined() ? true : footprintValue.toBool());

            QJsonValue model3dValue = exportOptions.value("model3d");
            m_settings->setValue("export/export_3d_model", model3dValue.isUndefined() ? true : model3dValue.toBool());

            // 同步到磁�?
            m_settings->sync();

            qDebug() << "Configuration saved successfully";
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Failed to save configuration:" << e.what();
            return false;
        }
    }

    bool ConfigManager::load()
    {
        try
        {
            // 加载导出路径
            m_config["export_path"] = m_settings->value("export/export_path", "").toString();
            m_config["lib_name"] = m_settings->value("export/lib_name", "easyeda_convertlib").toString();

            // 加载导出选项
            QJsonObject exportOptions;
            exportOptions["symbol"] = m_settings->value("export/export_symbol", true).toBool();
            exportOptions["footprint"] = m_settings->value("export/export_footprint", true).toBool();
            exportOptions["model3d"] = m_settings->value("export/export_3d_model", true).toBool();
            m_config["export_options"] = exportOptions;

            qDebug() << "Configuration loaded successfully";
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Failed to load configuration:" << e.what();
            return false;
        }
    }

    QJsonObject ConfigManager::getConfig() const
    {
        return m_config;
    }

    void ConfigManager::setConfig(const QJsonObject &config)
    {
        m_config = config;
        emit configChanged();
    }

    QString ConfigManager::getExportPath() const
    {
        QJsonValue value = m_config.value("export_path");
        return value.isUndefined() ? "" : value.toString();
    }

    void ConfigManager::setExportPath(const QString &path)
    {
        m_config["export_path"] = path;
        emit configChanged();
    }

    QString ConfigManager::getLibName() const
    {
        QJsonValue value = m_config.value("lib_name");
        return value.isUndefined() ? "easyeda_convertlib" : value.toString();
    }

    void ConfigManager::setLibName(const QString &name)
    {
        m_config["lib_name"] = name;
        emit configChanged();
    }

    QJsonObject ConfigManager::getExportOptions() const
    {
        QJsonValue value = m_config.value("export_options");
        return value.isUndefined() ? QJsonObject() : value.toObject();
    }

    void ConfigManager::setExportOptions(const QJsonObject &options)
    {
        m_config["export_options"] = options;
        emit configChanged();
    }

    void ConfigManager::resetToDefaults()
    {
        initDefaults();
        emit configChanged();
    }

    void ConfigManager::initDefaults()
    {
        // 设置默认导出路径
        m_config["export_path"] = "";

        // 设置默认库名�?
        m_config["lib_name"] = "easyeda_convertlib";

        // 设置默认导出选项
        QJsonObject exportOptions;
        exportOptions["symbol"] = true;
        exportOptions["footprint"] = true;
        exportOptions["model3d"] = true;
        m_config["export_options"] = exportOptions;

        qDebug() << "Default configuration initialized";
    }

} // namespace EasyKiConverter
