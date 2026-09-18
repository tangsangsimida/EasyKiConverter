#include "AltiumSymbolCurveConverter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <cmath>

namespace EasyKiConverter {

namespace {

using AltiumSymbolConversionUtils::finiteNonNegative;
using AltiumSymbolConversionUtils::toAltiumColor;
using AltiumSymbolConversionUtils::toAltiumLineStyle;
using AltiumSymbolConversionUtils::toAltiumOwnerPartId;

}  // namespace

/** @brief 根据三点计算圆弧，退化为首尾中点后写入 Altium 记录。 */
AltiumSchArc AltiumSymbolCurveConverter::convertArc(const IR::SymbolArcIR& arc) {
    AltiumSchArc altiumArc;
    const double ax = arc.startPoint.x();
    const double ay = arc.startPoint.y();
    const double bx = arc.midPoint.x();
    const double by = arc.midPoint.y();
    const double cx = arc.endPoint.x();
    const double cy = arc.endPoint.y();
    const double determinant = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    QPointF center = (arc.startPoint + arc.endPoint) / 2.0;
    if (std::abs(determinant) > 1e-12) {
        const double a2 = ax * ax + ay * ay;
        const double b2 = bx * bx + by * by;
        const double c2 = cx * cx + cy * cy;
        center.setX((a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / determinant);
        center.setY((a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / determinant);
    }
    const double dx = arc.startPoint.x() - center.x();
    const double dy = arc.startPoint.y() - center.y();
    const double radius = std::sqrt(dx * dx + dy * dy);
    altiumArc.centerX = AltiumCoord::mmToRaw(center.x());
    altiumArc.centerY = AltiumCoord::mmToRaw(center.y());
    altiumArc.radius = AltiumCoord::mmToRaw(radius);
    altiumArc.startAngle = std::atan2(arc.startPoint.y() - center.y(), arc.startPoint.x() - center.x()) * 180.0 / M_PI;
    altiumArc.endAngle = std::atan2(arc.endPoint.y() - center.y(), arc.endPoint.x() - center.x()) * 180.0 / M_PI;
    altiumArc.lineWidth = AltiumCoord::lineWidthMmToIndex(arc.strokeWidth);
    altiumArc.lineStyle = toAltiumLineStyle(arc.strokeStyle);
    altiumArc.color = toAltiumColor(arc.strokeColor);
    altiumArc.ownerPartId = toAltiumOwnerPartId(arc.partIndex);
    return altiumArc;
}

/** @brief 转换多边形及其顶点单位。 */
AltiumSchPolygon AltiumSymbolCurveConverter::convertPolygon(const IR::SymbolPolygonIR& polygon) {
    AltiumSchPolygon altiumPolygon;
    altiumPolygon.lineWidth = AltiumCoord::lineWidthMmToIndex(polygon.strokeWidth);
    altiumPolygon.lineStyle = toAltiumLineStyle(polygon.strokeStyle);
    altiumPolygon.color = toAltiumColor(polygon.strokeColor);
    altiumPolygon.areaColor = polygon.isFilled ? toAltiumColor(polygon.fillColor) : 0xFFFFFF;
    altiumPolygon.isSolid = polygon.isFilled;
    altiumPolygon.ownerPartId = toAltiumOwnerPartId(polygon.partIndex);
    for (const QPointF& point : polygon.points)
        altiumPolygon.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    return altiumPolygon;
}

/** @brief 转换折线及其顶点单位。 */
AltiumSchPolyline AltiumSymbolCurveConverter::convertPolyline(const IR::SymbolPolylineIR& polyline) {
    AltiumSchPolyline altiumPolyline;
    altiumPolyline.lineWidth = AltiumCoord::lineWidthMmToIndex(polyline.strokeWidth);
    altiumPolyline.lineStyle = toAltiumLineStyle(polyline.strokeStyle);
    altiumPolyline.color = toAltiumColor(polyline.strokeColor);
    altiumPolyline.ownerPartId = toAltiumOwnerPartId(polyline.partIndex);
    for (const QPointF& point : polyline.points)
        altiumPolyline.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    return altiumPolyline;
}

/** @brief 转换路径及其顶点单位。 */
AltiumSchPath AltiumSymbolCurveConverter::convertPath(const IR::SymbolPathIR& path) {
    AltiumSchPath altiumPath;
    altiumPath.lineWidth = AltiumCoord::lineWidthMmToIndex(path.strokeWidth);
    altiumPath.lineStyle = toAltiumLineStyle(path.strokeStyle);
    altiumPath.color = toAltiumColor(path.strokeColor);
    altiumPath.ownerPartId = toAltiumOwnerPartId(path.partIndex);
    for (const QPointF& point : path.points)
        altiumPath.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    return altiumPath;
}

/** @brief 转换 Bezier 控制点。 */
AltiumSchBezier AltiumSymbolCurveConverter::convertBezier(const IR::SymbolBezierIR& bezier) {
    AltiumSchBezier altiumBezier;
    altiumBezier.lineWidth = AltiumCoord::lineWidthMmToIndex(bezier.strokeWidth);
    altiumBezier.color = toAltiumColor(bezier.strokeColor);
    altiumBezier.ownerPartId = toAltiumOwnerPartId(bezier.partIndex);
    for (const QPointF& point : bezier.controlPoints)
        altiumBezier.controlPoints.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    return altiumBezier;
}

/** @brief 转换椭圆。 */
AltiumSchEllipse AltiumSymbolCurveConverter::convertEllipse(const IR::SymbolEllipseIR& ellipse) {
    AltiumSchEllipse altiumEllipse;
    altiumEllipse.centerX = AltiumCoord::mmToRaw(ellipse.center.x());
    altiumEllipse.centerY = AltiumCoord::mmToRaw(ellipse.center.y());
    altiumEllipse.radiusX = AltiumCoord::mmToRaw(ellipse.radiusX);
    altiumEllipse.radiusY = AltiumCoord::mmToRaw(ellipse.radiusY);
    altiumEllipse.lineWidth = AltiumCoord::lineWidthMmToIndex(ellipse.strokeWidth);
    altiumEllipse.lineStyle = toAltiumLineStyle(ellipse.strokeStyle);
    altiumEllipse.color = toAltiumColor(ellipse.strokeColor);
    altiumEllipse.areaColor = ellipse.isFilled ? toAltiumColor(ellipse.fillColor) : 0xFFFFFF;
    altiumEllipse.isSolid = ellipse.isFilled;
    altiumEllipse.ownerPartId = toAltiumOwnerPartId(ellipse.partIndex);
    return altiumEllipse;
}

/** @brief 转换扇形并规范化半径。 */
AltiumSchPie AltiumSymbolCurveConverter::convertPie(const IR::SymbolPieIR& pie) {
    AltiumSchPie altiumPie;
    altiumPie.centerX = AltiumCoord::mmToRaw(pie.center.x());
    altiumPie.centerY = AltiumCoord::mmToRaw(pie.center.y());
    altiumPie.radius = AltiumCoord::mmToRaw(finiteNonNegative(pie.radius));
    altiumPie.startAngle = pie.startAngle;
    altiumPie.endAngle = pie.endAngle;
    altiumPie.lineWidth = AltiumCoord::lineWidthMmToIndex(pie.strokeWidth);
    altiumPie.lineStyle = toAltiumLineStyle(pie.strokeStyle);
    altiumPie.color = toAltiumColor(pie.strokeColor);
    altiumPie.areaColor = pie.isFilled ? toAltiumColor(pie.fillColor) : 0xFFFFFF;
    altiumPie.isSolid = pie.isFilled;
    altiumPie.ownerPartId = toAltiumOwnerPartId(pie.partIndex);
    return altiumPie;
}

/** @brief 转换椭圆弧并规范化半径。 */
AltiumSchEllipticalArc AltiumSymbolCurveConverter::convertEllipticalArc(const IR::SymbolEllipticalArcIR& arc) {
    AltiumSchEllipticalArc altiumArc;
    altiumArc.centerX = AltiumCoord::mmToRaw(arc.center.x());
    altiumArc.centerY = AltiumCoord::mmToRaw(arc.center.y());
    altiumArc.radiusX = AltiumCoord::mmToRaw(finiteNonNegative(arc.radiusX));
    altiumArc.radiusY = AltiumCoord::mmToRaw(finiteNonNegative(arc.radiusY));
    altiumArc.startAngle = arc.startAngle;
    altiumArc.endAngle = arc.endAngle;
    altiumArc.lineWidth = AltiumCoord::lineWidthMmToIndex(arc.strokeWidth);
    altiumArc.lineStyle = toAltiumLineStyle(arc.strokeStyle);
    altiumArc.color = toAltiumColor(arc.strokeColor);
    altiumArc.areaColor = arc.isFilled ? toAltiumColor(arc.fillColor) : 0xFFFFFF;
    altiumArc.ownerPartId = toAltiumOwnerPartId(arc.partIndex);
    return altiumArc;
}

}  // namespace EasyKiConverter
