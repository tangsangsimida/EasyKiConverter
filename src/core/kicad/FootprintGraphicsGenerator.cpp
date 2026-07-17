#include "FootprintGraphicsGenerator.h"

#include "core/ir/EasyedaPadShapeMap.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

QString FootprintGraphicsGenerator::generatePad(const IR::FootprintPadIR& pad, double originX, double originY) const {
    QString content;

    // 坐标已为 mm，计算相对位置
    double x = roundTo2(pad.position.x() - originX);
    double y = roundTo2(pad.position.y() - originY);
    double width = roundTo2(pad.size.width());
    double height = roundTo2(pad.size.height());
    double holeRadius = roundTo2(pad.holeSize / 2.0);

    QString padNumber = pad.number;
    if (padNumber.contains("(") && padNumber.contains(")")) {
        int start = padNumber.indexOf("(");
        int end = padNumber.indexOf(")");
        padNumber = padNumber.mid(start + 1, end - start - 1);
    }

    double rotation = pad.rotation;
    if (rotation > 180.0) {
        rotation = rotation - 360.0;
    }

    // 使用 IR 映射获取 KiCad 焊盘形状
    bool isCustomShape = (pad.shape == IR::PadShape::Polygon);
    QString polygonStr;

    if (isCustomShape) {
        if (pad.customShapePoints.isEmpty()) {
            qWarning() << "Pad" << pad.number << "is a custom polygon, but has no points defined";
        } else {
            width = 0.005;
            height = 0.005;
            rotation = 0.0;

            QString path;
            // customShapePoints 已经是相对于焊盘中心的坐标（在 FootprintDataConverter 中计算）
            for (const QPointF& pt : pad.customShapePoints) {
                double relX = roundTo2(pt.x());
                double relY = roundTo2(pt.y());
                path += QString("(xy %1 %2) ").arg(relX, 0, 'f', 2).arg(relY, 0, 'f', 2);
            }

            polygonStr = QString(
                             "\n    (primitives\n      (gr_poly\n        (pts %1)\n        (width 0.1)\n        (fill "
                             "yes)\n      )\n    )\n  ")
                             .arg(path);
        }
    } else {
        width = qMax(width, 0.01);
        height = qMax(height, 0.01);
    }

    QString kicadShape = IR::EasyedaPadShapeMap::toKicadPadShape(pad.shape);

    QString kicadType = padTypeToKicad(pad);
    QString layers = padLayersToKicad(pad.layer);

    QString drillStr;
    if (pad.isThroughHole()) {
        double holeRadiusMm = roundTo2(pad.holeSize / 2.0);
        if (holeRadiusMm > 0 && pad.holeLength > 0) {
            double holeLengthMm = roundTo2(pad.holeLength);
            double maxDistanceHole = qMax(holeRadiusMm * 2, holeLengthMm);
            double pos0 = height - maxDistanceHole;
            double pos90 = width - maxDistanceHole;
            double maxDistance = qMax(pos0, pos90);

            if (qAbs(maxDistance - pos0) < qAbs(maxDistance - pos90)) {
                drillStr = QString(" (drill oval %1 %2)").arg(holeRadiusMm * 2, 0, 'f', 2).arg(holeLengthMm, 0, 'f', 2);
            } else {
                drillStr = QString(" (drill oval %1 %2)").arg(holeLengthMm, 0, 'f', 2).arg(holeRadiusMm * 2, 0, 'f', 2);
            }
        } else if (holeRadiusMm > 0) {
            double drillDiameter = holeRadiusMm * 2;
            drillStr = QString(" (drill %1)").arg(drillDiameter, 0, 'f', 2);
        }
    }

    content += QString("  (pad %1 %2 %3 (at %4 %5 %6) (size %7 %8) (layers %9)%10%11)\n")
                   .arg(padNumber)
                   .arg(kicadType)
                   .arg(kicadShape)
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(rotation, 0, 'f', 2)
                   .arg(width, 0, 'f', 2)
                   .arg(height, 0, 'f', 2)
                   .arg(layers)
                   .arg(drillStr)
                   .arg(polygonStr);

    return content;
}

QString FootprintGraphicsGenerator::generateTrack(const IR::FootprintTrackIR& track,
                                                  double originX,
                                                  double originY) const {
    QString content;

    // IR 中坐标已解析为 QList<QPointF>
    for (int i = 0; i + 1 < track.points.size(); ++i) {
        double startX = roundTo2(track.points[i].x() - originX);
        double startY = roundTo2(track.points[i].y() - originY);
        double endX = roundTo2(track.points[i + 1].x() - originX);
        double endY = roundTo2(track.points[i + 1].y() - originY);

        content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                       .arg(startX, 0, 'f', 2)
                       .arg(startY, 0, 'f', 2)
                       .arg(endX, 0, 'f', 2)
                       .arg(endY, 0, 'f', 2)
                       .arg(layerTypeToKicad(track.layer))
                       .arg(roundTo2(track.width), 0, 'f', 2);
    }

    return content;
}

QString FootprintGraphicsGenerator::generateHole(const IR::FootprintHoleIR& hole,
                                                 double originX,
                                                 double originY) const {
    QString content;
    double cx = roundTo2(hole.center.x() - originX);
    double cy = roundTo2(hole.center.y() - originY);
    double radius = roundTo2(hole.radius);

    content += QString("  (pad \"\" thru_hole circle (at %1 %2) (size %3 %3) (drill %3) (layers *.Cu *.Mask))\n")
                   .arg(cx, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(radius * 2, 0, 'f', 2);

    return content;
}

QString FootprintGraphicsGenerator::generateCircle(const IR::FootprintCircleIR& circle,
                                                   double originX,
                                                   double originY) const {
    QString content;
    double cx = roundTo2(circle.center.x() - originX);
    double cy = roundTo2(circle.center.y() - originY);
    double radius = roundTo2(circle.radius);

    content += QString("  (fp_circle (center %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                   .arg(cx, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(cx + radius, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(layerTypeToKicad(circle.layer))
                   .arg(circle.strokeWidth, 0, 'f', 2);

    return content;
}

QString FootprintGraphicsGenerator::generateRectangle(const IR::FootprintRectangleIR& rectangle,
                                                      double originX,
                                                      double originY) const {
    QString content;
    double x = roundTo2(rectangle.bounds.left() - originX);
    double y = roundTo2(rectangle.bounds.top() - originY);
    double width = roundTo2(rectangle.bounds.width());
    double height = roundTo2(rectangle.bounds.height());
    QString layer = layerTypeToKicad(rectangle.layer);
    double strokeWidth = roundTo2(rectangle.strokeWidth);

    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(x + width, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
                   .arg(x + width, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(x + width, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    return content;
}

QString FootprintGraphicsGenerator::generateArc(const IR::FootprintArcIR& arc, double originX, double originY) const {
    QString content;

    // IR 弧线已解析为 center/radius/startAngle/endAngle
    if (arc.radius > 0) {
        double cx = roundTo2(arc.center.x() - originX);
        double cy = roundTo2(arc.center.y() - originY);

        double startAngle = arc.startAngle;
        double deltaAngle = arc.endAngle - arc.startAngle;
        double step = 180.0;

        if (deltaAngle < 0) {
            startAngle = startAngle + deltaAngle;
            deltaAngle = -deltaAngle;
        }

        while (deltaAngle > 0.1) {
            if (deltaAngle < step) {
                step = deltaAngle;
            }

            // 计算圆弧段的起点和终点
            double radStart = startAngle * M_PI / 180.0;
            double radEnd = (startAngle + step) * M_PI / 180.0;

            double endX = roundTo2(arc.center.x() + arc.radius * std::cos(radEnd) - originX);
            double endY = roundTo2(arc.center.y() + arc.radius * std::sin(radEnd) - originY);

            double kiEndAngle = startAngle;
            if (step == 180.0)
                kiEndAngle += 0.1;

            content += QString("  (fp_arc (start %1 %2) (end %3 %4) (angle %5) (layer %6) (width %7))\n")
                           .arg(cx, 0, 'f', 2)
                           .arg(cy, 0, 'f', 2)
                           .arg(endX, 0, 'f', 2)
                           .arg(endY, 0, 'f', 2)
                           .arg(-kiEndAngle, 0, 'f', 2)
                           .arg(layerTypeToKicad(arc.layer))
                           .arg(roundTo2(arc.width), 0, 'f', 2);

            deltaAngle -= step;
            startAngle += step;
        }
    } else {
        qWarning() << "Warning: Arc has zero radius, skipping";
    }

    return content;
}

QString FootprintGraphicsGenerator::generateText(const IR::FootprintTextIR& text,
                                                 double originX,
                                                 double originY) const {
    QString content;
    double x = roundTo2(text.position.x() - originX);
    double y = roundTo2(text.position.y() - originY);

    QString layer = layerTypeToKicad(text.layer);

    // 装配层文本映射
    if (text.isFabrication) {
        layer = layer.replace(".SilkS", ".Fab");
    }

    QString displayStr = text.isDisplayed ? "" : " hide";

    bool isNonASCII = false;
    for (int i = 0; i < text.text.length(); ++i) {
        if (text.text[i].unicode() > 127) {
            isNonASCII = true;
            break;
        }
    }
    if (isNonASCII && !text.textPathPoints.isEmpty()) {
        qWarning() << "Warning: Converting non-ASCII text to polygon:" << text.text;

        // IR 中已解析为 QList<QPointF>
        const QList<QPointF>& points = text.textPathPoints;

        if (points.size() >= 2) {
            // 计算相对坐标
            QList<QPointF> relPoints;
            for (const QPointF& pt : points) {
                relPoints.append(QPointF(roundTo2(pt.x() - originX), roundTo2(pt.y() - originY)));
            }

            for (int i = 1; i < relPoints.size(); ++i) {
                content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                               .arg(relPoints[i - 1].x(), 0, 'f', 2)
                               .arg(relPoints[i - 1].y(), 0, 'f', 2)
                               .arg(relPoints[i].x(), 0, 'f', 2)
                               .arg(relPoints[i].y(), 0, 'f', 2)
                               .arg(layer)
                               .arg(roundTo2(text.strokeWidth) * 0.8, 0, 'f', 2);
            }
        }
    } else {
        bool isBottomLayer = layer.startsWith("B");
        QString mirrorStr = isBottomLayer ? " mirror" : "";

        content += QString("  (fp_text user %1 (at %2 %3 %4) (layer %5)%6\n")
                       .arg(text.text)
                       .arg(x, 0, 'f', 2)
                       .arg(y, 0, 'f', 2)
                       .arg(text.rotation, 0, 'f', 2)
                       .arg(layer)
                       .arg(displayStr);

        // IR 中 fontSize 和 strokeWidth 已为 mm
        double fontSize = roundTo2(text.fontSize);
        double thickness = roundTo2(text.strokeWidth);
        fontSize = qMax(fontSize, 1.0);
        thickness = qMax(thickness, 0.01);
        content += QString("    (effects (font (size %1 %2) (thickness %3)) (justify left%4))\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2)
                       .arg(thickness, 0, 'f', 2)
                       .arg(mirrorStr);
        content += "  )\n";
    }

    return content;
}

QString FootprintGraphicsGenerator::generateSolidRegion(const IR::FootprintRegionIR& region,
                                                        double originX,
                                                        double originY) const {
    QString content;

    QString layer = layerTypeToKicad(region.layer);

    // IR 中顶点已解析为 QList<QPointF>
    QList<QPointF> points;
    for (const QPointF& pt : region.vertices) {
        points.append(QPointF(roundTo2(pt.x() - originX), roundTo2(pt.y() - originY)));
    }

    // 闭合多边形
    if (!points.isEmpty() && points.first() != points.last()) {
        points.append(points.first());
    }

    if (points.size() >= 2) {
        if (region.layer == IR::LayerType::KeepOut || region.isKeepOut) {
            for (int i = 1; i < points.size(); ++i) {
                content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width 0.05))\n")
                               .arg(points[i - 1].x(), 0, 'f', 2)
                               .arg(points[i - 1].y(), 0, 'f', 2)
                               .arg(points[i].x(), 0, 'f', 2)
                               .arg(points[i].y(), 0, 'f', 2)
                               .arg(layer);
            }
        } else {
            content += "  (fp_poly\n";
            content += "    (pts";
            for (const QPointF& pt : points) {
                content += QString(" (xy %1 %2)").arg(pt.x(), 0, 'f', 2).arg(pt.y(), 0, 'f', 2);
            }
            content += ")\n";
            content += QString("    (layer %1)\n").arg(layer);
            content += "    (width 0.1)\n";
            content += "    (fill solid)\n";
            content += "  )\n";
        }
    }

    return content;
}

QString FootprintGraphicsGenerator::generateCourtyardFromBBox(double x1, double y1, double x2, double y2) const {
    QString content;

    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(x2, 0, 'f', 2);
    content += QString("  (fp_line (start %3 %2) (end %3 %4) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x2, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);
    content += QString("  (fp_line (start %3 %4) (end %1 %4) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(x2, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);
    content += QString("  (fp_line (start %1 %4) (end %1 %2) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);

    return content;
}

QString FootprintGraphicsGenerator::generateModel3D(const IR::Model3DIR& model3D, const QString& model3DPath) const {
    QString content;

    QString finalPath = model3DPath.isEmpty() ? model3D.name() : model3DPath;

    // IR 中 translation 和 rotation 已为 mm/度
    double z = -model3D.translation().z;

    double rotX = fmod(360.0 - model3D.rotation().x, 360.0);
    if (rotX < 0.0)
        rotX += 360.0;

    double rotY = fmod(360.0 - model3D.rotation().y, 360.0);
    if (rotY < 0.0)
        rotY += 360.0;

    double rotZ = fmod(360.0 - model3D.rotation().z, 360.0);
    if (rotZ < 0.0)
        rotZ += 360.0;

    // STEP 文件需要额外的 stepOffsetMm 补偿
    double finalOffsetX = 0.0;
    double finalOffsetY = 0.0;

    if (finalPath.endsWith(QStringLiteral(".step"), Qt::CaseInsensitive) ||
        finalPath.endsWith(QStringLiteral(".stp"), Qt::CaseInsensitive)) {
        finalOffsetX = model3D.stepOffsetMm().x;
        finalOffsetY = model3D.stepOffsetMm().y;
        z += model3D.stepOffsetMm().z;
    }

    content += QString("  (model \"%1\"\n").arg(finalPath);

    content += QString("    (offset (xyz %1 %2 %3))\n")
                   .arg(finalOffsetX, 0, 'f', 3)
                   .arg(finalOffsetY, 0, 'f', 3)
                   .arg(z, 0, 'f', 3);

    content += "    (scale (xyz 1 1 1))\n";

    content += QString("    (rotate (xyz %1 %2 %3))\n").arg(rotX, 0, 'f', 0).arg(rotY, 0, 'f', 0).arg(rotZ, 0, 'f', 0);

    content += "  )\n";

    return content;
}

double FootprintGraphicsGenerator::roundTo2(double value) {
    return std::floor(value * 100.0) / 100.0;
}

QString FootprintGraphicsGenerator::layerTypeToKicad(IR::LayerType layerType) {
    switch (layerType) {
        case IR::LayerType::TopCopper:
            return "F.Cu";
        case IR::LayerType::BottomCopper:
            return "B.Cu";
        case IR::LayerType::InnerCopper1:
            return "In1.Cu";
        case IR::LayerType::InnerCopper2:
            return "In2.Cu";
        case IR::LayerType::InnerCopper3:
            return "In3.Cu";
        case IR::LayerType::InnerCopper4:
            return "In4.Cu";
        case IR::LayerType::TopSilk:
            return "F.SilkS";
        case IR::LayerType::BottomSilk:
            return "B.SilkS";
        case IR::LayerType::TopPaste:
            return "F.Paste";
        case IR::LayerType::BottomPaste:
            return "B.Paste";
        case IR::LayerType::TopMask:
            return "F.Mask";
        case IR::LayerType::BottomMask:
            return "B.Mask";
        case IR::LayerType::TopOverlay:
            return "F.SilkS";
        case IR::LayerType::BottomOverlay:
            return "B.SilkS";
        case IR::LayerType::TopAssembly:
            return "F.Fab";
        case IR::LayerType::BottomAssembly:
            return "B.Fab";
        case IR::LayerType::MultiLayer:
            return "F.Cu";
        case IR::LayerType::KeepOut:
            return "F.CrtYd";
        case IR::LayerType::EdgeCuts:
            return "Edge.Cuts";
        case IR::LayerType::Mechanical1:
            return "Dwgs.User";
        case IR::LayerType::Mechanical2:
            return "Dwgs.User";
        case IR::LayerType::Mechanical3:
            return "Eco1.User";
        case IR::LayerType::Mechanical4:
            return "Eco2.User";
        case IR::LayerType::UserDefined:
            return "Dwgs.User";
        default:
            qWarning() << "Unknown layer type, defaulting to F.Fab";
            return "F.Fab";
    }
}

QString FootprintGraphicsGenerator::padLayersToKicad(IR::LayerType layer) {
    switch (layer) {
        case IR::LayerType::TopCopper:
            return "F.Cu F.Paste F.Mask";
        case IR::LayerType::BottomCopper:
            return "B.Cu B.Paste B.Mask";
        case IR::LayerType::TopSilk:
            return "F.SilkS";
        case IR::LayerType::BottomSilk:
            return "B.SilkS";
        case IR::LayerType::MultiLayer:
            return "*.Cu *.Mask";
        case IR::LayerType::TopAssembly:
            return "F.Fab";
        case IR::LayerType::BottomAssembly:
            return "B.Fab";
        case IR::LayerType::UserDefined:
            return "Dwgs.User";
        default:
            qWarning() << "Unknown pad layer type, using default thru-hole configuration";
            return "*.Cu *.Mask";
    }
}

QString FootprintGraphicsGenerator::padTypeToKicad(const IR::FootprintPadIR& pad) {
    if (pad.isSmd()) {
        return "smd";
    } else if (pad.isThroughHole()) {
        return "thru_hole";
    }
    return "thru_hole";
}

}  // namespace EasyKiConverter
