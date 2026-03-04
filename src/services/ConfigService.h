#ifndef CONFIGSERVICE_H
#define CONFIGSERVICE_H

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 配置服务
 *
 * 负责配置管理，使用单例模式
 * 不依赖任UI 组件
 */
class ConfigService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     *
     * @return ConfigService* 配置服务实例
     */
    static ConfigService* instance();

    /**
     * @brief 加载配置
     *
     * @param path 配置文件路径，为空时使用默认路径
     * @return bool 是否成功
     */
    bool loadConfig(const QString& path = QString());

    /**
     * @brief 保存配置
     *
     * @param path 配置文件路径，为空时使用默认路径
     * @return bool 是否成功
     */
    bool saveConfig(const QString& path = QString());

    /**
     * @brief 重置为默认配
     */
    void resetToDefaults();

    // 配置访问方法

    /**
     * @brief 获取输出路径
     *
     * @return QString 输出路径
     */
    QString getOutputPath() const;

    /**
     * @brief 设置输出路径
     *
     * @param path 输出路径
     */
    void setOutputPath(const QString& path);

    /**
     * @brief 获取库名称
     *
     * @return QString 库名称
     */
    QString getLibName() const;

    /**
     * @brief 设置库名称
     *
     * @param name 库名称
     */
    void setLibName(const QString& name);

    /**
     * @brief 是否导出符号
     *
     * @return bool 是否导出符号
     */
    bool getExportSymbol() const;

    /**
     * @brief 设置是否导出符号
     *
     * @param enabled 是否启用
     */
    void setExportSymbol(bool enabled);

    /**
     * @brief 是否导出封装
     *
     * @return bool 是否导出封装
     */
    bool getExportFootprint() const;

    /**
     * @brief 设置是否导出封装
     *
     * @param enabled 是否启用
     */
    void setExportFootprint(bool enabled);

    /**
     * @brief 是否导出3D模型
     *
     * @return bool 是否导出3D模型
     */
    bool getExportModel3D() const;

    /**
     * @brief 设置是否导出3D模型
     *
     * @param enabled 是否启用
     */
    void setExportModel3D(bool enabled);

    /**
     * @brief 是否导出预览图
     *
     * @return bool 是否导出预览图
     */
    bool getExportPreviewImages() const;

    /**
     * @brief 设置是否导出预览图
     *
     * @param enabled 是否启用
     */
    void setExportPreviewImages(bool enabled);

    /**
     * @brief 是否导出手册
     *
     * @return bool 是否导出手册
     */
    bool getExportDatasheet() const;

    /**
     * @brief 设置是否导出手册
     *
     * @param enabled 是否启用
     */
    void setExportDatasheet(bool enabled);

    /**
     * @brief 是否覆盖已存在文
     *
     * @return bool 是否覆盖
     */
    bool getOverwriteExistingFiles() const;

    /**
     * @brief 设置是否覆盖已存在文
     *
     * @param enabled 是否启用
     */
    void setOverwriteExistingFiles(bool enabled);

    /**
     * @brief 是否启用深色模式
     *
     * @return bool 是否启用深色模式
     */
    bool getDarkMode() const;

    /**
     * @brief 设置是否启用深色模式
     *
     * @param enabled 是否启用
     */
    void setDarkMode(bool enabled);

    /**
     * @brief 是否启用调试模式
     *
     * @return bool 是否启用调试模式
     */
    bool getDebugMode() const;

    /**
     * @brief 设置是否启用调试模式
     *
     * @param enabled 是否启用
     */
    void setDebugMode(bool enabled);

    /**
     * @brief 获取窗口宽度
     *
     * @return int 窗口宽度
     */
    Q_INVOKABLE int getWindowWidth() const;

    /**
     * @brief 设置窗口宽度
     *
     * @param width 窗口宽度
     */
    Q_INVOKABLE void setWindowWidth(int width);

    /**
     * @brief 获取窗口高度
     *
     * @return int 窗口高度
     */
    Q_INVOKABLE int getWindowHeight() const;

    /**
     * @brief 设置窗口高度
     *
     * @param height 窗口高度
     */
    Q_INVOKABLE void setWindowHeight(int height);

    /**
     * @brief 获取窗口X坐标
     *
     * @return int 窗口X坐标
     */
    Q_INVOKABLE int getWindowX() const;

    /**
     * @brief 设置窗口X坐标
     *
     * @param x 窗口X坐标
     */
    Q_INVOKABLE void setWindowX(int x);

    /**
     * @brief 获取窗口Y坐标
     *
     * @return int 窗口Y坐标
     */
    Q_INVOKABLE int getWindowY() const;

    /**
     * @brief 设置窗口Y坐标
     *
     * @param y 窗口Y坐标
     */
    Q_INVOKABLE void setWindowY(int y);

    /**
     * @brief 获取退出偏好
     *
     * @return QString 退出偏好（"minimize" 或 "exit"，空字符串表示未记住）
     */
    Q_INVOKABLE QString getExitPreference() const;

    /**
     * @brief 设置退出偏好
     *
     * @param preference 退出偏好（"minimize" 或 "exit"）
     */
    Q_INVOKABLE void setExitPreference(const QString& preference);

signals:
    /**
     * @brief 配置改变信号
     */
    void configChanged();

private:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    ConfigService(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ConfigService() override;

    /**
     * @brief 初始化默认配
     */
    void initializeDefaultConfig();

    /**
     * @brief 获取默认配置文件路径
     *
     * @return QString 配置文件路径
     */
    QString getDefaultConfigPath() const;

private:
    static ConfigService* s_instance;
    static QMutex s_mutex;

    QJsonObject m_config;
    QString m_configPath;
    mutable QMutex m_configMutex;
};

}  // namespace EasyKiConverter

#endif  // CONFIGSERVICE_H
