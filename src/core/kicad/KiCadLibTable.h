#pragma once

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 生成 KiCad 库表文件（sym-lib-table / fp-lib-table）
 *
 * @param tableType 表类型名（"sym_lib_table" 或 "fp_lib_table"）
 * @param fileName 输出文件名（"sym-lib-table" 或 "fp-lib-table"）
 * @param libName 库名称
 * @param libPath 库文件/目录路径
 * @param outputDir 输出目录
 * @param libraryDescription 库描述（可选）
 * @return bool 是否成功
 */
bool generateKiCadLibTable(const QString& tableType,
                           const QString& fileName,
                           const QString& libName,
                           const QString& libPath,
                           const QString& outputDir,
                           const QString& libraryDescription);

}  // namespace EasyKiConverter
