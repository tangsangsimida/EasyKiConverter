#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QSettings>
#include <QString>
#include <QTranslator>

namespace EasyKiConverter {

/**
 * @brief 语言管理器
 *
 * 负责管理应用程序的多语言支持
 * - 自动检测系统语言
 * - 支持手动切换语言
 * - 持久化语言设置
 * - 动态加载翻译文件
 */
class LanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY languageChanged)

public:
    /**
     * @brief 获取单例实例
     * @return LanguageManager 单例
     */
    static LanguageManager* instance();

    /**
     * @brief 设置语言
     * @param languageCode 语言代码 ("auto", "zh_CN", "en")
     */
    Q_INVOKABLE void setLanguage(const QString& languageCode);

    /**
     * @brief 获取当前语言代码
     * @return 当前语言代码
     */
    Q_INVOKABLE QString currentLanguage() const;

    /**
     * @brief 检测系统语言
     * @return 检测到的语言代码
     */
    Q_INVOKABLE QString detectSystemLanguage() const;

signals:
    /**
     * @brief 语言改变信号
     * @param language 新的语言代码
     */
    void languageChanged(const QString& language);

    /**
     * @brief 请求刷新界面信号
     *
     * 当语言切换时发出此信号，通知 QML 刷新界面
     */
    void refreshRequired();

private:
    explicit LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() override;

    void loadLanguageSettings();
    void saveLanguageSettings();
    void installTranslator(const QString& languageCode);

    static LanguageManager* s_instance;
    QString m_currentLanguage;
    QSettings* m_settings;
    QTranslator* m_translator;
};

}  // namespace EasyKiConverter

#endif  // LANGUAGEMANAGER_H