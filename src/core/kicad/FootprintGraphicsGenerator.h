#ifndef FOOTPRINTGRAPHICSGENERATOR_H
#define FOOTPRINTGRAPHICSGENERATOR_H

#include "core/ir/FootprintIR.h"
#include "core/ir/Model3DIR.h"

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
    QString generatePad(const IR::FootprintPadIR& pad, double originX, double originY) const;
    QString generateTrack(const IR::FootprintTrackIR& track, double originX, double originY) const;
    QString generateHole(const IR::FootprintHoleIR& hole, double originX, double originY) const;
    QString generateCircle(const IR::FootprintCircleIR& circle, double originX, double originY) const;
    QString generateRectangle(const IR::FootprintRectangleIR& rectangle, double originX, double originY) const;
    QString generateArc(const IR::FootprintArcIR& arc, double originX, double originY) const;
    QString generateText(const IR::FootprintTextIR& text, double originX, double originY) const;
    QString generateSolidRegion(const IR::FootprintRegionIR& region, double originX, double originY) const;
    QString generateCourtyardFromBBox(double x1, double y1, double x2, double y2) const;

    // 3D 模型生成
    QString generateModel3D(const IR::Model3DIR& model3D, const QString& model3DPath) const;

    // 层类型映射
    static QString layerTypeToKicad(IR::LayerType layerType);
    static QString padLayersToKicad(IR::LayerType layer);
    static QString padTypeToKicad(const IR::FootprintPadIR& pad);

private:
    static double roundTo2(double value);
};

}  // namespace EasyKiConverter

#endif  // FOOTPRINTGRAPHICSGENERATOR_H
