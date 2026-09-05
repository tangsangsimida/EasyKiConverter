#pragma once

/**
 * @file IrBuilder.h
 * @brief SymbolData -> SymbolComponentIR 的 IR 构建器
 *
 * 从 SymbolDataConverter.h 中提取的 IR 构建逻辑。
 * 负责：
 * - 引脚类型/方向映射（EasyedaPinTypeMap）
 * - 引脚样式/显示字段填充（SymbolPinStyle / SymbolPinDisplay）
 * - 图元坐标变换和 IR 构建
 * - EasyEDA 特有字段 -> sourceMetadata
 * - 多部件符号原点计算
 *
 * 不包含坐标解析工具函数（这些在 GeometryNormalizer.h 中）。
 */

#include "EasyedaPinTypeMap.h"
#include "GeometryNormalizer.h"
#include "SymbolIR.h"
#include "models/SymbolData.h"

#include <QRegularExpression>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief SymbolData -> SymbolComponentIR 转换
 * @param data 旧模型的符号数据
 * @return IR 的符号组件数据
 */
inline SymbolComponentIR toSymbolIR(const SymbolData& data) {
    SymbolComponentIR ir;

    // 通用元数据
    ir.name = data.info().name;
    ir.description = data.info().description;
    ir.designatorPrefix = data.info().prefix;

    // 封装关联
    ir.footprintName = data.info().package;

    // EasyEDA 特有字段 -> sourceMetadata
    if (!data.info().lcscId.isEmpty())
        ir.sourceMetadata["lcscId"] = data.info().lcscId;
    if (!data.info().jlcId.isEmpty())
        ir.sourceMetadata["jlcId"] = data.info().jlcId;
    if (!data.info().uuid.isEmpty())
        ir.sourceMetadata["uuid"] = data.info().uuid;
    if (!data.info().docType.isEmpty())
        ir.sourceMetadata["docType"] = data.info().docType;
    if (!data.info().datastrid.isEmpty())
        ir.sourceMetadata["datastrid"] = data.info().datastrid;
    if (!data.info().supplierPart.isEmpty())
        ir.sourceMetadata["supplierPart"] = data.info().supplierPart;
    if (!data.info().supplier.isEmpty())
        ir.sourceMetadata["supplier"] = data.info().supplier;
    if (!data.info().manufacturerPart.isEmpty())
        ir.sourceMetadata["manufacturerPart"] = data.info().manufacturerPart;
    if (!data.info().jlcpcbPartClass.isEmpty())
        ir.sourceMetadata["jlcpcbPartClass"] = data.info().jlcpcbPartClass;

    // 转换辅助 lambda：处理单个 part 的引脚
    auto convertPins = [&](const QList<SymbolPin>& pins, double originX, double originY, int partIdx = 0) {
        for (const auto& pin : pins) {
            SymbolPinIR pir;
            pir.name = pin.name.text;
            pir.designator = pin.settings.spicePinNumber;
            pir.position = GeometryNormalizer::transformPoint(pin.settings.posX, pin.settings.posY, originX, originY);

            // 从 pinPath SVG 解析引脚长度
            const QString pathStr = pin.pinPath.path;
            static const QRegularExpression lengthExpression(
                QStringLiteral(R"([hHvV]\s*([-+]?(?:\d+\.?\d*|\.\d+)))"));
            const QRegularExpressionMatch lengthMatch = lengthExpression.match(pathStr);
            if (lengthMatch.hasMatch()) {
                bool ok = false;
                const double len = lengthMatch.captured(1).toDouble(&ok);
                if (ok)
                    pir.length = qAbs(len) * EASYEDA_PX_TO_MM;
            }

            pir.direction = EasyedaPinTypeMap::toPinDirection(pin.settings.rotation);
            pir.electricalType = EasyedaPinTypeMap::toPinElectricalType(static_cast<int>(pin.settings.type));

            // 显示控制
            pir.showName = pin.name.isDisplayed;
            pir.showDesignator = pin.settings.isDisplayed;
            pir.display.showName = pin.name.isDisplayed;
            pir.display.showDesignator = pin.settings.isDisplayed;

            // 引脚样式（语义层）
            pir.hasDot = pin.dot.isDisplayed;
            pir.hasClock = pin.clock.isDisplayed;
            pir.style.inverted = pin.dot.isDisplayed;
            pir.style.clock = pin.clock.isDisplayed;
            if (pin.dot.isDisplayed && pin.clock.isDisplayed) {
                pir.style.decoration = PinDecoration::InvertedClock;
            } else if (pin.dot.isDisplayed) {
                pir.style.decoration = PinDecoration::Dot;
            } else if (pin.clock.isDisplayed) {
                pir.style.decoration = PinDecoration::Clock;
            }

            pir.partIndex = partIdx;
            ir.pins.append(pir);
        }
    };

    // 转换辅助 lambda：处理矩形
    auto convertRectangles = [&](const QList<SymbolRectangle>& rects, double originX, double originY, int partIdx = 0) {
        for (const auto& rect : rects) {
            SymbolRectangleIR rir;
            rir.x0 = (rect.posX - originX) * EASYEDA_PX_TO_MM;
            rir.y0 = -(rect.posY - originY) * EASYEDA_PX_TO_MM;
            rir.x1 = (rect.posX + rect.width - originX) * EASYEDA_PX_TO_MM;
            rir.y1 = -(rect.posY + rect.height - originY) * EASYEDA_PX_TO_MM;
            rir.strokeColor = parseColor(rect.strokeColor);
            rir.strokeWidth = rect.strokeWidth * EASYEDA_PX_TO_MM;
            rir.isFilled = !rect.fillColor.isEmpty() && rect.fillColor != "none";
            if (rir.isFilled)
                rir.fillColor = parseColor(rect.fillColor);
            rir.partIndex = partIdx;
            ir.rectangles.append(rir);
        }
    };

    // 转换辅助 lambda：处理圆形
    auto convertCircles = [&](const QList<SymbolCircle>& circles, double originX, double originY, int partIdx = 0) {
        for (const auto& circle : circles) {
            SymbolCircleIR cir;
            cir.center = QPointF((circle.centerX - originX) * EASYEDA_PX_TO_MM,
                                 -(circle.centerY - originY) * EASYEDA_PX_TO_MM);
            cir.radius = circle.radius * EASYEDA_PX_TO_MM;
            cir.strokeColor = parseColor(circle.strokeColor);
            cir.strokeWidth = circle.strokeWidth * EASYEDA_PX_TO_MM;
            cir.isFilled = circle.fillColor;
            cir.partIndex = partIdx;
            ir.circles.append(cir);
        }
    };

    // 转换辅助 lambda：处理椭圆
    auto convertEllipses = [&](const QList<SymbolEllipse>& ellipses, double originX, double originY, int partIdx = 0) {
        for (const auto& ellipse : ellipses) {
            SymbolEllipseIR eir;
            eir.center = QPointF((ellipse.centerX - originX) * EASYEDA_PX_TO_MM,
                                 -(ellipse.centerY - originY) * EASYEDA_PX_TO_MM);
            eir.radiusX = ellipse.radiusX * EASYEDA_PX_TO_MM;
            eir.radiusY = ellipse.radiusY * EASYEDA_PX_TO_MM;
            eir.strokeColor = parseColor(ellipse.strokeColor);
            eir.strokeWidth = ellipse.strokeWidth * EASYEDA_PX_TO_MM;
            eir.isFilled = ellipse.fillColor;
            eir.partIndex = partIdx;
            ir.ellipses.append(eir);
        }
    };

    // 转换辅助 lambda：处理折线
    auto convertPolylines =
        [&](const QList<SymbolPolyline>& polylines, double originX, double originY, int partIdx = 0) {
            for (const auto& pl : polylines) {
                SymbolPolylineIR plir;
                plir.points = GeometryNormalizer::parseFlatPointString(pl.points);
                QPointF originMm(originX * EASYEDA_PX_TO_MM, originY * EASYEDA_PX_TO_MM);
                plir.points = GeometryNormalizer::transformPoints(plir.points, originMm);
                plir.strokeColor = parseColor(pl.strokeColor);
                plir.strokeWidth = pl.strokeWidth * EASYEDA_PX_TO_MM;
                plir.isFilled = pl.fillColor;
                plir.partIndex = partIdx;
                ir.polylines.append(plir);
            }
        };

    // 转换辅助 lambda：处理多边形
    auto convertPolygons = [&](const QList<SymbolPolygon>& polygons, double originX, double originY, int partIdx = 0) {
        for (const auto& pg : polygons) {
            SymbolPolygonIR pgir;
            pgir.points = GeometryNormalizer::parseFlatPointString(pg.points);
            QPointF originMm(originX * EASYEDA_PX_TO_MM, originY * EASYEDA_PX_TO_MM);
            pgir.points = GeometryNormalizer::transformPoints(pgir.points, originMm);
            pgir.strokeColor = parseColor(pg.strokeColor);
            pgir.strokeWidth = pg.strokeWidth * EASYEDA_PX_TO_MM;
            pgir.isFilled = pg.fillColor;
            pgir.partIndex = partIdx;
            ir.polygons.append(pgir);
        }
    };

    // 转换辅助 lambda：处理路径
    auto convertPaths = [&](const QList<SymbolPath>& paths, double originX, double originY, int partIdx = 0) {
        for (const auto& path : paths) {
            SymbolPathIR pathIR;
            pathIR.points = GeometryNormalizer::parseSimpleSvgPath(path.paths);
            QPointF originMm(originX * EASYEDA_PX_TO_MM, originY * EASYEDA_PX_TO_MM);
            pathIR.points = GeometryNormalizer::transformPoints(pathIR.points, originMm);
            // SVG Z 命令闭合路径
            if (path.paths.contains('Z', Qt::CaseInsensitive) && pathIR.points.size() >= 2) {
                if (pathIR.points.first() != pathIR.points.last()) {
                    pathIR.points.append(pathIR.points.first());
                }
            }
            pathIR.strokeColor = parseColor(path.strokeColor);
            pathIR.strokeWidth = path.strokeWidth * EASYEDA_PX_TO_MM;
            pathIR.isFilled = path.fillColor;
            pathIR.partIndex = partIdx;
            ir.paths.append(pathIR);
        }
    };

    // 转换辅助 lambda：处理文本
    auto convertTexts = [&](const QList<SymbolText>& texts, double originX, double originY, int partIdx = 0) {
        for (const auto& text : texts) {
            SymbolTextIR tir;
            tir.text = text.text;
            tir.position = QPointF((text.posX - originX) * EASYEDA_PX_TO_MM,
                                   -(text.posY - originY) * EASYEDA_PX_TO_MM);
            tir.rotation = text.rotation;
            tir.color = parseColor(text.color);
            tir.fontFamily = text.font;
            tir.fontSizeMm = text.textSize * EASYEDA_PT_TO_MM;
            tir.bold = text.bold;
            tir.italic = (text.italic == "1" || text.italic == "Italic" || text.italic == "italic");
            tir.visible = text.visible;
            tir.partIndex = partIdx;
            ir.texts.append(tir);
        }
    };

    // 转换辅助 lambda：处理圆弧
    auto convertArcs = [&](const QList<SymbolArc>& arcs, double originX, double originY, int partIdx = 0) {
        for (const auto& arc : arcs) {
            SymbolArcIR air;
            if (arc.path.size() >= 3) {
                int midIdx = arc.path.size() / 2;
                air.startPoint = QPointF((arc.path.first().x() - originX) * EASYEDA_PX_TO_MM,
                                         -(arc.path.first().y() - originY) * EASYEDA_PX_TO_MM);
                air.midPoint = QPointF((arc.path[midIdx].x() - originX) * EASYEDA_PX_TO_MM,
                                       -(arc.path[midIdx].y() - originY) * EASYEDA_PX_TO_MM);
                air.endPoint = QPointF((arc.path.last().x() - originX) * EASYEDA_PX_TO_MM,
                                       -(arc.path.last().y() - originY) * EASYEDA_PX_TO_MM);
            } else if (arc.path.size() == 2) {
                air.startPoint = QPointF((arc.path.first().x() - originX) * EASYEDA_PX_TO_MM,
                                         -(arc.path.first().y() - originY) * EASYEDA_PX_TO_MM);
                QPointF mid = (arc.path.first() + arc.path.last()) / 2.0;
                air.midPoint = QPointF((mid.x() - originX) * EASYEDA_PX_TO_MM,
                                       -(mid.y() - originY) * EASYEDA_PX_TO_MM);
                air.endPoint = QPointF((arc.path.last().x() - originX) * EASYEDA_PX_TO_MM,
                                       -(arc.path.last().y() - originY) * EASYEDA_PX_TO_MM);
            }
            air.strokeColor = parseColor(arc.strokeColor);
            air.strokeWidth = arc.strokeWidth * EASYEDA_PX_TO_MM;
            air.isFilled = arc.fillColor;
            air.partIndex = partIdx;
            ir.arcs.append(air);
        }
    };

    // 处理单部件或多部件符号
    if (data.isMultiPart()) {
        ir.partCount = data.parts().size();
        int partIdx = 0;
        for (const auto& part : data.parts()) {
            // 计算该 part 的几何边界框原点
            double pox = part.originX;
            double poy = part.originY;
            {
                double minX = part.originX;
                double minY = part.originY;
                for (const auto& pin : part.pins) {
                    minX = qMin(minX, pin.settings.posX);
                    minY = qMin(minY, pin.settings.posY);
                }
                for (const auto& rect : part.rectangles) {
                    minX = qMin(minX, rect.posX);
                    minY = qMin(minY, rect.posY);
                }
                for (const auto& circle : part.circles) {
                    minX = qMin(minX, circle.centerX - circle.radius);
                    minY = qMin(minY, circle.centerY - circle.radius);
                }
                for (const auto& pg : part.polygons) {
                    QStringList pts = pg.points.split(" ");
                    pts.removeAll("");
                    for (int i = 0; i + 1 < pts.size(); i += 2) {
                        minX = qMin(minX, pts[i].toDouble());
                        minY = qMin(minY, pts[i + 1].toDouble());
                    }
                }
                for (const auto& pl : part.polylines) {
                    QStringList pts = pl.points.split(" ");
                    pts.removeAll("");
                    for (int i = 0; i + 1 < pts.size(); i += 2) {
                        minX = qMin(minX, pts[i].toDouble());
                        minY = qMin(minY, pts[i + 1].toDouble());
                    }
                }
                for (const auto& text : part.texts) {
                    minX = qMin(minX, text.posX);
                    minY = qMin(minY, text.posY);
                }
                if (minX < 1e30)
                    pox = minX;
                if (minY < 1e30)
                    poy = minY;
            }

            convertPins(part.pins, pox, poy, partIdx);
            convertRectangles(part.rectangles, pox, poy, partIdx);
            convertCircles(part.circles, pox, poy, partIdx);
            convertEllipses(part.ellipses, pox, poy, partIdx);
            convertArcs(part.arcs, pox, poy, partIdx);
            convertPolylines(part.polylines, pox, poy, partIdx);
            convertPolygons(part.polygons, pox, poy, partIdx);
            convertPaths(part.paths, pox, poy, partIdx);
            convertTexts(part.texts, pox, poy, partIdx);
            ++partIdx;
        }
    } else {
        const double ox = data.bbox().x;
        const double oy = data.bbox().y;
        convertPins(data.pins(), ox, oy);
        convertRectangles(data.rectangles(), ox, oy);
        convertCircles(data.circles(), ox, oy);
        convertEllipses(data.ellipses(), ox, oy);
        convertArcs(data.arcs(), ox, oy);
        convertPolylines(data.polylines(), ox, oy);
        convertPolygons(data.polygons(), ox, oy);
        convertPaths(data.paths(), ox, oy);
        convertTexts(data.texts(), ox, oy);
    }

    return ir;
}

}  // namespace IR
}  // namespace EasyKiConverter
