#pragma once

/**
 * @file SymbolDataConverter.h
 * @brief SymbolData -> SymbolComponentIR 转换桥接
 *
 * 提供从旧模型 SymbolData 到 IR SymbolComponentIR 的转换函数。
 * 负责：
 * - 坐标字符串解析（"x1 y1 x2 y2" -> QList<QPointF>）
 * - SVG 路径解析
 * - 引脚类型/方向映射（EasyedaPinTypeMap）
 * - EasyEDA 特有字段移入 sourceMetadata
 * - 单位转换（px -> mm）
 */

#include "EasyedaPinTypeMap.h"
#include "SymbolIR.h"
#include "models/SymbolData.h"

#include <QRegularExpression>
#include <QStringList>

namespace EasyKiConverter {
namespace IR {

/** @brief 向后兼容别名，新代码请使用 IR::EASYEDA_PX_TO_MM */
constexpr double PX_TO_MM = EASYEDA_PX_TO_MM;

/** @brief 向后兼容别名，新代码请使用 IR::EASYEDA_PT_TO_MM */
constexpr double PT_TO_MM = EASYEDA_PT_TO_MM;

/**
 * @brief 解析 EasyEDA 空格分隔的扁平坐标字符串
 * @param pointsStr "x1 y1 x2 y2 x3 y3" 格式
 * @param scaleFactor 缩放因子（默认 PX_TO_MM = 0.254）
 * @return 解析后的点列表
 */
inline QList<QPointF> parseFlatPointString(const QString& pointsStr, double scaleFactor = PX_TO_MM) {
    QList<QPointF> points;
    const QStringList parts = pointsStr.trimmed().split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); i += 2) {
        bool ok1 = false, ok2 = false;
        const double x = parts[i].toDouble(&ok1);
        const double y = parts[i + 1].toDouble(&ok2);
        if (ok1 && ok2) {
            points.append(QPointF(x * scaleFactor, y * scaleFactor));
        }
    }
    return points;
}

/**
 * @brief 解析 EasyEDA 逗号分隔的坐标字符串
 * @param pointsStr "x1,y1 x2,y2 x3,y3" 格式
 * @param scaleFactor 缩放因子
 * @return 解析后的点列表
 */
inline QList<QPointF> parseCommaSeparatedPoints(const QString& pointsStr, double scaleFactor = PX_TO_MM) {
    QList<QPointF> points;
    const QStringList pairs = pointsStr.trimmed().split(' ', Qt::SkipEmptyParts);
    for (const auto& pair : pairs) {
        const QStringList xy = pair.split(',');
        if (xy.size() >= 2) {
            bool ok1 = false, ok2 = false;
            const double x = xy[0].toDouble(&ok1);
            const double y = xy[1].toDouble(&ok2);
            if (ok1 && ok2) {
                points.append(QPointF(x * scaleFactor, y * scaleFactor));
            }
        }
    }
    return points;
}

/**
 * @brief 解析 SVG 路径字符串为点序列
 *
 * 简化解析：提取 M/L 命令后的坐标对，忽略 C/Z 等复杂命令。
 * 适用于 EasyEDA 的简单 SVG 路径。
 *
 * @param pathStr SVG 路径字符串（如 "M 0 0 L 100 0 L 100 100 Z"）
 * @param scaleFactor 缩放因子
 * @return 解析后的点列表
 */
inline QList<QPointF> parseSimpleSvgPath(const QString& pathStr, double scaleFactor = PX_TO_MM) {
    QList<QPointF> points;
    QString cleaned = pathStr;
    // 将命令字母替换为空格，只保留数字和分隔符
    for (const QChar& ch : QString("MmLlHhVvCcSsQqTtAaZz")) {
        cleaned.replace(ch, ' ');
    }
    const QStringList parts = cleaned.trimmed().split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); i += 2) {
        bool ok1 = false, ok2 = false;
        const double x = parts[i].toDouble(&ok1);
        const double y = parts[i + 1].toDouble(&ok2);
        if (ok1 && ok2) {
            points.append(QPointF(x * scaleFactor, y * scaleFactor));
        }
    }
    return points;
}

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

    // 转换辅助 lambda：处理单个 part 的原语
    auto convertPins = [&](const QList<SymbolPin>& pins, double originX, double originY, int partIdx = 0) {
        for (const auto& pin : pins) {
            SymbolPinIR pir;
            pir.name = pin.name.text;
            pir.designator = pin.settings.spicePinNumber;
            pir.position = QPointF((pin.settings.posX - originX) * PX_TO_MM,
                                   -(pin.settings.posY - originY) * PX_TO_MM  // Y 轴翻转
            );
            // 从 pinPath SVG 解析引脚长度
            const QString pathStr = pin.pinPath.path;
            // EasyEDA 引脚路径既可能使用相对水平/垂直线段，也可能使用大写命令。
            // 只识别 h 会让所有垂直引脚退回默认长度，进而破坏符号比例。
            static const QRegularExpression lengthExpression(QStringLiteral(R"([hHvV]\s*([-+]?(?:\d+\.?\d*|\.\d+)))"));
            const QRegularExpressionMatch lengthMatch = lengthExpression.match(pathStr);
            if (lengthMatch.hasMatch()) {
                bool ok = false;
                const double len = lengthMatch.captured(1).toDouble(&ok);
                if (ok)
                    pir.length = qAbs(len) * PX_TO_MM;
            }
            pir.direction = EasyedaPinTypeMap::toPinDirection(pin.settings.rotation);
            pir.electricalType = EasyedaPinTypeMap::toPinElectricalType(static_cast<int>(pin.settings.type));
            pir.showName = pin.name.isDisplayed;
            pir.showDesignator = pin.settings.isDisplayed;
            pir.hasDot = pin.dot.isDisplayed;
            pir.hasClock = pin.clock.isDisplayed;
            pir.partIndex = partIdx;
            ir.pins.append(pir);
        }
    };

    auto convertRectangles = [&](const QList<SymbolRectangle>& rects, double originX, double originY, int partIdx = 0) {
        for (const auto& rect : rects) {
            SymbolRectangleIR rir;
            // 使用与旧代码相同的坐标计算方式：先减去原点，再转换单位并翻转 Y
            rir.x0 = (rect.posX - originX) * PX_TO_MM;
            rir.y0 = -(rect.posY - originY) * PX_TO_MM;  // Y 翻转
            rir.x1 = (rect.posX + rect.width - originX) * PX_TO_MM;
            rir.y1 = -(rect.posY + rect.height - originY) * PX_TO_MM;  // Y 翻转
            rir.strokeColor = parseColor(rect.strokeColor);
            rir.strokeWidth = rect.strokeWidth * PX_TO_MM;
            rir.isFilled = !rect.fillColor.isEmpty() && rect.fillColor != "none";
            if (rir.isFilled)
                rir.fillColor = parseColor(rect.fillColor);
            rir.partIndex = partIdx;
            ir.rectangles.append(rir);
        }
    };

    auto convertCircles = [&](const QList<SymbolCircle>& circles, double originX, double originY, int partIdx = 0) {
        for (const auto& circle : circles) {
            SymbolCircleIR cir;
            cir.center = QPointF((circle.centerX - originX) * PX_TO_MM, -(circle.centerY - originY) * PX_TO_MM);
            cir.radius = circle.radius * PX_TO_MM;
            cir.strokeColor = parseColor(circle.strokeColor);
            cir.strokeWidth = circle.strokeWidth * PX_TO_MM;
            cir.isFilled = circle.fillColor;
            cir.partIndex = partIdx;
            ir.circles.append(cir);
        }
    };

    auto convertEllipses = [&](const QList<SymbolEllipse>& ellipses, double originX, double originY, int partIdx = 0) {
        for (const auto& ellipse : ellipses) {
            SymbolEllipseIR eir;
            eir.center = QPointF((ellipse.centerX - originX) * PX_TO_MM, -(ellipse.centerY - originY) * PX_TO_MM);
            eir.radiusX = ellipse.radiusX * PX_TO_MM;
            eir.radiusY = ellipse.radiusY * PX_TO_MM;
            eir.strokeColor = parseColor(ellipse.strokeColor);
            eir.strokeWidth = ellipse.strokeWidth * PX_TO_MM;
            eir.isFilled = ellipse.fillColor;
            eir.partIndex = partIdx;
            ir.ellipses.append(eir);
        }
    };

    auto convertPolylines =
        [&](const QList<SymbolPolyline>& polylines, double originX, double originY, int partIdx = 0) {
            for (const auto& pl : polylines) {
                SymbolPolylineIR plir;
                plir.points = parseFlatPointString(pl.points);
                // 坐标原点偏移和 Y 翻转
                for (auto& pt : plir.points) {
                    pt.setX(pt.x() - originX * PX_TO_MM);
                    pt.setY(-(pt.y() - originY * PX_TO_MM));
                }
                plir.strokeColor = parseColor(pl.strokeColor);
                plir.strokeWidth = pl.strokeWidth * PX_TO_MM;
                plir.isFilled = pl.fillColor;
                plir.partIndex = partIdx;
                ir.polylines.append(plir);
            }
        };

    auto convertPolygons = [&](const QList<SymbolPolygon>& polygons, double originX, double originY, int partIdx = 0) {
        for (const auto& pg : polygons) {
            SymbolPolygonIR pgir;
            pgir.points = parseFlatPointString(pg.points);
            for (auto& pt : pgir.points) {
                pt.setX(pt.x() - originX * PX_TO_MM);
                pt.setY(-(pt.y() - originY * PX_TO_MM));
            }
            pgir.strokeColor = parseColor(pg.strokeColor);
            pgir.strokeWidth = pg.strokeWidth * PX_TO_MM;
            pgir.isFilled = pg.fillColor;
            pgir.partIndex = partIdx;
            ir.polygons.append(pgir);
        }
    };

    auto convertPaths = [&](const QList<SymbolPath>& paths, double originX, double originY, int partIdx = 0) {
        for (const auto& path : paths) {
            SymbolPathIR pathIR;
            pathIR.points = parseSimpleSvgPath(path.paths);
            for (auto& pt : pathIR.points) {
                pt.setX(pt.x() - originX * PX_TO_MM);
                pt.setY(-(pt.y() - originY * PX_TO_MM));
            }
            // SVG Z 命令闭合路径：如果原始路径包含 Z，添加起点作为终点
            if (path.paths.contains('Z', Qt::CaseInsensitive) && pathIR.points.size() >= 2) {
                if (pathIR.points.first() != pathIR.points.last()) {
                    pathIR.points.append(pathIR.points.first());
                }
            }
            pathIR.strokeColor = parseColor(path.strokeColor);
            pathIR.strokeWidth = path.strokeWidth * PX_TO_MM;
            pathIR.isFilled = path.fillColor;
            pathIR.partIndex = partIdx;
            ir.paths.append(pathIR);
        }
    };

    auto convertTexts = [&](const QList<SymbolText>& texts, double originX, double originY, int partIdx = 0) {
        for (const auto& text : texts) {
            SymbolTextIR tir;
            tir.text = text.text;
            tir.position = QPointF((text.posX - originX) * PX_TO_MM, -(text.posY - originY) * PX_TO_MM);
            tir.rotation = text.rotation;
            tir.color = parseColor(text.color);
            tir.fontFamily = text.font;
            tir.fontSizeMm = text.textSize * PT_TO_MM;
            tir.bold = text.bold;
            tir.italic = (text.italic == "1" || text.italic == "Italic" || text.italic == "italic");
            tir.visible = text.visible;
            tir.partIndex = partIdx;
            ir.texts.append(tir);
        }
    };

    auto convertArcs = [&](const QList<SymbolArc>& arcs, double originX, double originY, int partIdx = 0) {
        for (const auto& arc : arcs) {
            SymbolArcIR air;
            // SymbolArc.path 存储3 个点：起点、中点、终点
            if (arc.path.size() >= 3) {
                int midIdx = arc.path.size() / 2;
                air.startPoint =
                    QPointF((arc.path.first().x() - originX) * PX_TO_MM, -(arc.path.first().y() - originY) * PX_TO_MM);
                air.midPoint =
                    QPointF((arc.path[midIdx].x() - originX) * PX_TO_MM, -(arc.path[midIdx].y() - originY) * PX_TO_MM);
                air.endPoint =
                    QPointF((arc.path.last().x() - originX) * PX_TO_MM, -(arc.path.last().y() - originY) * PX_TO_MM);
            } else if (arc.path.size() == 2) {
                air.startPoint =
                    QPointF((arc.path.first().x() - originX) * PX_TO_MM, -(arc.path.first().y() - originY) * PX_TO_MM);
                QPointF mid = (arc.path.first() + arc.path.last()) / 2.0;
                air.midPoint = QPointF((mid.x() - originX) * PX_TO_MM, -(mid.y() - originY) * PX_TO_MM);
                air.endPoint =
                    QPointF((arc.path.last().x() - originX) * PX_TO_MM, -(arc.path.last().y() - originY) * PX_TO_MM);
            }
            air.strokeColor = parseColor(arc.strokeColor);
            air.strokeWidth = arc.strokeWidth * PX_TO_MM;
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
            // 引脚使用 part 的几何边界框原点（与旧代码的 calculatePartBBox 一致）
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

            // 引脚使用几何边界框原点
            convertPins(part.pins, pox, poy, partIdx);
            // 所有图元和引脚必须共享同一个局部原点，避免进入目标导出器前就发生错位。
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
