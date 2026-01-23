#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QSettings>

namespace EasyKiConverter
{

    /**
     * @brief 配置管理�?
     *
     * 管理应用程序的配置信息，包括导出路径、库名称、导出选项�?
     */
    class ConfigManager : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函�?
         *
         * @param parent 父对�?
         */
        explicit ConfigManager(QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~ConfigManager() override;

        /**
         * @brief 保存配置
         *
         * @return bool 保存是否成功
         */
        bool save();

        /**
         * @brief 加载配置
         *
         * @return bool 加载是否成功
         */
        bool load();

        /**
         * @brief 获取配置对象
         *
         * @return QJsonObject 配置对象
         */
        QJsonObject getConfig() const;

        /**
         * @brief 设置配置对象
         *
         * @param config 配置对象
         */
        void setConfig(const QJsonObject &config);

        /**
         * @brief 获取导出路径
         *
         * @return QString 导出路径
         */
        QString getExportPath() const;

        /**
         * @brief 设置导出路径
         *
         * @param path 导出路径
         */
        void setExportPath(const QString &path);

        /**
         * @brief 获取库名�?
         *
         * @return QString 库名�?
         */
        QString getLibName() const;

        /**
         * @brief 设置库名�?
         *
         * @param name 库名�?
         */
        void setLibName(const QString &name);

        /**
         * @brief 获取导出选项
         *
         * @return QJsonObject 导出选项
         */
        QJsonObject getExportOptions() const;

        /**
         * @brief 设置导出选项
         *
         * @param options 导出选项
         */
        void setExportOptions(const QJsonObject &options);

        /**
         * @brief 重置配置为默认�?
         */
        void resetToDefaults();

    signals:
        /**
         * @brief 配置已更改信�?
         */
        void configChanged();

    private:
        /**
         * @brief 初始化默认配�?
         */
        void initDefaults();

    private:
        QSettings *m_settings;
        QJsonObject m_config;
    };

} // namespace EasyKiConverter

#endif // CONFIGMANAGER_H
