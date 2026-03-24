#ifndef THEMESETTINGSVIEWMODEL_H
#define THEMESETTINGSVIEWMODEL_H

#include "services/ConfigService.h"

#include <QObject>

namespace EasyKiConverter {

class ThemeSettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setDarkMode NOTIFY darkModeChanged)

public:
    explicit ThemeSettingsViewModel(QObject* parent = nullptr);
    ~ThemeSettingsViewModel() override;

    bool isDarkMode() const {
        return m_isDarkMode;
    }

    Q_INVOKABLE void setDarkMode(bool darkMode);

signals:
    void darkModeChanged();

private:
    ConfigService* m_configService;
    bool m_isDarkMode;
};

}  // namespace EasyKiConverter

#endif  // THEMESETTINGSVIEWMODEL_H
