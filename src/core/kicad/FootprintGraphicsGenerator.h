#ifndef FOOTPRINTGRAPHICSGENERATOR_H
#define FOOTPRINTGRAPHICSGENERATOR_H

#include "models/FootprintData.h"

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 封装图形生成器
 *
 * 从 ExporterFootprint 中提取的图形生成函数，
 * 负责将各类封装图形元素转换为 KiCad 格式文本。
 */
class FootprintGraphicsGenerator {
public:
    FootprintGraphicsGenerator() = default;

    // 图形元素生成函数
    QString generatePad(const FootprintPad& pad, double bboxX, double bboxY) const;
    QString generateTrack(const FootprintTrack& track, double bboxX, double bboxY) const;
    QString generateHole(const FootprintHole& hole, double bboxX, double bboxY) const;
    QString generateCircle(const FootprintCircle& circle, double bboxX, double bboxY) const;
    QString generateRectangle(const FootprintRectangle& rectangle, double bboxX, double bboxY) const;
    QString generateArc(const FootprintArc& arc, double bboxX, double bboxY) const;
    QString generateText(const FootprintText& text, double bboxX, double bboxY) const;
    QString generateSolidRegion(const FootprintSolidRegion& region, double bboxX, double bboxY) const;
    QString generateCourtyardFromBBox(const FootprintBBox& bbox, double bboxX, double bboxY) const;

    // 3D 模型生成
    QString generateModel3D(const Model3DData& model3D,
                            double bboxX,
                            double bboxY,
                            const QString& model3DPath,
                            const QString& fpType) const;

    // 单位转换
    double pxToMm(double px) const;
    double pxToMmRounded(double px) const;

    // 类型映射
    QString padShapeToKicad(const QString& shape) const;
    QString padTypeToKicad(int layerId) const;
    QString padLayersToKicad(int layerId) const;
    QString layerIdToKicad(int layerId) const;
};

}  // namespace EasyKiConverter

#endif  // FOOTPRINTGRAPHICSGENERATOR_H
