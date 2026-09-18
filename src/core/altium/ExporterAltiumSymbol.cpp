#include "ExporterAltiumSymbol.h"

#include "AltiumSchSymbolGeometryNormalizer.h"
#include "AltiumSymbolPinConverter.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumSymbolConversionUtils.h"

#include <QDebug>
#include <QFile>
#include <QSet>

#include <algorithm>
#include <climits>
#include <cmath>

namespace EasyKiConverter {

namespace {

using AltiumSymbolConversionUtils::finiteNonNegative;
using AltiumSymbolConversionUtils::toAltiumColor;
using AltiumSymbolConversionUtils::toAltiumLineStyle;
using AltiumSymbolConversionUtils::toAltiumOrientation;
using AltiumSymbolConversionUtils::toAltiumOwnerPartId;

}  // namespace

/**
 * @brief 导出单个符号
 */
bool ExporterAltiumSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    m_diagnostics.clear();
    QList<AltiumSchComponent> components;
    components.append(convertSymbol(symbol));
    bool ok = m_writer.write(components, filePath);
    m_diagnostics.append(m_writer.diagnostics());
    if (!ok) {
        qWarning() << "ExporterAltiumSymbol: Failed to write symbol to" << filePath;
    }
    return ok;
}

/**
 * @brief 导出符号库
 */
bool ExporterAltiumSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                               const QString& libName,
                                               const QString& filePath,
                                               bool appendMode,
                                               bool updateMode,
                                               const QString& libraryDescription) {
    m_diagnostics.clear();
    if ((appendMode || updateMode) && QFile::exists(filePath)) {
        const QString diagnostic =
            QStringLiteral("Altium SchLib 暂不支持在已有库上追加或更新，已拒绝覆盖: %1").arg(filePath);
        m_diagnostics.append(diagnostic);
        qWarning() << "ExporterAltiumSymbol:" << diagnostic;
        return false;
    }

    QList<AltiumSchComponent> components;
    for (const IR::SymbolComponentIR& symbol : symbols) {
        components.append(convertSymbol(symbol));
    }
    bool ok = m_writer.write(components, filePath, libName);
    m_diagnostics.append(m_writer.diagnostics());
    if (!ok) {
        qWarning() << "ExporterAltiumSymbol: Failed to write symbol library to" << filePath;
    }
    return ok;
}

/**
 * @brief SymbolComponentIR → AltiumSchComponent
 */
AltiumSchComponent ExporterAltiumSymbol::convertSymbol(const IR::SymbolComponentIR& data) {
    AltiumSchComponent component;
    component.name = data.name;
    component.description = data.description;
    component.designatorPrefix = data.designatorPrefix;
    component.partCount = data.partCount;
    component.sourceMetadata = data.sourceMetadata;
    component.aliases = data.aliases;
    for (const IR::SymbolGraphicOrderIR& order : data.graphicOrder)
        component.graphicOrder.append({order.type, order.index, order.partIndex});
    const auto sourceIndexForPart = [](const auto& values, int index, int partIndex) {
        int localIndex = 0;
        for (int i = 0; i < index; ++i) {
            if (values.at(i).partIndex == partIndex)
                ++localIndex;
        }
        return localIndex;
    };
    const auto isFinitePoint = [](const QPointF& point) {
        return std::isfinite(point.x()) && std::isfinite(point.y());
    };
    const auto hasFinitePoints = [&](const QList<QPointF>& points, int minimum) {
        if (points.size() < minimum)
            return false;
        return std::all_of(points.cbegin(), points.cend(), isFinitePoint);
    };
    const auto isValidStrokeWidth = [](double width) { return std::isfinite(width) && width >= 0.0; };
    const auto isValidBounds = [](double x0, double y0, double x1, double y1) {
        return std::isfinite(x0) && std::isfinite(y0) && std::isfinite(x1) && std::isfinite(y1) && x0 != x1 && y0 != y1;
    };
    const auto normalizeTextAnchor = [this, &data](const QString& anchor, const QString& context) {
        const QString normalized = anchor.trimmed().toLower();
        if (normalized.isEmpty())
            return QStringLiteral("middle");
        if (!QStringList{QStringLiteral("start"), QStringLiteral("middle"), QStringLiteral("end")}.contains(
                normalized)) {
            m_diagnostics.append(QStringLiteral("符号 %1 %2 的对齐锚点无效: %3，已回退为 middle")
                                     .arg(data.name)
                                     .arg(context)
                                     .arg(anchor));
            return QStringLiteral("middle");
        }
        return normalized;
    };

    for (const IR::SymbolParameterIR& parameter : data.parameters) {
        if (!isFinitePoint(parameter.position) || !std::isfinite(parameter.rotation) ||
            parameter.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("符号 %1 参数 %2 的位置、旋转角度或名称无效，已跳过")
                                     .arg(data.name)
                                     .arg(parameter.name));
            continue;
        }
        if (!std::isfinite(parameter.fontSizeMm) || parameter.fontSizeMm < 0.0) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 参数 %2 的字体大小无效，已跳过").arg(data.name).arg(parameter.name));
            continue;
        }
        AltiumSchParameter altiumParameter;
        altiumParameter.name = parameter.name;
        altiumParameter.value = parameter.value;
        altiumParameter.locationX = AltiumCoord::mmToRaw(parameter.position.x());
        altiumParameter.locationY = AltiumCoord::mmToRaw(parameter.position.y());
        altiumParameter.fontSizeMm = parameter.fontSizeMm;
        altiumParameter.isHidden = !parameter.visible;
        altiumParameter.readOnly = parameter.readOnly;
        altiumParameter.orientation = toAltiumOrientation(parameter.rotation);
        altiumParameter.ownerPartId = toAltiumOwnerPartId(parameter.partIndex);
        altiumParameter.color = toAltiumColor(parameter.color);
        component.parameters.append(altiumParameter);
    }

    // 转换引脚
    for (const IR::SymbolPinIR& pin : data.pins) {
        if (!isFinitePoint(pin.position) || !std::isfinite(pin.length)) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 引脚 %2 的位置或长度无效，已跳过").arg(data.name).arg(pin.designator));
            continue;
        }
        component.pins.append(convertPin(pin));
        if (pin.hasNamePosition && !pin.name.isEmpty()) {
            if (!isFinitePoint(pin.namePosition) || !std::isfinite(pin.nameFontSizeMm) || pin.nameFontSizeMm < 0.0 ||
                !std::isfinite(pin.nameRotation)) {
                m_diagnostics.append(
                    QStringLiteral("符号 %1 引脚 %2 名称文本参数无效，已跳过").arg(data.name).arg(pin.designator));
            } else {
                AltiumSchText text;
                text.locationX = AltiumCoord::mmToRaw(pin.namePosition.x());
                text.locationY = AltiumCoord::mmToRaw(pin.namePosition.y());
                text.text = pin.name;
                text.fontSizeMm = pin.nameFontSizeMm;
                text.anchor =
                    normalizeTextAnchor(pin.nameAnchor, QStringLiteral("引脚 %1 名称文本").arg(pin.designator));
                text.isDisplayed = true;
                text.orientation = toAltiumOrientation(pin.nameRotation);
                text.ownerPartId = pin.commonToAllParts ? -1 : toAltiumOwnerPartId(pin.partIndex);
                text.isPinLabel = true;
                text.sourcePartIndex = pin.commonToAllParts ? -1 : pin.partIndex;
                component.texts.append(text);
            }
        }
        if (pin.hasNumberPosition && !pin.designator.isEmpty()) {
            if (!isFinitePoint(pin.numberPosition) || !std::isfinite(pin.numberFontSizeMm) ||
                pin.numberFontSizeMm < 0.0 || !std::isfinite(pin.numberRotation)) {
                m_diagnostics.append(
                    QStringLiteral("符号 %1 引脚 %2 编号文本参数无效，已跳过").arg(data.name).arg(pin.designator));
            } else {
                AltiumSchText text;
                text.locationX = AltiumCoord::mmToRaw(pin.numberPosition.x());
                text.locationY = AltiumCoord::mmToRaw(pin.numberPosition.y());
                text.text = pin.designator;
                text.fontSizeMm = pin.numberFontSizeMm;
                text.anchor =
                    normalizeTextAnchor(pin.numberAnchor, QStringLiteral("引脚 %1 编号文本").arg(pin.designator));
                text.isDisplayed = true;
                text.orientation = toAltiumOrientation(pin.numberRotation);
                text.ownerPartId = pin.commonToAllParts ? -1 : toAltiumOwnerPartId(pin.partIndex);
                text.isPinLabel = true;
                text.sourcePartIndex = pin.commonToAllParts ? -1 : pin.partIndex;
                component.texts.append(text);
            }
        }
    }

    // 转换图形元素
    for (int i = 0; i < data.rectangles.size(); ++i) {
        const IR::SymbolRectangleIR& r = data.rectangles.at(i);
        if (!isValidBounds(r.x0, r.y0, r.x1, r.y1)) {
            m_diagnostics.append(QStringLiteral("符号 %1 矩形图元 %2 的边界无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!std::isfinite(r.cornerRadiusX) || !std::isfinite(r.cornerRadiusY) || r.cornerRadiusX < 0.0 ||
            r.cornerRadiusY < 0.0) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 矩形图元 %2 的圆角半径无效，已钳制为非负值").arg(data.name).arg(i));
        }
        if (!isValidStrokeWidth(r.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 矩形图元 %2 的线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (r.cornerRadiusX > 0.0 || r.cornerRadiusY > 0.0) {
            AltiumSchRoundRectangle rectangle = convertRoundRectangle(r);
            rectangle.sourceGraphicIndex = sourceIndexForPart(data.rectangles, i, r.partIndex);
            rectangle.sourcePartIndex = r.partIndex;
            component.roundRectangles.append(rectangle);
        } else {
            AltiumSchRectangle rectangle = convertRectangle(r);
            rectangle.sourceGraphicIndex = sourceIndexForPart(data.rectangles, i, r.partIndex);
            rectangle.sourcePartIndex = r.partIndex;
            component.rectangles.append(rectangle);
        }
    }
    for (int i = 0; i < data.circles.size(); ++i) {
        const IR::SymbolCircleIR& circle = data.circles.at(i);
        if (!isFinitePoint(circle.center) || !isValidStrokeWidth(circle.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆图元 %2 的中心或线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!std::isfinite(circle.radius) || circle.radius < 0.0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆图元 %2 的半径无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchEllipse ellipse = convertCircle(circle);
        if (ellipse.radiusX <= 0 || ellipse.radiusY <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        ellipse.sourceGraphicType = QStringLiteral("C");
        ellipse.sourceGraphicIndex = sourceIndexForPart(data.circles, i, data.circles.at(i).partIndex);
        ellipse.sourcePartIndex = data.circles.at(i).partIndex;
        component.ellipses.append(ellipse);
    }
    for (int i = 0; i < data.arcs.size(); ++i) {
        const IR::SymbolArcIR& sourceArc = data.arcs.at(i);
        if (!isFinitePoint(sourceArc.startPoint) || !isFinitePoint(sourceArc.midPoint) ||
            !isFinitePoint(sourceArc.endPoint) || !std::isfinite(sourceArc.strokeWidth) ||
            sourceArc.strokeWidth < 0.0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆弧图元 %2 的点列或线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        const double determinant = 2.0 * (sourceArc.startPoint.x() * (sourceArc.midPoint.y() - sourceArc.endPoint.y()) +
                                          sourceArc.midPoint.x() * (sourceArc.endPoint.y() - sourceArc.startPoint.y()) +
                                          sourceArc.endPoint.x() * (sourceArc.startPoint.y() - sourceArc.midPoint.y()));
        if (!std::isfinite(determinant) || std::abs(determinant) <= 1e-12) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 圆弧图元 %2 三点退化，已使用安全回退圆心").arg(data.name).arg(i));
        }
        AltiumSchArc arc = convertArc(sourceArc);
        if (arc.radius <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 圆弧图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        arc.sourceGraphicType = QStringLiteral("A");
        arc.sourceGraphicIndex = sourceIndexForPart(data.arcs, i, data.arcs.at(i).partIndex);
        arc.sourcePartIndex = data.arcs.at(i).partIndex;
        component.arcs.append(arc);
    }
    for (int i = 0; i < data.polygons.size(); ++i) {
        const IR::SymbolPolygonIR& sourcePolygon = data.polygons.at(i);
        if (!hasFinitePoints(sourcePolygon.points, 3)) {
            m_diagnostics.append(QStringLiteral("符号 %1 多边形图元 %2 的点列无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!isValidStrokeWidth(sourcePolygon.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 多边形图元 %2 的线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchPolygon polygon = convertPolygon(sourcePolygon);
        polygon.sourceGraphicIndex = sourceIndexForPart(data.polygons, i, sourcePolygon.partIndex);
        polygon.sourcePartIndex = sourcePolygon.partIndex;
        component.polygons.append(polygon);
    }
    for (int i = 0; i < data.polylines.size(); ++i) {
        const IR::SymbolPolylineIR& sourcePolyline = data.polylines.at(i);
        if (!hasFinitePoints(sourcePolyline.points, 2)) {
            m_diagnostics.append(QStringLiteral("符号 %1 折线图元 %2 的点列无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        if (!isValidStrokeWidth(sourcePolyline.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 折线图元 %2 的线宽无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchPolyline polyline = convertPolyline(sourcePolyline);
        polyline.sourceGraphicIndex = sourceIndexForPart(data.polylines, i, sourcePolyline.partIndex);
        polyline.sourcePartIndex = sourcePolyline.partIndex;
        component.polylines.append(polyline);
    }
    for (int pathIndex = 0; pathIndex < data.paths.size(); ++pathIndex) {
        const IR::SymbolPathIR& p = data.paths.at(pathIndex);
        if (!isValidStrokeWidth(p.strokeWidth)) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 路径图元 %2 的线宽无效，已跳过").arg(data.name).arg(pathIndex));
            continue;
        }
        // 非填充路径可直接拆分为 Altium 原生线段和 Bézier 记录，避免曲线被
        // 强制膨胀为大量折线；填充路径仍使用闭合点列以保留填充语义。
        if (!p.isFilled && !p.segments.isEmpty()) {
            int segmentIndex = 0;
            for (const IR::SymbolPathSegmentIR& segment : p.segments) {
                bool validSegment = isFinitePoint(segment.start) && isFinitePoint(segment.end);
                // 按路径段类型校验所需控制点，避免将不完整曲线写入目标库。
                switch (segment.type) {
                    case IR::SymbolPathSegmentIR::Type::QuadraticBezier:
                        validSegment = validSegment && isFinitePoint(segment.control1);
                        break;
                    case IR::SymbolPathSegmentIR::Type::CubicBezier:
                        validSegment =
                            validSegment && isFinitePoint(segment.control1) && isFinitePoint(segment.control2);
                        break;
                    case IR::SymbolPathSegmentIR::Type::CircularArc:
                        validSegment = validSegment && isFinitePoint(segment.arcMid);
                        break;
                    case IR::SymbolPathSegmentIR::Type::EllipticalArc:
                        validSegment = validSegment && isFinitePoint(segment.arcCenter) &&
                                       std::isfinite(segment.radiusX) && std::isfinite(segment.radiusY) &&
                                       segment.radiusX > 0.0 && segment.radiusY > 0.0 &&
                                       std::isfinite(segment.arcStartAngle) && std::isfinite(segment.arcEndAngle);
                        break;
                    case IR::SymbolPathSegmentIR::Type::Line:
                        break;
                }
                if (!validSegment) {
                    m_diagnostics.append(QStringLiteral("符号 %1 路径图元 %2 的段 %3 参数无效，已跳过")
                                             .arg(data.name)
                                             .arg(pathIndex)
                                             .arg(segmentIndex));
                    ++segmentIndex;
                    continue;
                }
                if (segment.type == IR::SymbolPathSegmentIR::Type::Line) {
                    AltiumSchPath path;
                    path.lineWidth = AltiumCoord::lineWidthMmToIndex(p.strokeWidth);
                    path.lineStyle = toAltiumLineStyle(p.strokeStyle);
                    path.color = toAltiumColor(p.strokeColor);
                    path.ownerPartId = toAltiumOwnerPartId(p.partIndex);
                    path.sourceGraphicType = QStringLiteral("PT");
                    path.sourceGraphicIndex = sourceIndexForPart(data.paths, pathIndex, p.partIndex);
                    path.sourceSegmentIndex = segmentIndex;
                    path.sourcePartIndex = p.partIndex;
                    path.vertices.append(QPointF(AltiumCoord::mmToSchematicUnits(segment.start.x()),
                                                 AltiumCoord::mmToSchematicUnits(segment.start.y())));
                    path.vertices.append(QPointF(AltiumCoord::mmToSchematicUnits(segment.end.x()),
                                                 AltiumCoord::mmToSchematicUnits(segment.end.y())));
                    component.paths.append(path);
                } else if (segment.type == IR::SymbolPathSegmentIR::Type::QuadraticBezier ||
                           segment.type == IR::SymbolPathSegmentIR::Type::CubicBezier) {
                    IR::SymbolBezierIR bezier;
                    if (segment.type == IR::SymbolPathSegmentIR::Type::QuadraticBezier) {
                        // Altium SchLib 仅提供三次 Bézier 记录；二次曲线可用
                        // C1=P0+2/3(Q-P0), C2=P1+2/3(Q-P1) 精确表示。
                        const QPointF control1 = segment.start + (segment.control1 - segment.start) * (2.0 / 3.0);
                        const QPointF control2 = segment.end + (segment.control1 - segment.end) * (2.0 / 3.0);
                        bezier.controlPoints = {segment.start, control1, control2, segment.end};
                    } else {
                        bezier.controlPoints = {segment.start, segment.control1, segment.control2, segment.end};
                    }
                    bezier.strokeColor = p.strokeColor;
                    bezier.strokeWidth = p.strokeWidth;
                    bezier.strokeStyle = p.strokeStyle;
                    bezier.partIndex = p.partIndex;
                    AltiumSchBezier altiumBezier = convertBezier(bezier);
                    altiumBezier.sourceGraphicType = QStringLiteral("PT");
                    altiumBezier.sourceGraphicIndex = sourceIndexForPart(data.paths, pathIndex, p.partIndex);
                    altiumBezier.sourceSegmentIndex = segmentIndex;
                    altiumBezier.sourcePartIndex = p.partIndex;
                    component.beziers.append(altiumBezier);
                } else if (segment.type == IR::SymbolPathSegmentIR::Type::EllipticalArc) {
                    IR::SymbolEllipticalArcIR ellipseArc;
                    ellipseArc.center = segment.arcCenter;
                    ellipseArc.radiusX = segment.radiusX;
                    ellipseArc.radiusY = segment.radiusY;
                    ellipseArc.startAngle = segment.arcStartAngle;
                    ellipseArc.endAngle = segment.arcEndAngle;
                    ellipseArc.strokeColor = p.strokeColor;
                    ellipseArc.strokeWidth = p.strokeWidth;
                    ellipseArc.strokeStyle = p.strokeStyle;
                    ellipseArc.partIndex = p.partIndex;
                    AltiumSchEllipticalArc altiumArc = convertEllipticalArc(ellipseArc);
                    altiumArc.sourceGraphicType = QStringLiteral("PT");
                    altiumArc.sourceGraphicIndex = sourceIndexForPart(data.paths, pathIndex, p.partIndex);
                    altiumArc.sourceSegmentIndex = segmentIndex;
                    altiumArc.sourcePartIndex = p.partIndex;
                    if (altiumArc.radiusX <= 0 || altiumArc.radiusY <= 0) {
                        m_diagnostics.append(QStringLiteral("符号 %1 路径图元 %2 的段 %3 椭圆弧半径量化后无效，已跳过")
                                                 .arg(data.name)
                                                 .arg(pathIndex)
                                                 .arg(segmentIndex));
                        ++segmentIndex;
                        continue;
                    }
                    component.ellipticalArcs.append(altiumArc);
                } else {
                    IR::SymbolArcIR arc;
                    arc.startPoint = segment.start;
                    arc.midPoint = segment.arcMid;
                    arc.endPoint = segment.end;
                    arc.strokeColor = p.strokeColor;
                    arc.strokeWidth = p.strokeWidth;
                    arc.strokeStyle = p.strokeStyle;
                    arc.partIndex = p.partIndex;
                    AltiumSchArc altiumArc = convertArc(arc);
                    if (altiumArc.radius <= 0) {
                        m_diagnostics.append(QStringLiteral("符号 %1 路径图元 %2 的段 %3 圆弧半径量化后无效，已跳过")
                                                 .arg(data.name)
                                                 .arg(pathIndex)
                                                 .arg(segmentIndex));
                        ++segmentIndex;
                        continue;
                    }
                    altiumArc.sourceGraphicType = QStringLiteral("PT");
                    altiumArc.sourceGraphicIndex = sourceIndexForPart(data.paths, pathIndex, p.partIndex);
                    altiumArc.sourceSegmentIndex = segmentIndex;
                    altiumArc.sourcePartIndex = p.partIndex;
                    component.arcs.append(altiumArc);
                }
                ++segmentIndex;
            }
        } else {
            const int minimumPointCount = p.isFilled ? 3 : 2;
            if (!hasFinitePoints(p.points, minimumPointCount)) {
                m_diagnostics.append(
                    QStringLiteral("符号 %1 路径图元 %2 的点列无效，已跳过").arg(data.name).arg(pathIndex));
                continue;
            }
            AltiumSchPath path = convertPath(p);
            path.sourceGraphicType = QStringLiteral("PT");
            path.sourceGraphicIndex = sourceIndexForPart(data.paths, pathIndex, p.partIndex);
            path.sourcePartIndex = p.partIndex;
            component.paths.append(path);
        }
    }
    for (int bezierIndex = 0; bezierIndex < data.beziers.size(); ++bezierIndex) {
        const IR::SymbolBezierIR& b = data.beziers.at(bezierIndex);
        if (b.controlPoints.size() == 4 && hasFinitePoints(b.controlPoints, 4) && isValidStrokeWidth(b.strokeWidth)) {
            component.beziers.append(convertBezier(b));
        } else if (b.controlPoints.size() != 4 || !hasFinitePoints(b.controlPoints, 4)) {
            m_diagnostics.append(QStringLiteral("符号 %1 Bézier 图元 %2 的控制点参数无效（数量为 %3），已跳过")
                                     .arg(data.name)
                                     .arg(bezierIndex)
                                     .arg(b.controlPoints.size()));
        } else {
            m_diagnostics.append(
                QStringLiteral("符号 %1 Bézier 图元 %2 的线宽无效，已跳过").arg(data.name).arg(bezierIndex));
        }
    }
    for (int i = 0; i < data.ieeeSymbols.size(); ++i) {
        const IR::SymbolIeeeIR& ieee = data.ieeeSymbols.at(i);
        if (!isFinitePoint(ieee.position)) {
            m_diagnostics.append(QStringLiteral("符号 %1 IEEE 图形 %2 的位置无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.ieeeSymbols.append(convertIeee(ieee));
    }
    for (int i = 0; i < data.texts.size(); ++i) {
        const IR::SymbolTextIR& sourceText = data.texts.at(i);
        if (sourceText.text.trimmed().isEmpty() || !isFinitePoint(sourceText.position) ||
            !std::isfinite(sourceText.rotation) ||
            (sourceText.visible && (!std::isfinite(sourceText.fontSizeMm) || sourceText.fontSizeMm < 0.0))) {
            m_diagnostics.append(
                QStringLiteral("符号 %1 文本图元 %2 的内容或几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchText text = convertText(sourceText);
        text.anchor = normalizeTextAnchor(sourceText.anchor, QStringLiteral("文本图元 %1").arg(i));
        text.sourceGraphicIndex = sourceIndexForPart(data.texts, i, sourceText.partIndex);
        text.sourcePartIndex = sourceText.partIndex;
        component.texts.append(text);
    }
    for (int i = 0; i < data.textFrames.size(); ++i) {
        const IR::SymbolTextFrameIR& frame = data.textFrames.at(i);
        if (!isValidBounds(frame.x0, frame.y0, frame.x1, frame.y1) || !std::isfinite(frame.textMargin) ||
            frame.textMargin < 0.0 || !isValidStrokeWidth(frame.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 文本框图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.textFrames.append(convertTextFrame(frame));
    }
    for (int i = 0; i < data.images.size(); ++i) {
        const IR::SymbolImageIR& image = data.images.at(i);
        const bool validBounds = std::isfinite(image.x0) && std::isfinite(image.y0) && std::isfinite(image.x1) &&
                                 std::isfinite(image.y1) && image.x0 != image.x1 && image.y0 != image.y1;
        const QString imageSource = image.fileName.trimmed();
        const bool hasSource = !imageSource.isEmpty() || !image.data.isEmpty();
        const bool hasUnparsedDataUrl = imageSource.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive);
        if (!validBounds || !hasSource || hasUnparsedDataUrl || !std::isfinite(image.rotation) ||
            !std::isfinite(image.strokeWidth) || image.strokeWidth < 0.0) {
            if (hasUnparsedDataUrl) {
                m_diagnostics.append(
                    QStringLiteral("符号 %1 图片图元 %2 包含未解析的 data URL，已跳过").arg(data.name).arg(i));
            } else {
                m_diagnostics.append(
                    QStringLiteral("符号 %1 图片图元 %2 的边界、线宽或资源无效，已跳过").arg(data.name).arg(i));
            }
            continue;
        }
        AltiumSchImage altiumImage = convertImage(image);
        altiumImage.sourceGraphicIndex = sourceIndexForPart(data.images, i, image.partIndex);
        altiumImage.sourcePartIndex = image.partIndex;
        component.images.append(altiumImage);
    }
    for (int i = 0; i < data.ellipses.size(); ++i) {
        const IR::SymbolEllipseIR& sourceEllipse = data.ellipses.at(i);
        if (!isFinitePoint(sourceEllipse.center) || !std::isfinite(sourceEllipse.radiusX) ||
            !std::isfinite(sourceEllipse.radiusY) || sourceEllipse.radiusX <= 0.0 || sourceEllipse.radiusY <= 0.0 ||
            !isValidStrokeWidth(sourceEllipse.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchEllipse ellipse = convertEllipse(sourceEllipse);
        if (ellipse.radiusX <= 0 || ellipse.radiusY <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        ellipse.sourceGraphicType = QStringLiteral("E");
        ellipse.sourceGraphicIndex = sourceIndexForPart(data.ellipses, i, sourceEllipse.partIndex);
        ellipse.sourcePartIndex = sourceEllipse.partIndex;
        component.ellipses.append(ellipse);
    }
    for (int i = 0; i < data.pies.size(); ++i) {
        const IR::SymbolPieIR& sourcePie = data.pies.at(i);
        if (!isFinitePoint(sourcePie.center) || !std::isfinite(sourcePie.radius) || sourcePie.radius <= 0.0 ||
            !std::isfinite(sourcePie.startAngle) || !std::isfinite(sourcePie.endAngle) ||
            !isValidStrokeWidth(sourcePie.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 扇形图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchPie pie = convertPie(sourcePie);
        if (pie.radius <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 扇形图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.pies.append(pie);
    }
    for (int i = 0; i < data.ellipticalArcs.size(); ++i) {
        const IR::SymbolEllipticalArcIR& sourceArc = data.ellipticalArcs.at(i);
        if (!isFinitePoint(sourceArc.center) || !std::isfinite(sourceArc.radiusX) ||
            !std::isfinite(sourceArc.radiusY) || sourceArc.radiusX <= 0.0 || sourceArc.radiusY <= 0.0 ||
            !std::isfinite(sourceArc.startAngle) || !std::isfinite(sourceArc.endAngle) ||
            !isValidStrokeWidth(sourceArc.strokeWidth)) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆弧图元 %2 的几何参数无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        AltiumSchEllipticalArc ellipticalArc = convertEllipticalArc(sourceArc);
        if (ellipticalArc.radiusX <= 0 || ellipticalArc.radiusY <= 0) {
            m_diagnostics.append(QStringLiteral("符号 %1 椭圆弧图元 %2 半径量化后无效，已跳过").arg(data.name).arg(i));
            continue;
        }
        component.ellipticalArcs.append(ellipticalArc);
    }

    // 添加封装链接，保留多个候选封装
    QStringList footprintNames = data.footprintNames;
    if (footprintNames.isEmpty() && !data.footprintName.isEmpty())
        footprintNames.append(data.footprintName);
    QSet<QString> uniqueFootprints;
    for (const QString& footprintName : footprintNames) {
        const QString normalizedName = footprintName.trimmed();
        if (normalizedName.isEmpty() || uniqueFootprints.contains(normalizedName))
            continue;
        AltiumSchComponent::Implementation impl;
        impl.modelName = normalizedName;
        impl.modelType = "PCBLIB";
        impl.dataFileKind = "PCBLib";
        component.implementations.append(impl);
        uniqueFootprints.insert(normalizedName);
    }

    for (const IR::SymbolModelIR& model : data.models) {
        const QString modelName = model.name.trimmed();
        if (modelName.isEmpty())
            continue;
        AltiumSchComponent::Implementation impl;
        impl.modelName = modelName;
        impl.modelType = model.type.trimmed().isEmpty() ? QStringLiteral("SIM") : model.type.trimmed();
        impl.dataFileKind = model.fileKind.trimmed();
        impl.dataFileEntity = model.fileEntity.trimmed();
        impl.parameters = model.parameters;
        impl.pinMappings = model.pinMappings;
        component.implementations.append(impl);
    }

    // 兼容没有显式 SymbolModelIR 的调用方，允许通过来源元数据关联模型。
    const QMap<QString, QString> metadataModels = {
        {QStringLiteral("spiceModel"), QStringLiteral("SPICE")},
        {QStringLiteral("simulationModel"), QStringLiteral("SIM")},
        {QStringLiteral("model3D"), QStringLiteral("STEP")},
        {QStringLiteral("model3d"), QStringLiteral("STEP")},
    };
    for (auto it = metadataModels.constBegin(); it != metadataModels.constEnd(); ++it) {
        const QString modelName = data.sourceMetadata.value(it.key()).trimmed();
        if (modelName.isEmpty())
            continue;
        AltiumSchComponent::Implementation impl;
        impl.modelName = modelName;
        impl.modelType = it.value();
        impl.dataFileKind = data.sourceMetadata.value(it.key() + QStringLiteral("FileKind")).trimmed();
        impl.dataFileEntity = data.sourceMetadata.value(it.key() + QStringLiteral("File")).trimmed();
        component.implementations.append(impl);
    }

    // 坐标归一化：将符号中心移到原点
    centerComponent(component);

    return component;
}

/**
 * @brief SymbolPinIR → AltiumSchPin
 */
AltiumSchPin ExporterAltiumSymbol::convertPin(const IR::SymbolPinIR& pin) {
    return AltiumSymbolPinConverter::convert(pin);
}

/**
 * @brief SymbolRectangleIR → AltiumSchRectangle
 */
AltiumSchRectangle ExporterAltiumSymbol::convertRectangle(const IR::SymbolRectangleIR& rect) {
    AltiumSchRectangle altiumRect;
    altiumRect.locationX = AltiumCoord::mmToRaw(rect.x0);
    altiumRect.locationY = AltiumCoord::mmToRaw(rect.y0);
    altiumRect.cornerX = AltiumCoord::mmToRaw(rect.x1);
    altiumRect.cornerY = AltiumCoord::mmToRaw(rect.y1);
    altiumRect.lineWidth = AltiumCoord::lineWidthMmToIndex(rect.strokeWidth);
    altiumRect.lineStyle = toAltiumLineStyle(rect.strokeStyle);
    altiumRect.color = toAltiumColor(rect.strokeColor);
    altiumRect.areaColor = rect.isFilled ? toAltiumColor(rect.fillColor) : 0xFFFFFF;
    altiumRect.isSolid = rect.isFilled;
    altiumRect.ownerPartId = toAltiumOwnerPartId(rect.partIndex);
    return altiumRect;
}

/**
 * @brief SymbolRectangleIR → AltiumSchRoundRectangle
 */
AltiumSchRoundRectangle ExporterAltiumSymbol::convertRoundRectangle(const IR::SymbolRectangleIR& rect) {
    AltiumSchRoundRectangle altiumRect;
    altiumRect.locationX = AltiumCoord::mmToRaw(rect.x0);
    altiumRect.locationY = AltiumCoord::mmToRaw(rect.y0);
    altiumRect.cornerX = AltiumCoord::mmToRaw(rect.x1);
    altiumRect.cornerY = AltiumCoord::mmToRaw(rect.y1);
    altiumRect.cornerXRadius = AltiumCoord::mmToRaw(finiteNonNegative(rect.cornerRadiusX));
    altiumRect.cornerYRadius = AltiumCoord::mmToRaw(finiteNonNegative(rect.cornerRadiusY));
    altiumRect.lineWidth = AltiumCoord::lineWidthMmToIndex(rect.strokeWidth);
    altiumRect.lineStyle = toAltiumLineStyle(rect.strokeStyle);
    altiumRect.color = toAltiumColor(rect.strokeColor);
    altiumRect.areaColor = rect.isFilled ? toAltiumColor(rect.fillColor) : 0xFFFFFF;
    altiumRect.isSolid = rect.isFilled;
    altiumRect.ownerPartId = toAltiumOwnerPartId(rect.partIndex);
    return altiumRect;
}

/**
 * @brief SymbolCircleIR → AltiumSchEllipse
 */
AltiumSchEllipse ExporterAltiumSymbol::convertCircle(const IR::SymbolCircleIR& circle) {
    AltiumSchEllipse altiumEllipse;
    altiumEllipse.centerX = AltiumCoord::mmToRaw(circle.center.x());
    altiumEllipse.centerY = AltiumCoord::mmToRaw(circle.center.y());
    const double radius = finiteNonNegative(circle.radius);
    altiumEllipse.radiusX = AltiumCoord::mmToRaw(radius);
    altiumEllipse.radiusY = AltiumCoord::mmToRaw(radius);
    altiumEllipse.lineWidth = AltiumCoord::lineWidthMmToIndex(circle.strokeWidth);
    altiumEllipse.lineStyle = toAltiumLineStyle(circle.strokeStyle);
    altiumEllipse.color = toAltiumColor(circle.strokeColor);
    altiumEllipse.areaColor = circle.isFilled ? toAltiumColor(circle.fillColor) : 0xFFFFFF;
    altiumEllipse.isSolid = circle.isFilled;
    altiumEllipse.ownerPartId = toAltiumOwnerPartId(circle.partIndex);
    return altiumEllipse;
}

/**
 * @brief SymbolArcIR → AltiumSchArc
 */
AltiumSchArc ExporterAltiumSymbol::convertArc(const IR::SymbolArcIR& arc) {
    AltiumSchArc altiumArc;
    // 三点确定圆弧。退化为共线时，以首尾中点作为安全回退。
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
    double dx = arc.startPoint.x() - center.x();
    double dy = arc.startPoint.y() - center.y();
    double radius = std::sqrt(dx * dx + dy * dy);
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

/**
 * @brief SymbolPolygonIR → AltiumSchPolygon
 */
AltiumSchPolygon ExporterAltiumSymbol::convertPolygon(const IR::SymbolPolygonIR& polygon) {
    AltiumSchPolygon altiumPolygon;
    altiumPolygon.lineWidth = AltiumCoord::lineWidthMmToIndex(polygon.strokeWidth);
    altiumPolygon.lineStyle = toAltiumLineStyle(polygon.strokeStyle);
    altiumPolygon.color = toAltiumColor(polygon.strokeColor);
    altiumPolygon.areaColor = polygon.isFilled ? toAltiumColor(polygon.fillColor) : 0xFFFFFF;
    altiumPolygon.isSolid = polygon.isFilled;
    altiumPolygon.ownerPartId = toAltiumOwnerPartId(polygon.partIndex);

    for (const QPointF& point : polygon.points) {
        altiumPolygon.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    }
    return altiumPolygon;
}

/**
 * @brief SymbolPolylineIR → AltiumSchPolyline
 */
AltiumSchPolyline ExporterAltiumSymbol::convertPolyline(const IR::SymbolPolylineIR& polyline) {
    AltiumSchPolyline altiumPolyline;
    altiumPolyline.lineWidth = AltiumCoord::lineWidthMmToIndex(polyline.strokeWidth);
    altiumPolyline.lineStyle = toAltiumLineStyle(polyline.strokeStyle);
    altiumPolyline.color = toAltiumColor(polyline.strokeColor);
    altiumPolyline.ownerPartId = toAltiumOwnerPartId(polyline.partIndex);

    for (const QPointF& point : polyline.points) {
        altiumPolyline.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    }
    return altiumPolyline;
}

/**
 * @brief SymbolPathIR → AltiumSchPath
 */
AltiumSchPath ExporterAltiumSymbol::convertPath(const IR::SymbolPathIR& path) {
    AltiumSchPath altiumPath;
    altiumPath.lineWidth = AltiumCoord::lineWidthMmToIndex(path.strokeWidth);
    altiumPath.lineStyle = toAltiumLineStyle(path.strokeStyle);
    altiumPath.color = toAltiumColor(path.strokeColor);
    altiumPath.ownerPartId = toAltiumOwnerPartId(path.partIndex);

    for (const QPointF& point : path.points) {
        altiumPath.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    }
    return altiumPath;
}

/**
 * @brief SymbolBezierIR → AltiumSchBezier
 */
AltiumSchBezier ExporterAltiumSymbol::convertBezier(const IR::SymbolBezierIR& bezier) {
    AltiumSchBezier altiumBezier;
    altiumBezier.lineWidth = AltiumCoord::lineWidthMmToIndex(bezier.strokeWidth);
    altiumBezier.color = toAltiumColor(bezier.strokeColor);
    altiumBezier.ownerPartId = toAltiumOwnerPartId(bezier.partIndex);
    for (const QPointF& point : bezier.controlPoints) {
        altiumBezier.controlPoints.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    }
    return altiumBezier;
}

/**
 * @brief SymbolIeeeIR → AltiumSchIeee
 */
AltiumSchIeee ExporterAltiumSymbol::convertIeee(const IR::SymbolIeeeIR& ieee) {
    AltiumSchIeee altiumIeee;
    altiumIeee.symbol = qBound(0, ieee.symbol, 34);
    altiumIeee.locationX = AltiumCoord::mmToRaw(ieee.position.x());
    altiumIeee.locationY = AltiumCoord::mmToRaw(ieee.position.y());
    altiumIeee.scaleFactor = qMax(1, ieee.scaleFactor);
    altiumIeee.orientation = ((ieee.orientation % 4) + 4) % 4;
    altiumIeee.mirrored = ieee.mirrored;
    altiumIeee.color = toAltiumColor(ieee.color);
    altiumIeee.ownerPartId = toAltiumOwnerPartId(ieee.partIndex);
    return altiumIeee;
}

/**
 * @brief SymbolTextIR → AltiumSchText
 */
AltiumSchText ExporterAltiumSymbol::convertText(const IR::SymbolTextIR& text) {
    AltiumSchText altiumText;
    altiumText.locationX = AltiumCoord::mmToRaw(text.position.x());
    altiumText.locationY = AltiumCoord::mmToRaw(text.position.y());
    altiumText.text = text.text;
    altiumText.fontId = 0;
    altiumText.fontName = text.fontFamily;
    altiumText.fontSizeMm = text.fontSizeMm;
    altiumText.bold = text.bold;
    altiumText.italic = text.italic;
    altiumText.anchor = text.anchor.trimmed().isEmpty() ? QStringLiteral("middle") : text.anchor.trimmed();
    altiumText.color = toAltiumColor(text.color);
    altiumText.isHidden = !text.visible;
    altiumText.orientation = toAltiumOrientation(text.rotation);
    altiumText.ownerPartId = toAltiumOwnerPartId(text.partIndex);
    return altiumText;
}

/**
 * @brief SymbolTextFrameIR → AltiumSchTextFrame
 */
AltiumSchTextFrame ExporterAltiumSymbol::convertTextFrame(const IR::SymbolTextFrameIR& frame) {
    AltiumSchTextFrame altiumFrame;
    altiumFrame.locationX = AltiumCoord::mmToRaw(frame.x0);
    altiumFrame.locationY = AltiumCoord::mmToRaw(frame.y0);
    altiumFrame.cornerX = AltiumCoord::mmToRaw(frame.x1);
    altiumFrame.cornerY = AltiumCoord::mmToRaw(frame.y1);
    altiumFrame.lineWidth = AltiumCoord::lineWidthMmToIndex(frame.strokeWidth);
    altiumFrame.lineStyle = toAltiumLineStyle(frame.strokeStyle);
    altiumFrame.color = toAltiumColor(frame.strokeColor);
    altiumFrame.areaColor = frame.isFilled ? toAltiumColor(frame.fillColor) : 0;
    altiumFrame.textColor = toAltiumColor(frame.textColor);
    altiumFrame.fontName = frame.fontFamily;
    altiumFrame.fontSizeMm = frame.fontSizeMm;
    altiumFrame.bold = frame.bold;
    altiumFrame.italic = frame.italic;
    altiumFrame.fontId = qMax(0, frame.fontId);
    altiumFrame.orientation = ((frame.orientation % 4) + 4) % 4;
    altiumFrame.alignment = qMax(0, frame.alignment);
    altiumFrame.textMargin = AltiumCoord::mmToRaw(qMax(0.0, frame.textMargin));
    altiumFrame.text = frame.text;
    altiumFrame.isSolid = frame.isFilled;
    altiumFrame.showBorder = frame.showBorder;
    altiumFrame.wordWrap = frame.wordWrap;
    altiumFrame.clipToRect = frame.clipToRect;
    altiumFrame.transparent = frame.transparent;
    altiumFrame.ownerPartId = toAltiumOwnerPartId(frame.partIndex);
    return altiumFrame;
}

/**
 * @brief SymbolImageIR → AltiumSchImage
 */
AltiumSchImage ExporterAltiumSymbol::convertImage(const IR::SymbolImageIR& image) {
    AltiumSchImage altiumImage;
    altiumImage.locationX = AltiumCoord::mmToRaw(image.x0);
    altiumImage.locationY = AltiumCoord::mmToRaw(image.y0);
    altiumImage.cornerX = AltiumCoord::mmToRaw(image.x1);
    altiumImage.cornerY = AltiumCoord::mmToRaw(image.y1);
    altiumImage.rotation = image.rotation;
    altiumImage.lineWidth = AltiumCoord::lineWidthMmToIndex(image.strokeWidth);
    altiumImage.lineStyle = toAltiumLineStyle(image.strokeStyle);
    altiumImage.color = toAltiumColor(image.strokeColor);
    altiumImage.areaColor = image.isFilled ? toAltiumColor(image.fillColor) : 0;
    altiumImage.fileName = image.fileName;
    altiumImage.data = image.data;
    altiumImage.isSolid = image.isFilled;
    altiumImage.transparent = image.transparent;
    altiumImage.showBorder = image.showBorder;
    altiumImage.keepAspect = image.keepAspect;
    altiumImage.embedImage = !image.data.isEmpty();
    altiumImage.ownerPartId = toAltiumOwnerPartId(image.partIndex);
    return altiumImage;
}

/**
 * @brief SymbolEllipseIR → AltiumSchEllipse
 */
AltiumSchEllipse ExporterAltiumSymbol::convertEllipse(const IR::SymbolEllipseIR& ellipse) {
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

/**
 * @brief SymbolPieIR → AltiumSchPie
 */
AltiumSchPie ExporterAltiumSymbol::convertPie(const IR::SymbolPieIR& pie) {
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

/**
 * @brief SymbolEllipticalArcIR → AltiumSchEllipticalArc
 */
AltiumSchEllipticalArc ExporterAltiumSymbol::convertEllipticalArc(const IR::SymbolEllipticalArcIR& arc) {
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

/** @brief 将符号图元委托给独立的几何归一化器。 */
void ExporterAltiumSymbol::centerComponent(AltiumSchComponent& component) {
    AltiumSchSymbolGeometryNormalizer::normalize(component);
}

}  // namespace EasyKiConverter
