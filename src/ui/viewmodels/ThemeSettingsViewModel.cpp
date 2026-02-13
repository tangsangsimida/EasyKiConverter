#include "ThemeSettingsViewModel.h"

#include <QDebug>

namespace EasyKiConverter {

ThemeSettingsViewModel::ThemeSettingsViewModel(QObject* parent)
    : QObject(parent), m_configService(ConfigService::instance()), m_isDarkMode(false) {
    m_isDarkMode = m_configService->getDarkMode();
}

ThemeSettingsViewModel::~ThemeSettingsViewModel() {}

void ThemeSettingsViewModel::setDarkMode(bool darkMode) {
    if (m_isDarkMode != darkMode) {
        m_isDarkMode = darkMode;
        m_configService->setDarkMode(darkMode);
        emit darkModeChanged();
        qDebug() << "Dark mode changed to:" << darkMode;
    }
}

}  // namespace EasyKiConverter
