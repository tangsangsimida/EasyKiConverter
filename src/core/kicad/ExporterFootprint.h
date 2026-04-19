#ifndef EXPORTERFOOTPRINT_H
#define EXPORTERFOOTPRINT_H

#include "FootprintGraphicsGenerator.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"

#include <QJsonObject>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief KiCad 封装导出器类
 *
 * 用于EasyEDA 封装数据导出KiCad 封装格式
 */
class ExporterFootprint {
public:
    ExporterFootprint();

    ~ExporterFootprint();

    /**
     * @brief 导出封装库KiCad 格式（单D模型
     *
     * @param footprintData 封装数据
     * @param filePath 输出文件路径
     * @param model3DPath 3D模型路径
     * @return bool 是否成功
     */
    bool exportFootprint(const FootprintData& footprintData,
                         const QString& filePath,
                         const QString& model3DPath = QString());

    /**
     * @brief 导出封装库KiCad 格式（两D模型
         *
     * @param footprintData 封装数据
     * @param filePath 输出文件路径
     * @param model3DWrlPath WRL模型路径
     * @param model3DStepPath STEP模型路径
     * @return bool 是否成功
     */
    bool exportFootprint(const FootprintData& footprintData,
                         const QString& filePath,
                         const QString& model3DWrlPath,
                         const QString& model3DStepPath);

    /**
     * @brief 批量导出封装库（KiCad .kicad_mod 格式）
     *
     * @param footprints 封装列表
     * @param libName 库名称（用于构建 3D 模型相对路径）
     * @param filePath 输出目录路径（.pretty 目录）
     * @param preferWrl 是否优先使用 WRL 格式（默认 true，向后兼容）
     * @param exportStep 是否同时导出 STEP 格式（默认 false，向后兼容）
     * @note 默认参数仅导出 WRL，以保持与旧版本的兼容行为。
     *       调用方应显式传入 exportStep=true 以启用双格式导出。
     * @return bool 是否成功
     */
    bool exportFootprintLibrary(const QList<FootprintData>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false);

private:
    QString generateHeader(const QString& libName) const;
    void generateFootprintBaseContent(const FootprintData& footprintData,
                                      QString& content,
                                      double& outBboxX,
                                      double& outBboxY) const;
    QString generateFootprintContent(const FootprintData& footprintData, const QString& model3DPath = QString()) const;
    QString generateFootprintContent(const FootprintData& footprintData,
                                     const QString& model3DWrlPath,
                                     const QString& model3DStepPath) const;

private:
    mutable FootprintGraphicsGenerator m_graphicsGenerator;
};

}  // namespace EasyKiConverter

#endif  // EXPORTERFOOTPRINT_H
