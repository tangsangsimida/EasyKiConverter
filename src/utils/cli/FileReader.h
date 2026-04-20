#ifndef FILEREADER_H
#define FILEREADER_H

#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 文件读取工具
 *
 * 提供 BOM 表和元器件列表文件的读取功能。
 */
class FileReader {
public:
    /**
     * @brief 读取 BOM 表文件
     * @param filePath 文件路径
     * @param errorMessage 错误信息输出
     * @return 元器件 ID 列表
     */
    static QStringList readBomFile(const QString& filePath, QString& errorMessage);

    /**
     * @brief 读取元器件列表文件
     * @param filePath 文件路径
     * @param errorMessage 错误信息输出
     * @return 元器件 ID 列表
     */
    static QStringList readComponentListFile(const QString& filePath, QString& errorMessage);

    /**
     * @brief 检查文件是否存在
     * @param filePath 文件路径
     * @return 存在返回 true
     */
    static bool fileExists(const QString& filePath);

    /**
     * @brief 获取文件扩展名
     * @param filePath 文件路径
     * @return 扩展名（小写）
     */
    static QString getFileExtension(const QString& filePath);
};

}  // namespace EasyKiConverter

#endif  // FILEREADER_H
