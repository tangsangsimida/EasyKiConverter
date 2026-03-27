#ifndef SYSTEMTRAYMANAGER_H
#define SYSTEMTRAYMANAGER_H

#include <QMenu>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

namespace EasyKiConverter {

class SystemTrayManager : public QObject {
    Q_OBJECT
public:
    explicit SystemTrayManager(QObject* parent = nullptr);
    ~SystemTrayManager();

    void initialize();
    void showExportNotification(int successCount,
                                int failureCount,
                                int symbolCount,
                                int footprintCount,
                                int model3DCount,
                                const QString& duration = QString());

    bool isAvailable() const;
    bool isVisible() const;

signals:
    void showWindowRequested();
    void quitRequested();

private:
    void setupTrayIcon();
    void setupContextMenu();
    void setupConnections();

private:
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_contextMenu;
    QString m_appName;
    bool m_initialized;
};

}  // namespace EasyKiConverter

#endif  // SYSTEMTRAYMANAGER_H