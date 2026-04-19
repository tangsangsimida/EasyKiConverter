#ifndef CONFIGSERVICE_H
#define CONFIGSERVICE_H

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariantMap>

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
     * @brief 获取3D模型格式
     *
     * @return int 3D模型格式(位掩码): 0=NONE, 1=WRL, 2=STEP, 3=BOTH
     */
    int getExportModel3DFormat() const;

    /**
     * @brief 设置3D模型格式
     *
     * @param format 3D模型格式(位掩码): 0=NONE, 1=WRL, 2=STEP, 3=BOTH
     */
    void setExportModel3DFormat(int format);

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
     * @brief 是否启用客户端弱网络适配
     *
     * @return bool 是否启用客户端弱网络适配
     */
    bool getWeakNetworkSupport() const;

    /**
     * @brief 设置是否启用客户端弱网络适配
     *
     * @param enabled 是否启用
     */
    void setWeakNetworkSupport(bool enabled);

    /**
     * @brief 获取验证阶段的并发数
     *
     * @return int 并发数
     */
    int getValidationConcurrentCount() const;

    /**
     * @brief 获取预览图阶段的并发数
     *
     * @return int 并发数
     */
    int getPreviewConcurrentCount() const;

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
     * @param save 是否保存到配置文件（默认 true）
     */
    void setDebugMode(bool enabled, bool save = true);

    /**
     * @brief 获取窗口状态值对象
     *
     * @return QVariantMap 包含 x/y/width/height/maximized 字段
     */
    Q_INVOKABLE QVariantMap getWindowState() const;

    /**
     * @brief 设置窗口状态值对象
     *
     * @param state 包含 x/y/width/height/maximized 字段的值对象
     */
    Q_INVOKABLE void setWindowState(const QVariantMap& state);

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

    /**
     * @brief 获取语言设置
     *
     * @return QString 语言代码（"zh_CN", "en" 等）
     */
    Q_INVOKABLE QString getLanguage() const;

    /**
     * @brief 设置语言
     *
     * @param languageCode 语言代码（"zh_CN", "en" 等）
     */
    Q_INVOKABLE void setLanguage(const QString& languageCode);

signals:
    /**
     * @brief 配置改变信号
     */
    void configChanged();

private:
    static constexpr int DEFAULT_WINDOW_WIDTH = -1;
    static constexpr int DEFAULT_WINDOW_HEIGHT = -1;
    static constexpr int DEFAULT_WINDOW_X = -9999;
    static constexpr int DEFAULT_WINDOW_Y = -9999;

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
    QVariantMap buildWindowState_locked() const;

private:
    static ConfigService* s_instance;
    static QMutex s_mutex;

    QJsonObject m_config;
    QString m_configPath;
    mutable QMutex m_configMutex;
};

}  // namespace EasyKiConverter

#endif  // CONFIGSERVICE_H
