#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief Debug 数据导出助手
 *
 * 当 debugMode 启用时，将元器件的原始数据导出到 debug 文件夹。
 * 输出路径格式: outputPath/debug/componentId/
 *
 * 导出的文件:
 * - cinfo_raw.json: 元器件信息原始 JSON
 * - cad_raw.json: CAD 数据原始 JSON（包含符号和封装）
 * - model3d_raw.obj: 3D 模型 OBJ 原始数据
 */
class DebugExportHelper {
public:
    /**
     * @brief 导出 debug 数据
     * @param componentId 元器件 ID
     * @param data 元器件数据
     * @param outputPath 导出输出路径
     * @return bool 是否成功
     */
    static bool exportDebugData(const QString& componentId,
                                const QSharedPointer<ComponentData>& data,
                                const QString& outputPath);

private:
    static bool createOutputDirectory(const QString& path);
    static bool writeFile(const QString& filePath, const QByteArray& data);
};

}  // namespace EasyKiConverter