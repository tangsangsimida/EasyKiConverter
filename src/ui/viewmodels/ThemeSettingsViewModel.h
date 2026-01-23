#ifndef THEMESETTINGSVIEWMODEL_H
#define THEMESETTINGSVIEWMODEL_H

#include <QObject>
#include "services/ConfigService.h"

namespace EasyKiConverter
{

    /**
     * @brief 主题设置视图模型�?
     *
     * 负责管理主题设置相关�?UI 状�?
     */
    class ThemeSettingsViewModel : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setDarkMode NOTIFY darkModeChanged)

    public:
        explicit ThemeSettingsViewModel(QObject *parent = nullptr);
        ~ThemeSettingsViewModel() override;

        // Getter 方法
        bool isDarkMode() const { return m_isDarkMode; }

        // Setter 方法（标记为 Q_INVOKABLE 以便�?QML 中调用）
        Q_INVOKABLE void setDarkMode(bool darkMode);

    signals:
        void darkModeChanged();

    private:
        ConfigService *m_configService;
        bool m_isDarkMode;
    };

} // namespace EasyKiConverter

#endif // THEMESETTINGSVIEWMODEL_H
