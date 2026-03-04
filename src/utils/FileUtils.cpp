#include "FileUtils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_WIN)
#    include <QProcess>
#elif defined(Q_OS_LINUX)
#    include <QProcess>
#elif defined(Q_OS_MAC)
#    include <QProcess>
#endif

namespace EasyKiConverter {

FileUtils::FileUtils(QObject* parent) : QObject(parent) {}

bool FileUtils::openFolder(const QString& path) {
    if (path.isEmpty()) {
        qWarning() << "FileUtils::openFolder: Path is empty";
        return false;
    }

    // 转换为绝对路径
    QString absolutePath = toAbsolutePath(path);

    // 检查路径是否存在
    if (!pathExists(absolutePath)) {
        qWarning() << "FileUtils::openFolder: Path does not exist:" << absolutePath;
        return false;
    }

    qDebug() << "FileUtils::openFolder: Opening folder:" << absolutePath;

#if defined(Q_OS_WIN)
    return openFolderOnWindows(absolutePath);
#elif defined(Q_OS_LINUX)
    return openFolderOnLinux(absolutePath);
#elif defined(Q_OS_MAC)
    return openFolderOnMacOS(absolutePath);
#else
    qWarning() << "FileUtils::openFolder: Unsupported platform";
    return false;
#endif
}

QString FileUtils::toAbsolutePath(const QString& path) {
    if (path.isEmpty()) {
        return "";
    }

    QDir dir(path);
    if (dir.isAbsolute()) {
        // 已经是绝对路径，规范化路径分隔符
        return dir.cleanPath(path);
    }

    // 相对路径，转换为绝对路径
    QString absolutePath = dir.absolutePath();
    qDebug() << "FileUtils::toAbsolutePath: Converting relative path" << path << "to" << absolutePath;
    return absolutePath;
}

bool FileUtils::pathExists(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }

    QDir dir(path);
    return dir.exists();
}

bool FileUtils::openFolderOnWindows(const QString& absolutePath) {
#ifdef Q_OS_WIN
    // 使用 explorer 命令打开文件夹
    QStringList arguments;
    arguments << "/select,";  // /select 参数可以同时选择文件夹
    arguments << QDir::toNativeSeparators(absolutePath);

    qDebug() << "FileUtils::openFolderOnWindows: Running explorer with arguments:" << arguments;

    QProcess process;
    bool success = process.startDetached("explorer", arguments);

    if (!success) {
        qWarning() << "FileUtils::openFolderOnWindows: Failed to start explorer, trying QDesktopServices fallback";
        // 降级方案：使用 QDesktopServices
        QUrl url = QUrl::fromLocalFile(absolutePath);
        return QDesktopServices::openUrl(url);
    }

    return success;
#else
    Q_UNUSED(absolutePath);
    return false;
#endif
}

bool FileUtils::openFolderOnLinux(const QString& absolutePath) {
#ifdef Q_OS_LINUX
    // 使用 xdg-open 命令打开文件夹
    QStringList arguments;
    arguments << absolutePath;

    qDebug() << "FileUtils::openFolderOnLinux: Running xdg-open with arguments:" << arguments;

    QProcess process;
    bool success = process.startDetached("xdg-open", arguments);

    if (!success) {
        qWarning() << "FileUtils::openFolderOnLinux: Failed to start xdg-open, trying QDesktopServices fallback";
        // 降级方案：使用 QDesktopServices
        QUrl url = QUrl::fromLocalFile(absolutePath);
        return QDesktopServices::openUrl(url);
    }

    return success;
#else
    Q_UNUSED(absolutePath);
    return false;
#endif
}

bool FileUtils::openFolderOnMacOS(const QString& absolutePath) {
#ifdef Q_OS_MAC
    // 使用 open 命令打开文件夹
    QStringList arguments;
    arguments << absolutePath;

    qDebug() << "FileUtils::openFolderOnMacOS: Running open with arguments:" << arguments;

    QProcess process;
    bool success = process.startDetached("open", arguments);

    if (!success) {
        qWarning() << "FileUtils::openFolderOnMacOS: Failed to start open, trying QDesktopServices fallback";
        // 降级方案：使用 QDesktopServices
        QUrl url = QUrl::fromLocalFile(absolutePath);
        return QDesktopServices::openUrl(url);
    }

    return success;
#else
    Q_UNUSED(absolutePath);
    return false;
#endif
}

}  // namespace EasyKiConverter