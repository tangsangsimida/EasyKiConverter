#ifndef EXPORTWORKERHELPERS_H
#define EXPORTWORKERHELPERS_H

#include "ExportProgress.h"

#include <QString>

class QDir;
class ComponentData;

namespace EasyKiConverter {

/**
 * @brief 导出Worker公共辅助函数
 *
 * 提供所有导出类型Worker的公共逻辑：
 * - 输出目录创建
 * - 文件存在检查
 * - 文件路径生成
 *
 * 使用方式：直接调用静态方法
 * @code
 * QString outputDir = ExportWorkerHelpers::ensureOutputDir(m_options, "symbols");
 * if (outputDir.isEmpty()) {
 *     emit completed(m_componentId, false, "Failed to create directory");
 *     return;
 * }
 * QString filePath = outputDir + "/" + m_componentId + ".kicad_sym";
 * if (ExportWorkerHelpers::shouldSkipExisting(filePath, m_options)) {
 *     emit completed(m_componentId, true, QString());
 *     return;
 * }
 * @endcode
 */
class ExportWorkerHelpers {
public:
    /**
     * @brief 确保输出目录存在
     * @param options 导出选项
     * @param subdir 子目录名称（如 "symbols", "footprints"）
     * @return 目录路径（失败时返回空字符串）
     */
    static QString ensureOutputDir(const struct ExportOptions& options, const QString& subdir);

    /**
     * @brief 构建输出文件完整路径
     * @param componentId 元器件ID
     * @param outputDir 输出目录路径
     * @param fileExtension 文件扩展名（如 ".kicad_sym"）
     * @return 完整文件路径
     */
    static QString buildFilePath(const QString& componentId, const QString& outputDir, const QString& fileExtension);

    /**
     * @brief 检查是否应跳过已存在的文件
     * @param filePath 完整文件路径
     * @param options 导出选项
     * @return true 应跳过（文件存在且不允许覆盖）
     */
    static bool shouldSkipExisting(const QString& filePath, const struct ExportOptions& options);

    /**
     * @brief 获取默认输出目录路径
     * @param subdir 子目录名称
     * @return 完整默认路径
     */
    static QString defaultOutputDir(const QString& subdir);
};

}  // namespace EasyKiConverter

#endif  // EXPORTWORKERHELPERS_H
