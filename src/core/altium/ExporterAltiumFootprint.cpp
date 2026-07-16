#include "ExporterAltiumFootprint.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"
#include "utils/AltiumStringUtils.h"

#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

/**
 * @brief 导出单个封装
 */
bool ExporterAltiumFootprint::exportFootprint(const FootprintData& footprintData,
                                              const QString& filePath,
                                              const QString& model3DPath) {
    QList<AltiumPcbComponent> components;
    components.append(convertFootprint(footprintData, model3DPath));
    return m_writer.write(components, filePath);
}

/**
 * @brief 导出封装库
 */
bool ExporterAltiumFootprint::exportFootprintLibrary(const QList<FootprintData>& footprints,
                                                     const QString& libName,
                                                     const QString& filePath,
                                                     bool preferWrl,
                                                     bool exportStep,
                                                     const QString& libraryDescription,
                                                     const QString& libraryKeywords,
                                                     bool useAbsolutePaths,
                                                     const QString& model3DBaseDir) {
    QList<AltiumPcbComponent> components;
    for (const FootprintData& fp : footprints) {
        // 对于库级别导出，3D 模型路径在后续处理
        components.append(convertFootprint(fp));
    }
    return m_writer.write(components, filePath, libName);
}

/**
 * @brief FootprintData → AltiumPcbComponent
 */
AltiumPcbComponent ExporterAltiumFootprint::convertFootprint(const FootprintData& data, const QString& model3DPath) {
    AltiumPcbComponent component;
    component.name = data.info().name;
    component.description = data.info().description;

    // 转换焊盘
    for (const FootprintPad& pad : data.pads()) {
        component.pads.append(convertPad(pad));
    }

    // 转换走线
    for (const FootprintTrack& track : data.tracks()) {
        component.tracks.append(convertTrack(track));
    }

    // 转换圆
    for (const FootprintCircle& circle : data.circles()) {
        component.arcs.append(convertCircle(circle));
    }

    // 转换弧线
    for (const FootprintArc& arc : data.arcs()) {
        component.arcs.append(convertArc(arc));
    }

    // 转换矩形
    for (const FootprintRectangle& rect : data.rectangles()) {
        component.fills.append(convertRectangle(rect));
    }

    // 转换文本
    for (const FootprintText& text : data.texts()) {
        component.texts.append(convertText(text));
    }

    // 转换区域
    for (const FootprintSolidRegion& region : data.solidRegions()) {
        component.regions.append(convertSolidRegion(region));
    }

    // 加载 3D 模型
    if (!model3DPath.isEmpty()) {
        QByteArray stepData = loadStepData(model3DPath);
        if (!stepData.isEmpty()) {
            AltiumPcbComponent::Model3D model;
            model.name = QFileInfo(model3DPath).fileName();
            model.stepData = stepData;
            component.models.append(model);
        }
    }

    return component;
}

/**
 * @brief FootprintPad → AltiumPcbPad
 */
AltiumPcbPad ExporterAltiumFootprint::convertPad(const FootprintPad& pad) {
    AltiumPcbPad altiumPad;
    altiumPad.designator = pad.number;
    altiumPad.locationX = AltiumCoord::toPcbRaw(static_cast<int>(pad.centerX));
    altiumPad.locationY = AltiumCoord::toPcbRaw(static_cast<int>(pad.centerY));

    int width = static_cast<int>(pad.width);
    int height = static_cast<int>(pad.height);

    // 确定焊盘类型
    bool hasHole = pad.holeRadius > 0;
    altiumPad.isSMD = !hasHole;
    altiumPad.isPlated = hasHole;

    if (hasHole) {
        // 通孔焊盘
        altiumPad.layer = 74;  // MultiLayer
        altiumPad.holeSize = AltiumCoord::toPcbRaw(static_cast<int>(pad.holeRadius * 2));
    } else {
        // 表贴焊盘
        altiumPad.layer = static_cast<uint8_t>(pad.layerId > 0 ? pad.layerId : 1);
    }

    // 形状
    uint8_t shape = AltiumLayerMap::toAltiumPadShape(pad.shape);
    altiumPad.shapeTop = shape;
    altiumPad.shapeMid = shape;
    altiumPad.shapeBot = shape;

    // 尺寸
    altiumPad.sizeTopX = AltiumCoord::toPcbRaw(width);
    altiumPad.sizeTopY = AltiumCoord::toPcbRaw(height);
    altiumPad.sizeMidX = altiumPad.sizeTopX;
    altiumPad.sizeMidY = altiumPad.sizeTopY;
    altiumPad.sizeBotX = altiumPad.sizeTopX;
    altiumPad.sizeBotY = altiumPad.sizeTopY;

    altiumPad.rotation = pad.rotation;

    return altiumPad;
}

/**
 * @brief FootprintTrack → AltiumPcbTrack
 * @details FootprintTrack.points 是空格分隔的 "x,y" 字符串
 */
AltiumPcbTrack ExporterAltiumFootprint::convertTrack(const FootprintTrack& track) {
    AltiumPcbTrack altiumTrack;
    altiumTrack.layer = static_cast<uint8_t>(track.layerId > 0 ? track.layerId : 1);
    altiumTrack.width = AltiumCoord::toPcbRaw(static_cast<int>(track.strokeWidth));

    QList<QPointF> points = AltiumStringUtils::parsePointsString(track.points);
    if (points.size() >= 2) {
        altiumTrack.startX = AltiumCoord::toPcbRaw(static_cast<int>(points.first().x()));
        altiumTrack.startY = AltiumCoord::toPcbRaw(static_cast<int>(points.first().y()));
        altiumTrack.endX = AltiumCoord::toPcbRaw(static_cast<int>(points.last().x()));
        altiumTrack.endY = AltiumCoord::toPcbRaw(static_cast<int>(points.last().y()));
    }

    return altiumTrack;
}

/**
 * @brief FootprintCircle → AltiumPcbArc（360° 圆弧）
 */
AltiumPcbArc ExporterAltiumFootprint::convertCircle(const FootprintCircle& circle) {
    AltiumPcbArc altiumArc;
    altiumArc.centerX = AltiumCoord::toPcbRaw(static_cast<int>(circle.cx));
    altiumArc.centerY = AltiumCoord::toPcbRaw(static_cast<int>(circle.cy));
    altiumArc.radius = AltiumCoord::toPcbRaw(static_cast<int>(circle.radius));
    altiumArc.startAngle = 0.0;
    altiumArc.endAngle = 360.0;
    altiumArc.width = AltiumCoord::toPcbRaw(static_cast<int>(circle.strokeWidth));
    altiumArc.layer = static_cast<uint8_t>(circle.layerId > 0 ? circle.layerId : 1);
    return altiumArc;
}

/**
 * @brief FootprintArc → AltiumPcbArc
 */
AltiumPcbArc ExporterAltiumFootprint::convertArc(const FootprintArc& arc) {
    AltiumPcbArc altiumArc;
    // EasyEDA 弧线使用 path 数据，这里简化处理
    altiumArc.layer = static_cast<uint8_t>(arc.layerId > 0 ? arc.layerId : 1);
    altiumArc.width = AltiumCoord::toPcbRaw(static_cast<int>(arc.strokeWidth));
    return altiumArc;
}

/**
 * @brief FootprintRectangle → AltiumPcbFill
 */
AltiumPcbFill ExporterAltiumFootprint::convertRectangle(const FootprintRectangle& rect) {
    AltiumPcbFill altiumFill;
    altiumFill.corner1X = AltiumCoord::toPcbRaw(static_cast<int>(rect.x));
    altiumFill.corner1Y = AltiumCoord::toPcbRaw(static_cast<int>(rect.y));
    altiumFill.corner2X = AltiumCoord::toPcbRaw(static_cast<int>(rect.x + rect.width));
    altiumFill.corner2Y = AltiumCoord::toPcbRaw(static_cast<int>(rect.y + rect.height));
    altiumFill.layer = static_cast<uint8_t>(rect.layerId > 0 ? rect.layerId : 1);
    return altiumFill;
}

/**
 * @brief FootprintText → AltiumPcbText
 */
AltiumPcbText ExporterAltiumFootprint::convertText(const FootprintText& text) {
    AltiumPcbText altiumText;
    altiumText.locationX = AltiumCoord::toPcbRaw(static_cast<int>(text.centerX));
    altiumText.locationY = AltiumCoord::toPcbRaw(static_cast<int>(text.centerY));
    altiumText.text = text.text;
    altiumText.rotation = text.rotation;
    altiumText.height = AltiumCoord::toPcbRaw(static_cast<int>(text.fontSize));
    altiumText.strokeWidth = AltiumCoord::toPcbRaw(static_cast<int>(text.strokeWidth));
    altiumText.layer = static_cast<uint8_t>(text.layerId > 0 ? text.layerId : 33);
    return altiumText;
}

/**
 * @brief FootprintSolidRegion → AltiumPcbRegion
 * @details FootprintSolidRegion.path 是 SVG 路径命令字符串
 */
AltiumPcbRegion ExporterAltiumFootprint::convertSolidRegion(const FootprintSolidRegion& region) {
    AltiumPcbRegion altiumRegion;
    altiumRegion.layer = static_cast<uint8_t>(region.layerId > 0 ? region.layerId : 1);
    altiumRegion.isBoardCutout = region.isKeepOut;

    QList<QPointF> points = AltiumStringUtils::parsePathString(region.path);
    for (const QPointF& point : points) {
        altiumRegion.vertices.append(QPointF(static_cast<double>(AltiumCoord::toPcbRaw(static_cast<int>(point.x()))),
                                             static_cast<double>(AltiumCoord::toPcbRaw(static_cast<int>(point.y())))));
    }

    return altiumRegion;
}

/**
 * @brief 加载 STEP 文件数据
 */
QByteArray ExporterAltiumFootprint::loadStepData(const QString& stepPath) {
    QFile file(stepPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

}  // namespace EasyKiConverter
