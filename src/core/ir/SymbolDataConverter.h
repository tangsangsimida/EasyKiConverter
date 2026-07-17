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

#include <QStringList>

namespace EasyKiConverter {
namespace IR {

/** @brief EasyEDA 坐标单位 -> mm 转换因子（1 EasyEDA 单位 = 10 mil = 0.254 mm） */
constexpr double PX_TO_MM = 0.254;

/** @brief pt -> mm 转换因子 */
constexpr double PT_TO_MM = 0.352778;

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
            const int hIdx = pathStr.indexOf('h');
            if (hIdx >= 0) {
                bool ok = false;
                const double len = pathStr.mid(hIdx + 1).toDouble(&ok);
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
            rir.bounds = QRectF((rect.posX - originX) * PX_TO_MM,
                                -(rect.posY - originY + rect.height) * PX_TO_MM,  // Y 翻转
                                rect.width * PX_TO_MM,
                                rect.height * PX_TO_MM);
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

    // 处理单部件或多部件符号
    if (data.isMultiPart()) {
        ir.partCount = data.parts().size();
        int partIdx = 0;
        for (const auto& part : data.parts()) {
            const double ox = part.originX;
            const double oy = part.originY;
            convertPins(part.pins, ox, oy, partIdx);
            convertRectangles(part.rectangles, ox, oy, partIdx);
            convertCircles(part.circles, ox, oy, partIdx);
            convertEllipses(part.ellipses, ox, oy, partIdx);
            convertPolylines(part.polylines, ox, oy, partIdx);
            convertPolygons(part.polygons, ox, oy, partIdx);
            convertPaths(part.paths, ox, oy, partIdx);
            convertTexts(part.texts, ox, oy, partIdx);
            ++partIdx;
        }
    } else {
        const double ox = data.bbox().x;
        const double oy = data.bbox().y;
        convertPins(data.pins(), ox, oy);
        convertRectangles(data.rectangles(), ox, oy);
        convertCircles(data.circles(), ox, oy);
        convertEllipses(data.ellipses(), ox, oy);
        convertPolylines(data.polylines(), ox, oy);
        convertPolygons(data.polygons(), ox, oy);
        convertPaths(data.paths(), ox, oy);
        convertTexts(data.texts(), ox, oy);
    }

    return ir;
}

}  // namespace IR
}  // namespace EasyKiConverter
