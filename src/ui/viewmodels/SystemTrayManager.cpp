#include "SystemTrayManager.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QWindow>

namespace EasyKiConverter {

SystemTrayManager::SystemTrayManager(QObject* parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_contextMenu(nullptr)
    , m_appName(QStringLiteral("EasyKiConverter"))
    , m_initialized(false) {}

SystemTrayManager::~SystemTrayManager() {
    if (m_trayIcon) {
        m_trayIcon->hide();
        if (m_contextMenu) {
            m_trayIcon->setContextMenu(nullptr);
            m_contextMenu->deleteLater();
            m_contextMenu = nullptr;
        }
        m_trayIcon->deleteLater();
        m_trayIcon = nullptr;
    }
}

void SystemTrayManager::initialize() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug() << "System tray is not available on this platform";
        return;
    }

    qDebug() << "Initializing system tray icon...";

    m_trayIcon = new QSystemTrayIcon(this);

    QIcon appIcon;
    QStringList iconPaths = {":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.ico",
                             ":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png",
                             ":/resources/icons/app_icon.ico",
                             ":/resources/icons/app_icon.png",
                             "resources/icons/app_icon.ico",
                             "resources/icons/app_icon.png"};

    for (const QString& path : iconPaths) {
        appIcon = QIcon(path);
        if (!appIcon.isNull()) {
            qDebug() << "System tray icon loaded successfully from:" << path;
            break;
        }
    }

    if (appIcon.isNull()) {
        qWarning() << "Failed to load icon from all paths, using default icon";
        appIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        if (appIcon.isNull()) {
            qCritical() << "Failed to load even default icon";
            return;
        }
    }

    m_trayIcon->setIcon(appIcon);
    qDebug() << "System tray icon set successfully";

    m_trayIcon->setToolTip(QStringLiteral("EasyKiConverter - LCSC 转换工具"));

    setupContextMenu();
    setupConnections();

    m_trayIcon->show();

    if (m_trayIcon->isVisible()) {
        qDebug() << "System tray icon is visible";
    } else {
        qWarning() << "System tray icon is not visible after show()";
    }

    m_initialized = true;
    qDebug() << "System tray icon initialized and shown";
}

void SystemTrayManager::setupContextMenu() {
    m_contextMenu = new QMenu();
    m_contextMenu->setWindowFlags(m_contextMenu->windowFlags() | Qt::Popup);

    QAction* showAction = m_contextMenu->addAction(QStringLiteral("显示窗口"));
    connect(showAction, &QAction::triggered, this, [this]() {
        qDebug() << "Show window action triggered";
        QTimer::singleShot(0, m_contextMenu, &QMenu::close);
        emit showWindowRequested();
    });

    QAction* quitAction = m_contextMenu->addAction(QStringLiteral("退出"));
    connect(quitAction, &QAction::triggered, this, [this]() {
        qDebug() << "Quit action triggered";
        QTimer::singleShot(0, m_contextMenu, &QMenu::close);
        emit quitRequested();
    });

    m_trayIcon->setContextMenu(m_contextMenu);
}

void SystemTrayManager::setupConnections() {
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            qDebug() << "System tray icon triggered (left click/double click)";
            emit showWindowRequested();
        }
    });
}

void SystemTrayManager::showExportNotification(int successCount,
                                               int failureCount,
                                               int symbolCount,
                                               int footprintCount,
                                               int model3DCount,
                                               const QString& duration) {
    if (!m_trayIcon) {
        qDebug() << "System tray icon not initialized";
        return;
    }

    if (!QSystemTrayIcon::supportsMessages()) {
        qDebug() << "System tray does not support messages on this platform";
        return;
    }

    QString title;
    QString message;
    QSystemTrayIcon::MessageIcon iconType = QSystemTrayIcon::Information;

    if (failureCount > 0) {
        title = QStringLiteral("导出完成");

        double successRate = 0.0;
        if (successCount + failureCount > 0) {
            successRate = (static_cast<double>(successCount) * 100.0) / (successCount + failureCount);
        }

        if (successCount == 0) {
            iconType = QSystemTrayIcon::Critical;
            message = QStringLiteral("导出失败：%1 个元器件全部失败").arg(successCount + failureCount);
        } else if (successRate < 50.0) {
            iconType = QSystemTrayIcon::Warning;
            message = QStringLiteral("成功 %1 个，失败 %2 个").arg(successCount).arg(failureCount);
        } else {
            message = QStringLiteral("成功 %1 个，失败 %2 个").arg(successCount).arg(failureCount);
        }

        if (successCount > 0) {
            message += QStringLiteral("\n输出：符号 %1 · 封装 %2 · 3D %3")
                           .arg(symbolCount)
                           .arg(footprintCount)
                           .arg(model3DCount);
        }
    } else {
        iconType = QSystemTrayIcon::Information;
        title = QStringLiteral("导出完成");

        if (successCount == 1) {
            message = QStringLiteral("成功导出 1 个元器件");
        } else {
            message = QStringLiteral("成功导出 %1 个元器件").arg(successCount);
        }

        QStringList stats;
        stats
            << QStringLiteral("输出：符号 %1 · 封装 %2 · 3D %3").arg(symbolCount).arg(footprintCount).arg(model3DCount);

        if (!duration.isEmpty()) {
            stats << QStringLiteral("耗时：%1").arg(duration);
        }

        message += "\n" + stats.join("\n");
    }

    qDebug() << "Attempting to show notification:" << title << message;
    m_trayIcon->showMessage(title, message, iconType, 12000);
    qDebug() << "Notification shown successfully";
}

bool SystemTrayManager::isAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool SystemTrayManager::isVisible() const {
    return m_trayIcon && m_trayIcon->isVisible();
}

}  // namespace EasyKiConverter