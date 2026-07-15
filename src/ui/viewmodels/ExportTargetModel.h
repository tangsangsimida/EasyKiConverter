#pragma once

#include <QObject>
#include <QVariantList>

namespace EasyKiConverter {

/**
 * @brief 导出目标模型
 * @details 管理可用目标列表、当前选择、目标元数据。
 *          从 export_plugins.json 读取配置，支持运行时动态切换目标格式。
 */
class ExportTargetModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentTargetChanged)
    Q_PROPERTY(QString currentTargetId READ currentTargetId NOTIFY currentTargetChanged)
    Q_PROPERTY(QString currentDisplayName READ currentDisplayName NOTIFY currentTargetChanged)
    Q_PROPERTY(QString currentIcon READ currentIcon NOTIFY currentTargetChanged)
    Q_PROPERTY(QString currentOptionsComponent READ currentOptionsComponent NOTIFY currentTargetChanged)
    Q_PROPERTY(QVariantList availableTargets READ availableTargets NOTIFY availableTargetsChanged)

public:
    explicit ExportTargetModel(QObject* parent = nullptr);
    ~ExportTargetModel() override = default;

    /** @brief 获取当前选中索引 */
    int currentIndex() const;

    /** @brief 设置当前选中索引 */
    void setCurrentIndex(int index);

    /** @brief 获取当前目标 ID（如 "kicad", "altium"） */
    QString currentTargetId() const;

    /** @brief 获取当前目标显示名称 */
    QString currentDisplayName() const;

    /** @brief 获取当前目标图标资源名 */
    QString currentIcon() const;

    /** @brief 获取当前目标对应的 QML 设置组件文件名 */
    QString currentOptionsComponent() const;

    /** @brief 获取所有可用目标列表（供 QML ComboBox model 使用） */
    QVariantList availableTargets() const;

    /**
     * @brief 加载插件配置
     * @param configPath JSON 配置文件路径，默认从 Qt 资源加载
     */
    void loadPlugins(const QString& configPath = ":/resources/export_plugins.json");

signals:
    /** @brief 当前目标变更 */
    void currentTargetChanged();

    /** @brief 可用目标列表变更 */
    void availableTargetsChanged();

private:
    /** @brief 目标信息结构 */
    struct TargetInfo {
        QString id;               ///< 目标标识符
        QString displayName;      ///< 显示名称
        QString icon;             ///< 图标资源名
        QString optionsComponent; ///< QML 设置组件文件名
    };

    QList<TargetInfo> m_targets;  ///< 可用目标列表
    int m_currentIndex = 0;       ///< 当前选中索引
};

}  // namespace EasyKiConverter
