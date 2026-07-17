#include "ExporterAltiumFootprint.h"

#include "core/ir/EasyedaLayerMap.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"

#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

/**
 * @brief 导出单个封装
 */
bool ExporterAltiumFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                              const QString& filePath,
                                              const QString& model3DPath) {
    QList<AltiumPcbComponent> components;
    components.append(convertFootprint(footprint, model3DPath));
    return m_writer.write(components, filePath);
}

/**
 * @brief 导出封装库
 */
bool ExporterAltiumFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                     const QString& libName,
                                                     const QString& filePath,
                                                     bool preferWrl,
                                                     bool exportStep,
                                                     const QString& libraryDescription,
                                                     const QString& libraryKeywords,
                                                     bool useAbsolutePaths,
                                                     const QString& model3DBaseDir) {
    QList<AltiumPcbComponent> components;
    for (const IR::FootprintComponentIR& fp : footprints) {
        // 对于库级别导出，3D 模型路径在后续处理
        components.append(convertFootprint(fp));
    }
    return m_writer.write(components, filePath, libName);
}

/**
 * @brief FootprintComponentIR → AltiumPcbComponent
 */
AltiumPcbComponent ExporterAltiumFootprint::convertFootprint(const IR::FootprintComponentIR& data,
                                                             const QString& model3DPath) {
    AltiumPcbComponent component;
    component.name = data.name;
    component.description = data.description;
    component.height = data.height;  // IR height 已经是 mm

    // 转换焊盘
    for (const IR::FootprintPadIR& pad : data.pads) {
        component.pads.append(convertPad(pad));
    }

    // 转换走线
    for (const IR::FootprintTrackIR& track : data.tracks) {
        component.tracks.append(convertTrack(track));
    }

    // 转换圆
    for (const IR::FootprintCircleIR& circle : data.circles) {
        component.arcs.append(convertCircle(circle));
    }

    // 转换弧线
    for (const IR::FootprintArcIR& arc : data.arcs) {
        component.arcs.append(convertArc(arc));
    }

    // 转换矩形
    for (const IR::FootprintRectangleIR& rect : data.rectangles) {
        component.fills.append(convertRectangle(rect));
    }

    // 转换文本
    for (const IR::FootprintTextIR& text : data.texts) {
        component.texts.append(convertText(text));
    }

    // 转换区域
    for (const IR::FootprintRegionIR& region : data.regions) {
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
    } else if (!data.models3d.isEmpty()) {
        // 从 IR 的 3D 模型中加载 STEP 数据
        for (const IR::Model3DIR& model3d : data.models3d) {
            if (model3d.hasStepData()) {
                AltiumPcbComponent::Model3D model;
                model.name = model3d.name();
                model.stepData = model3d.stepData();
                const auto& rot = model3d.rotation();
                model.rotX = rot.x;
                model.rotY = rot.y;
                model.rotZ = rot.z;
                component.models.append(model);
            }
        }
    }

    return component;
}

/**
 * @brief FootprintPadIR → AltiumPcbPad
 */
AltiumPcbPad ExporterAltiumFootprint::convertPad(const IR::FootprintPadIR& pad) {
    AltiumPcbPad altiumPad;
    altiumPad.designator = pad.number;
    altiumPad.locationX = AltiumCoord::mmToPcbRaw(pad.position.x());
    altiumPad.locationY = AltiumCoord::mmToPcbRaw(pad.position.y());

    // IR size 已经是 mm
    int width = AltiumCoord::mmToPcbRaw(pad.size.width());
    int height = AltiumCoord::mmToPcbRaw(pad.size.height());

    // 确定焊盘类型
    altiumPad.isSMD = pad.isSmd();
    altiumPad.isPlated = pad.isPlated;

    if (pad.isThroughHole()) {
        // 通孔焊盘
        altiumPad.layer = 74;  // MultiLayer
        altiumPad.holeSize = AltiumCoord::mmToPcbRaw(pad.holeSize);
    } else {
        // 表贴焊盘：从 IR LayerType 映射到 Altium 层字节
        altiumPad.layer = static_cast<uint8_t>(IR::EasyedaLayerMap::fromLayerTypeToAltium(pad.layer));
    }

    // 形状：从 IR PadShape 映射到 Altium 焊盘形状字节
    uint8_t shape = AltiumLayerMap::toAltiumPadShape(pad.shape);
    altiumPad.shapeTop = shape;
    altiumPad.shapeMid = shape;
    altiumPad.shapeBot = shape;

    // 尺寸
    altiumPad.sizeTopX = width;
    altiumPad.sizeTopY = height;
    altiumPad.sizeMidX = width;
    altiumPad.sizeMidY = height;
    altiumPad.sizeBotX = width;
    altiumPad.sizeBotY = height;

    altiumPad.rotation = pad.rotation;

    return altiumPad;
}

/**
 * @brief FootprintTrackIR → AltiumPcbTrack
 */
AltiumPcbTrack ExporterAltiumFootprint::convertTrack(const IR::FootprintTrackIR& track) {
    AltiumPcbTrack altiumTrack;
    altiumTrack.layer = static_cast<uint8_t>(IR::EasyedaLayerMap::fromLayerTypeToAltium(track.layer));
    altiumTrack.width = AltiumCoord::mmToPcbRaw(track.width);

    // IR points 已经是解析后的 QList<QPointF>，单位 mm
    if (track.points.size() >= 2) {
        altiumTrack.startX = AltiumCoord::mmToPcbRaw(track.points.first().x());
        altiumTrack.startY = AltiumCoord::mmToPcbRaw(track.points.first().y());
        altiumTrack.endX = AltiumCoord::mmToPcbRaw(track.points.last().x());
        altiumTrack.endY = AltiumCoord::mmToPcbRaw(track.points.last().y());
    }

    return altiumTrack;
}

/**
 * @brief FootprintCircleIR → AltiumPcbArc（360° 圆弧）
 */
AltiumPcbArc ExporterAltiumFootprint::convertCircle(const IR::FootprintCircleIR& circle) {
    AltiumPcbArc altiumArc;
    altiumArc.centerX = AltiumCoord::mmToPcbRaw(circle.center.x());
    altiumArc.centerY = AltiumCoord::mmToPcbRaw(circle.center.y());
    altiumArc.radius = AltiumCoord::mmToPcbRaw(circle.radius);
    altiumArc.startAngle = 0.0;
    altiumArc.endAngle = 360.0;
    altiumArc.width = AltiumCoord::mmToPcbRaw(circle.strokeWidth);
    altiumArc.layer = static_cast<uint8_t>(IR::EasyedaLayerMap::fromLayerTypeToAltium(circle.layer));
    return altiumArc;
}

/**
 * @brief FootprintArcIR → AltiumPcbArc
 */
AltiumPcbArc ExporterAltiumFootprint::convertArc(const IR::FootprintArcIR& arc) {
    AltiumPcbArc altiumArc;
    altiumArc.centerX = AltiumCoord::mmToPcbRaw(arc.center.x());
    altiumArc.centerY = AltiumCoord::mmToPcbRaw(arc.center.y());
    altiumArc.radius = AltiumCoord::mmToPcbRaw(arc.radius);
    altiumArc.startAngle = arc.startAngle;
    altiumArc.endAngle = arc.endAngle;
    altiumArc.width = AltiumCoord::mmToPcbRaw(arc.width);
    altiumArc.layer = static_cast<uint8_t>(IR::EasyedaLayerMap::fromLayerTypeToAltium(arc.layer));
    return altiumArc;
}

/**
 * @brief FootprintRectangleIR → AltiumPcbFill
 */
AltiumPcbFill ExporterAltiumFootprint::convertRectangle(const IR::FootprintRectangleIR& rect) {
    AltiumPcbFill altiumFill;
    altiumFill.corner1X = AltiumCoord::mmToPcbRaw(rect.bounds.left());
    altiumFill.corner1Y = AltiumCoord::mmToPcbRaw(rect.bounds.top());
    altiumFill.corner2X = AltiumCoord::mmToPcbRaw(rect.bounds.right());
    altiumFill.corner2Y = AltiumCoord::mmToPcbRaw(rect.bounds.bottom());
    altiumFill.layer = static_cast<uint8_t>(IR::EasyedaLayerMap::fromLayerTypeToAltium(rect.layer));
    return altiumFill;
}

/**
 * @brief FootprintTextIR → AltiumPcbText
 */
AltiumPcbText ExporterAltiumFootprint::convertText(const IR::FootprintTextIR& text) {
    AltiumPcbText altiumText;
    altiumText.locationX = AltiumCoord::mmToPcbRaw(text.position.x());
    altiumText.locationY = AltiumCoord::mmToPcbRaw(text.position.y());
    altiumText.text = text.text;
    altiumText.rotation = text.rotation;
    altiumText.height = AltiumCoord::mmToPcbRaw(text.fontSize);
    altiumText.strokeWidth = AltiumCoord::mmToPcbRaw(text.strokeWidth);
    altiumText.isMirrored = text.mirror;
    altiumText.layer = static_cast<uint8_t>(IR::EasyedaLayerMap::fromLayerTypeToAltium(text.layer));
    return altiumText;
}

/**
 * @brief FootprintRegionIR → AltiumPcbRegion
 */
AltiumPcbRegion ExporterAltiumFootprint::convertSolidRegion(const IR::FootprintRegionIR& region) {
    AltiumPcbRegion altiumRegion;
    altiumRegion.layer = static_cast<uint8_t>(IR::EasyedaLayerMap::fromLayerTypeToAltium(region.layer));
    altiumRegion.isBoardCutout = region.isKeepOut;

    // IR vertices 已经是解析后的 QList<QPointF>，单位 mm
    for (const QPointF& point : region.vertices) {
        altiumRegion.vertices.append(QPointF(static_cast<double>(AltiumCoord::mmToPcbRaw(point.x())),
                                             static_cast<double>(AltiumCoord::mmToPcbRaw(point.y()))));
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
