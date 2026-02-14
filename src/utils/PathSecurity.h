#ifndef PATHSECURITY_H
#define PATHSECURITY_H

#include <QDir>
#include <QString>

namespace EasyKiConverter {

class PathSecurity {
public:
    /**
     * @brief 验证路径分块（文件名）是否安全
     * @param name 要验证的文件名或目录名
     * @return 如果不包含路径遍历字符（..）或分隔符，则返回 true
     */
    static bool isValidPathComponent(const QString& name);

    /**
     * @brief 验证目标路径是否在基准目录内
     * @param fullPath 目标完整路径
     * @param baseDir 基准目录路径
     * @return 如果安全则返回 true
     */
    static bool isSafePath(const QString& fullPath, const QString& baseDir);

    /**
     * @brief 安全递归删除目录
     * @param path 要删除的目录路径
     * @param maxFileCount 最大允许删除的文件数量（防止意外的大规模删除）
     * @return 如果成功删除则返回 true
     */
    static bool safeRemoveRecursively(const QString& path, int maxFileCount = 10000);

    /**
     * @brief 清洗文件名，将非法字符替换为下划线
     * @param name 原始文件名
     * @return 清洗后的文件名
     */
    static QString sanitizeFilename(const QString& name);

private:
    // 参数顺序优化：将常用变动参数放在前面，或者保持默认值在后
    static int countFilesRecursively(const QString& path, int limit, int currentCount = 0);
};

}  // namespace EasyKiConverter

#endif  // PATHSECURITY_H
