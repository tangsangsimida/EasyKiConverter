#ifndef TEMPFILEMANAGER_H
#define TEMPFILEMANAGER_H

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 临时文件管理器
 *
 * 负责管理导出过程中的临时文件，确保：
 * 1. 临时文件统一存放在输出目录的 .tmp 子目录中
 * 2. 导出完成时将临时文件重命名为最终文件
 * 3. 导出取消或失败时删除所有临时文件
 * 4. 程序异常退出时，下次启动清理遗留的临时文件
 *
 * 使用方式：
 * @code
 * TempFileManager tempManager;
 * tempManager.setOutputPath("/path/to/output");
 *
 * // 创建临时文件（返回最终文件路径）
 * QString finalPath = tempManager.createTempFile("C12345", ".kicad_sym");
 * QString tempPath = tempManager.tempFilePath(finalPath);
 *
 * // 写入临时文件...
 *
 * // 导出成功：完成（重命名）
 * tempManager.commit(finalPath);
 *
 * // 导出失败/取消：删除
 * tempManager.rollbackAll();
 * @endcode
 */
class TempFileManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit TempFileManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     * @details 自动清理所有临时文件
     */
    ~TempFileManager() override;

    /**
     * @brief 设置输出目录
     * @param outputPath 导出输出目录路径
     */
    void setOutputPath(const QString& outputPath);

    /**
     * @brief 获取临时文件目录路径
     * @return .tmp 子目录的完整路径
     */
    QString tempDirectory() const;

    /**
     * @brief 创建临时文件路径
     * @param componentId 元器件ID（用于生成文件名）
     * @param suffix 文件扩展名（如 ".kicad_sym"）
     * @return 最终文件路径（用于调用方写入）
     *
     * 注意：实际文件创建在 .tmp 子目录中，此方法返回最终路径供写入使用
     */
    QString createTempFilePath(const QString& componentId, const QString& suffix);

    /**
     * @brief 获取临时文件对应的实际路径
     * @param finalPath 最终文件路径
     * @return 临时文件路径（.tmp/ 目录下的路径）
     */
    QString tempFilePath(const QString& finalPath) const;

    /**
     * @brief 获取符号库临时文件路径
     * @param libName 库名称
     * @param suffix 文件扩展名
     * @return 临时文件路径
     */
    QString createSymbolTempPath(const QString& libName, const QString& suffix);

    /**
     * @brief 创建临时目录路径
     * @param dirName 目录名称（如 "libName.pretty"）
     * @return 临时目录路径
     *
     * 注意：仅创建路径信息，实际目录由调用方创建
     */
    QString createTempDirectoryPath(const QString& dirName);

    /**
     * @brief 提交临时文件（完成导出）
     * @param finalPath 最终文件路径
     * @return true 提交成功，false 失败
     *
     * 将临时文件重命名为最终路径，并从追踪列表中移除
     */
    bool commit(const QString& finalPath);

    /**
     * @brief 提交临时目录（完成导出）
     * @param finalDirPath 最终目录路径
     * @return true 提交成功，false 失败
     *
     * 将临时目录重命名为最终目录，并从追踪列表中移除
     */
    bool commitDirectory(const QString& finalDirPath);

    /**
     * @brief 回滚所有临时文件（取消导出）
     *
     * 删除所有临时文件，清空追踪列表
     */
    void rollbackAll();

    /**
     * @brief 仅删除临时目录（保留其他文件）
     *
     * 用于导出完成后的清理
     */
    void cleanupTempDirectory();

    /**
     * @brief 检查并清理遗留的临时文件
     *
     * 在应用程序启动时调用，清理上次异常退出留下的临时文件
     */
    void cleanupOrphanedTempFiles();

    /**
     * @brief 注册一个临时文件路径（用于追踪）
     * @param tempPath 临时文件路径
     */
    void registerTempFile(const QString& tempPath);

    /**
     * @brief 获取所有已注册的临时文件
     * @return 临时文件路径集合
     */
    QSet<QString> registeredTempFiles() const;

signals:
    /**
     * @brief 临时文件清理完成信号
     * @param deletedCount 删除的文件数量
     */
    void cleanupCompleted(int deletedCount);

    /**
     * @brief 提交完成信号
     * @param finalPath 最终文件路径
     * @param success 是否成功
     */
    void commitCompleted(const QString& finalPath, bool success);

private:
    /**
     * @brief 生成唯一的临时文件名
     * @param prefix 文件名前缀
     * @param suffix 文件扩展名
     * @return 唯一的临时文件名
     */
    QString generateUniqueTempName(const QString& prefix, const QString& suffix) const;

    /**
     * @brief 确保临时目录存在
     * @return true 目录存在或创建成功
     */
    bool ensureTempDirectory() const;

    /**
     * @brief 实际执行文件删除
     * @param path 文件路径
     * @return true 删除成功或文件不存在
     */
    bool deleteFile(const QString& path) const;

    QString m_outputPath;  ///< 输出目录路径
    mutable QMutex m_mutex;  ///< 保护临时文件集合
    QSet<QString> m_tempFiles;  ///< 注册的临时文件集合
    QString m_tempDirName = QStringLiteral(".tmp");  ///< 临时目录名称
};

}  // namespace EasyKiConverter

#endif  // TEMPFILEMANAGER_H
