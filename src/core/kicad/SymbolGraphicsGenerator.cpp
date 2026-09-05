#include "SymbolGraphicsGenerator.h"

#include "KiCadTypeMap.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

QString SymbolGraphicsGenerator::generateDrawings(const IR::SymbolComponentIR& data) const {
    QString content;
    for (const IR::SymbolRectangleIR& rect : data.rectangles) {
        content += generateRectangle(rect);
    }
    for (const IR::SymbolCircleIR& circle : data.circles) {
        content += generateCircle(circle);
    }
    for (const IR::SymbolArcIR& arc : data.arcs) {
        content += generateArc(arc);
    }
    for (const IR::SymbolEllipseIR& ellipse : data.ellipses) {
        content += generateEllipse(ellipse);
    }
    for (const IR::SymbolPolygonIR& polygon : data.polygons) {
        content += generatePolygon(polygon);
    }
    for (const IR::SymbolPolylineIR& polyline : data.polylines) {
        content += generatePolyline(polyline);
    }
    for (const IR::SymbolPathIR& path : data.paths) {
        content += generatePath(path);
    }
    for (const IR::SymbolTextIR& text : data.texts) {
        content += generateText(text);
    }
    return content;
}

QString SymbolGraphicsGenerator::generatePins(const QList<IR::SymbolPinIR>& pins) const {
    QString content;
    for (const IR::SymbolPinIR& pin : pins) {
        content += generatePin(pin);
    }
    return content;
}

QString SymbolGraphicsGenerator::generatePin(const IR::SymbolPinIR& pin) const {
    QString content;

    // 坐标已为 mm，直接计算相对位置
    double x = pin.position.x() - m_originX;
    double y = pin.position.y() - m_originY;

    // 引脚长度（已为 mm）
    double length = pin.length;

    // 确保引脚长度为正数（KiCad 引脚长度必须是正数）
    length = std::abs(length);

    // 确保引脚长度不为 0
    if (length < 0.01) {
        length = 2.54;  // 默认引脚长度（100mil）
    }

    // 使用 IR 映射获取 KiCad 引脚电气类型
    const char* kicadPinType = KiCadTypeMap::toKicadPinType(pin.electricalType);

    // 引脚样式：从 IR 语义层获取
    QString kicadPinStyle = "line";
    if (pin.style.inverted && pin.style.clock) {
        kicadPinStyle = "inverted_clock";
    } else if (pin.style.inverted) {
        kicadPinStyle = "inverted";
    } else if (pin.style.clock) {
        kicadPinStyle = "clock";
    }

    // 处理引脚名称和编号
    QString pinName = pin.name;
    pinName.replace(" ", "");
    QString pinNumber = pin.designator;
    pinNumber.replace(" ", "");

    // 如果引脚名称为空，使用引脚编号
    if (pinName.isEmpty()) {
        pinName = pinNumber;
    }

    // 使用 IR 方向枚举获取 KiCad 角度
    double kicadOrientation = KiCadTypeMap::toKicadAngle(pin.direction);

    content += QString("    (pin %1 %2\n").arg(kicadPinType, kicadPinStyle);
    content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(kicadOrientation, 0, 'f', 0);
    content += QString("      (length %1)\n").arg(length, 0, 'f', 2);
    content += QString("      (name \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinName);
    content += QString("      (number \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinNumber);
    content += "    )\n";

    return content;
}

QString SymbolGraphicsGenerator::generateRectangle(const IR::SymbolRectangleIR& rect) const {
    QString content;

    // 坐标已为 mm，计算相对位置
    double x0 = rect.x0 - m_originX;
    double y0 = rect.y0 - m_originY;
    double x1 = rect.x1 - m_originX;
    double y1 = rect.y1 - m_originY;
    double strokeWidth = rect.strokeWidth;

    content += "    (rectangle\n";
    content += QString("      (start %1 %2)\n").arg(x0, 0, 'f', 2).arg(y0, 0, 'f', 2);
    content += QString("      (end %1 %2)\n").arg(x1, 0, 'f', 2).arg(y1, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

QString SymbolGraphicsGenerator::generateCircle(const IR::SymbolCircleIR& circle) const {
    QString content;

    // 坐标已为 mm，计算相对位置
    double cx = circle.center.x() - m_originX;
    double cy = circle.center.y() - m_originY;
    double radius = circle.radius;
    double strokeWidth = circle.strokeWidth;

    content += "    (circle\n";
    content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
    content += QString("      (radius %1)\n").arg(radius, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

QString SymbolGraphicsGenerator::generateArc(const IR::SymbolArcIR& arc) const {
    QString content;
    double strokeWidth = arc.strokeWidth;

    // 使用三点法生成 KiCad 圆弧（与旧代码一致）
    double startX = arc.startPoint.x() - m_originX;
    double startY = arc.startPoint.y() - m_originY;
    double midX = arc.midPoint.x() - m_originX;
    double midY = arc.midPoint.y() - m_originY;
    double endX = arc.endPoint.x() - m_originX;
    double endY = arc.endPoint.y() - m_originY;

    content += "    (arc\n";
    content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
    content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
    content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

    if (arc.isFilled) {
        content += "      (fill (type background))\n";
    } else {
        content += "      (fill (type none))\n";
    }

    content += "    )\n";

    return content;
}

QString SymbolGraphicsGenerator::generateEllipse(const IR::SymbolEllipseIR& ellipse) const {
    QString content;

    // 坐标已为 mm，计算相对位置
    double cx = ellipse.center.x() - m_originX;
    double cy = ellipse.center.y() - m_originY;
    double radiusX = ellipse.radiusX;
    double radiusY = ellipse.radiusY;
    double strokeWidth = ellipse.strokeWidth;

    // 如果是圆形（radiusX 等于 radiusY），使用 circle 元素
    if (qAbs(radiusX - radiusY) < 0.01) {
        content += "    (circle\n";
        content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
        content += QString("      (radius %1)\n").arg(radiusX, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        // 椭圆：使用 32 段折线近似
        content += "    (polyline\n";
        content += "      (pts";

        const int segments = 32;
        for (int i = 0; i <= segments; ++i) {
            double angle = 2.0 * M_PI * i / segments;
            double x = cx + radiusX * std::cos(angle);
            double y = cy + radiusY * std::sin(angle);
            content += QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
        }

        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

        if (ellipse.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

QString SymbolGraphicsGenerator::generatePolygon(const IR::SymbolPolygonIR& polygon) const {
    QString content;
    double strokeWidth = polygon.strokeWidth;

    // IR 中坐标已解析为 QList<QPointF>
    if (polygon.points.size() >= 2) {
        // KiCad V6 不支持 polygon 元素，使用 polyline 代替
        content += "    (polyline\n";
        content += "      (pts";
        QString firstPoint;
        QString lastPoint;
        for (const QPointF& pt : polygon.points) {
            double x = pt.x() - m_originX;
            double y = pt.y() - m_originY;
            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

            // 避免重复点
            if (point != lastPoint) {
                content += point;
                if (firstPoint.isEmpty()) {
                    firstPoint = point;
                }
                lastPoint = point;
            }
        }
        // 多边形总是重复第一个点以闭合
        if (!firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        if (polygon.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

QString SymbolGraphicsGenerator::generatePolyline(const IR::SymbolPolylineIR& polyline) const {
    QString content;
    double strokeWidth = polyline.strokeWidth;

    // IR 中坐标已解析为 QList<QPointF>
    if (polyline.points.size() >= 2) {
        content += "    (polyline\n";
        content += "      (pts";
        QString firstPoint;
        QString lastPoint;
        for (const QPointF& pt : polyline.points) {
            double x = pt.x() - m_originX;
            double y = pt.y() - m_originY;
            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

            // 避免重复点
            if (point != lastPoint) {
                content += point;
                if (firstPoint.isEmpty()) {
                    firstPoint = point;
                }
                lastPoint = point;
            }
        }
        // 只有 fillColor 为 true 时才重复第一个点
        if (polyline.isFilled && !firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        if (polyline.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

QString SymbolGraphicsGenerator::generatePath(const IR::SymbolPathIR& path) const {
    QString content;

    // IR 中路径坐标已解析为 QList<QPointF>
    if (!path.points.isEmpty()) {
        content += "    (polyline\n";
        content += "      (pts";

        QString lastPoint;
        for (const QPointF& pt : path.points) {
            // 坐标已为 mm，计算相对位置
            double x = pt.x() - m_originX;
            double y = pt.y() - m_originY;

            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

            // 避免重复点
            if (point != lastPoint) {
                content += point;
                lastPoint = point;
            }
        }

        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(path.strokeWidth, 0, 'f', 3);

        if (path.isFilled) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }

        content += "    )\n";
    } else {
        // 如果没有有效点，生成占位
        content += "    (polyline (pts (xy 0 0))\n";
        content += "      (stroke (width 0.127) (type default))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";
    }

    return content;
}

QString SymbolGraphicsGenerator::generateText(const IR::SymbolTextIR& text) const {
    QString content;

    // 坐标已为 mm，计算相对位置
    double x = text.position.x() - m_originX;
    double y = text.position.y() - m_originY;

    // IR 中 fontSizeMm 已从 pt 转换
    double fontSize = text.fontSizeMm;

    // 处理粗体和斜体（IR 中 italic 已为 bool）
    QString fontStyle = "";
    if (text.bold) {
        fontStyle += "bold ";
    }
    if (text.italic) {
        fontStyle += "italic ";
    }
    if (fontStyle.isEmpty()) {
        fontStyle = "normal";
    } else {
        fontStyle = fontStyle.trimmed();
    }

    // 处理旋转角度
    double rotation = text.rotation;
    if (rotation != 0) {
        rotation = 360 - rotation;
    }

    // 处理可见性
    QString hide = text.visible ? "" : "hide";

    // 转义文本内容
    QString textContent = text.text;
    textContent.replace(" ", "");
    if (textContent.isEmpty()) {
        textContent = "~";
    }

    // 生成文本元素
    content += "    (text\n";
    content += QString("      \"%1\"\n").arg(textContent);
    content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(rotation, 0, 'f', 0);
    content += QString("      (effects (font (size %1 %2) (thickness 0.1) %3) %4)\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontStyle)
                   .arg(hide);
    content += "    )\n";

    return content;
}

}  // namespace EasyKiConverter
