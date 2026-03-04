#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QObject>
#include <QProcess>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 文件操作工具类
 *
 * 提供跨平台的文件操作功能，包括打开文件夹、路径转换等
 */
class FileUtils : public QObject {
    Q_OBJECT

public:
    explicit FileUtils(QObject* parent = nullptr);
    ~FileUtils() = default;

    /**
     * @brief 打开文件夹（跨平台）
     * @param path 文件夹路径（可以是相对路径）
     * @return 如果成功打开则返回 true
     *
     * 平台支持：
     * - Windows: 使用 explorer 命令
     * - Linux: 使用 xdg-open 命令
     * - macOS: 使用 open 命令
     */
    Q_INVOKABLE bool openFolder(const QString& path);

    /**
     * @brief 将相对路径转换为绝对路径
     * @param path 输入路径（可以是相对路径或绝对路径）
     * @return 绝对路径
     */
    Q_INVOKABLE QString toAbsolutePath(const QString& path);

    /**
     * @brief 检查路径是否存在
     * @param path 文件或文件夹路径
     * @return 如果存在则返回 true
     */
    Q_INVOKABLE bool pathExists(const QString& path);

private:
    /**
     * @brief 在 Windows 上打开文件夹
     * @param absolutePath 绝对路径
     * @return 如果成功则返回 true
     */
    bool openFolderOnWindows(const QString& absolutePath);

    /**
     * @brief 在 Linux 上打开文件夹
     * @param absolutePath 绝对路径
     * @return 如果成功则返回 true
     */
    bool openFolderOnLinux(const QString& absolutePath);

    /**
     * @brief 在 macOS 上打开文件夹
     * @param absolutePath 绝对路径
     * @return 如果成功则返回 true
     */
    bool openFolderOnMacOS(const QString& absolutePath);
};

}  // namespace EasyKiConverter

#endif  // FILEUTILS_H