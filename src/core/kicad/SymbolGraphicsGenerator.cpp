#include "SymbolGraphicsGenerator.h"

#include "core/utils/GeometryUtils.h"
#include "core/utils/SvgPathParser.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>


namespace EasyKiConverter {

QString SymbolGraphicsGenerator::generateDrawings(const SymbolData& data) const {
    QString content;
    for (const SymbolRectangle& rect : data.rectangles()) {
        content += generateRectangle(rect);
    }
    for (const SymbolCircle& circle : data.circles()) {
        content += generateCircle(circle);
    }
    for (const SymbolArc& arc : data.arcs()) {
        content += generateArc(arc);
    }
    for (const SymbolEllipse& ellipse : data.ellipses()) {
        content += generateEllipse(ellipse);
    }
    for (const SymbolPolygon& polygon : data.polygons()) {
        content += generatePolygon(polygon);
    }
    for (const SymbolPolyline& polyline : data.polylines()) {
        content += generatePolyline(polyline);
    }
    for (const SymbolPath& path : data.paths()) {
        content += generatePath(path);
    }
    for (const SymbolText& text : data.texts()) {
        content += generateText(text);
    }
    return content;
}

QString SymbolGraphicsGenerator::generateDrawings(const SymbolPart& part) const {
    QString content;
    for (const SymbolRectangle& rect : part.rectangles) {
        content += generateRectangle(rect);
    }
    for (const SymbolCircle& circle : part.circles) {
        content += generateCircle(circle);
    }
    for (const SymbolArc& arc : part.arcs) {
        content += generateArc(arc);
    }
    for (const SymbolEllipse& ellipse : part.ellipses) {
        content += generateEllipse(ellipse);
    }
    for (const SymbolPolygon& polygon : part.polygons) {
        content += generatePolygon(polygon);
    }
    for (const SymbolPolyline& polyline : part.polylines) {
        content += generatePolyline(polyline);
    }
    for (const SymbolPath& path : part.paths) {
        content += generatePath(path);
    }
    for (const SymbolText& text : part.texts) {
        content += generateText(text);
    }
    return content;
}

QString SymbolGraphicsGenerator::generatePins(const QList<SymbolPin>& pins, const SymbolBBox& bbox) const {
    QString content;
    for (const SymbolPin& pin : pins) {
        content += generatePin(pin, bbox);
    }
    return content;
}

QString SymbolGraphicsGenerator::generatePin(const SymbolPin& pin, const SymbolBBox& bbox) const {
    QString content;

    // 使用边界框偏移量计算相对坐标
    double x = pxToMm(pin.settings.posX - bbox.x);
    double y = -pxToMm(pin.settings.posY - bbox.y);  // Y 轴翻转

    // 计算引脚长度（Python版本的做法：直接从路径字符串中提h'后面的数字）
    QString path = pin.pinPath.path;
    double length = 0;

    // 查找 'h' 命令并提取其后的数值作为引脚长度
    int hIndex = path.indexOf('h');
    if (hIndex >= 0) {
        QString lengthStr = path.mid(hIndex + 1);
        // 提取数值部分（可能包含其他命令）
        QStringList parts = lengthStr.split(QRegularExpression("[^0-9.-]"), Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            bool ok = false;
            double tempLength = parts[0].toDouble(&ok);
            if (ok) {
                length = tempLength;
            }
        }
    }

    // 转换为毫米单位
    length = pxToMm(length);

    // 调试：输出引脚信息
    qDebug() << "Pin Debug - Name:" << pin.name.text << "Number:" << pin.settings.spicePinNumber;
    qDebug() << "  Original posX:" << pin.settings.posX << "bbox.x:" << bbox.x;
    qDebug() << "  Calculated x:" << x << "y:" << y;
    qDebug() << "  Path:" << path << "Extracted length (px):" << length / 0.0254;
    qDebug() << "  Length (mm):" << length << "Rotation:" << pin.settings.rotation;

    // 确保引脚长度为正数（KiCad 引脚长度必须是正数）
    length = std::abs(length);

    // 确保引脚长度不为 0
    if (length < 0.01) {
        length = 2.54;  // 默认引脚长度（100mil）
    }

    // 直接使用原始引脚类型，不进行自动推断
    PinType pinType = pin.settings.type;
    QString kicadPinType = pinTypeToKicad(pinType);

    // 动态计算引脚样式（根据 dot 和 clock 的显示状态）
    PinStyle pinStyle = PinStyle::Line;
    if (pin.dot.isDisplayed && pin.clock.isDisplayed) {
        pinStyle = PinStyle::InvertedClock;
    } else if (pin.dot.isDisplayed) {
        pinStyle = PinStyle::Inverted;
    } else if (pin.clock.isDisplayed) {
        pinStyle = PinStyle::Clock;
    }
    QString kicadPinStyle = pinStyleToKicad(pinStyle);

    // 处理引脚名称和编
    QString pinName = pin.name.text;
    pinName.replace(" ", "");
    QString pinNumber = pin.settings.spicePinNumber;
    pinNumber.replace(" ", "");

    // 如果引脚名称为空，使用引脚编号
    if (pinName.isEmpty()) {
        pinName = pinNumber;
    }

    double orientation = pin.settings.rotation;

    // Python 版本使用 (180 + orientation) % 360 计算方向
    // 注意：orientation 为 0, 90, 180, 270 的整数
    double kicadOrientation = (180.0 + orientation);
    // 使用 fmod 替代 while 循环，避免死循环风险
    kicadOrientation = fmod(kicadOrientation, 360.0);
    if (kicadOrientation < 0.0) {
        kicadOrientation += 360.0;
    }

    // 规范化为 KiCad 要求的标准角度：0, 90, 180, 270
    // 找到最接近的标准角度
    double standardAngles[] = {0.0, 90.0, 180.0, 270.0};
    double closestAngle = 0.0;
    double minDiff = 360.0;
    for (double angle : standardAngles) {
        double diff = qAbs(kicadOrientation - angle);
        if (diff < minDiff) {
            minDiff = diff;
            closestAngle = angle;
        }
    }
    kicadOrientation = closestAngle;

    content += QString("    (pin %1 %2\n").arg(kicadPinType, kicadPinStyle);
    content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(kicadOrientation, 0, 'f', 0);
    content += QString("      (length %1)\n").arg(length, 0, 'f', 2);
    content += QString("      (name \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinName);
    content += QString("      (number \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinNumber);
    content += "    )\n";

    return content;
}

QString SymbolGraphicsGenerator::generateRectangle(const SymbolRectangle& rect) const {
    QString content;

    // V6 使用毫米单位
    // 使用原始矩形的坐标和尺寸
    double x0 = pxToMm(rect.posX - m_currentBBox.x);
    double y0 = -pxToMm(rect.posY - m_currentBBox.y);  // Y 轴翻转
    double x1 = pxToMm(rect.posX + rect.width - m_currentBBox.x);
    double y1 = -pxToMm(rect.posY + rect.height - m_currentBBox.y);  // Y 轴翻转
    double strokeWidth = pxToMm(rect.strokeWidth);

    content += "    (rectangle\n";
    content += QString("      (start %1 %2)\n").arg(x0, 0, 'f', 2).arg(y0, 0, 'f', 2);
    content += QString("      (end %1 %2)\n").arg(x1, 0, 'f', 2).arg(y1, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

QString SymbolGraphicsGenerator::generateCircle(const SymbolCircle& circle) const {
    QString content;

    // V6 使用毫米单位
    double cx = pxToMm(circle.centerX - m_currentBBox.x);
    double cy = -pxToMm(circle.centerY - m_currentBBox.y);  // Y 轴翻转
    double radius = pxToMm(circle.radius);
    double strokeWidth = pxToMm(circle.strokeWidth);

    content += "    (circle\n";
    content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
    content += QString("      (radius %1)\n").arg(radius, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

QString SymbolGraphicsGenerator::generateArc(const SymbolArc& arc) const {
    QString content;
    double strokeWidth = pxToMm(arc.strokeWidth);

    // KiCad V6 使用三点法定义圆弧：start、mid、end
    if (arc.path.size() >= 3) {
        // 提取起点、中点、终点
        const auto path = arc.path;  // 避免临时对象detach
        QPointF startPoint = path.first();
        QPointF endPoint = path.last();

        // 计算中点（取中间的点）
        int midIndex = path.size() / 2;
        QPointF midPoint = path[midIndex];

        // 转换为相对于边界框的坐标，并转换为毫米
        double startX = pxToMm(startPoint.x() - m_currentBBox.x);
        double startY = -pxToMm(startPoint.y() - m_currentBBox.y);  // Y 轴翻转
        double midX = pxToMm(midPoint.x() - m_currentBBox.x);
        double midY = -pxToMm(midPoint.y() - m_currentBBox.y);  // Y 轴翻转
        double endX = pxToMm(endPoint.x() - m_currentBBox.x);
        double endY = -pxToMm(endPoint.y() - m_currentBBox.y);  // Y 轴翻转

        content += "    (arc\n";
        content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
        content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
        content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

        // 根据fillColor设置填充类型（与Python版本一致）
        if (arc.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }

        content += "    )\n";
    } else if (arc.path.size() == 2) {
        // 只有两个点，计算中点
        const auto path = arc.path;  // 避免临时对象detach
        QPointF startPoint = path.first();
        QPointF endPoint = path.last();
        QPointF midPoint = (startPoint + endPoint) / 2;

        // 转换为相对于边界框的坐标，并转换为毫米
        double startX = pxToMm(startPoint.x() - m_currentBBox.x);
        double startY = -pxToMm(startPoint.y() - m_currentBBox.y);  // Y 轴翻转
        double midX = pxToMm(midPoint.x() - m_currentBBox.x);
        double midY = -pxToMm(midPoint.y() - m_currentBBox.y);  // Y 轴翻转
        double endX = pxToMm(endPoint.x() - m_currentBBox.x);
        double endY = -pxToMm(endPoint.y() - m_currentBBox.y);  // Y 轴翻转

        content += "    (arc\n";
        content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
        content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
        content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

        // 根据fillColor设置填充类型
        if (arc.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }

        content += "    )\n";
    } else {
        // 点数不足，跳过或生成一个默认的arc
        qDebug() << "Warning: Arc has insufficient points (" << arc.path.size() << "), skipping";
    }

    return content;
}

QString SymbolGraphicsGenerator::generateEllipse(const SymbolEllipse& ellipse) const {
    QString content;

    // V6 使用毫米单位
    double cx = pxToMm(ellipse.centerX - m_currentBBox.x);
    double cy = -pxToMm(ellipse.centerY - m_currentBBox.y);  // Y 轴翻转
    double radiusX = pxToMm(ellipse.radiusX);
    double radiusY = pxToMm(ellipse.radiusY);
    double strokeWidth = pxToMm(ellipse.strokeWidth);

    // 如果是圆形（radiusX 等于 radiusY），使用 circle 元素
    if (qAbs(radiusX - radiusY) < 0.01) {
        content += "    (circle\n";
        content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
        content += QString("      (radius %1)\n").arg(radiusX, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        // 椭圆：转换为路径
        // 使用 32 段折线近似椭圆
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

        // 根据 fillColor 属性设置填充类型
        if (ellipse.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

QString SymbolGraphicsGenerator::generatePolygon(const SymbolPolygon& polygon) const {
    QString content;
    double strokeWidth = pxToMm(polygon.strokeWidth);

    // 解析点数据
    QStringList points = polygon.points.split(" ");
    // 过滤掉空字符串
    points.removeAll("");

    // 至少需要2 个有效的点（4 个坐标值）
    if (points.size() >= 4) {
        // KiCad V6 不支持 polygon 元素，使用 polyline 代替
        content += "    (polyline\n";
        content += "      (pts";
        // 存储第一个点以便在最后重
        QString firstPoint;
        QString lastPoint;  // 用于检测重复点
        for (int i = 0; i < points.size(); i += 2) {
            if (i + 1 < points.size()) {
                // 转换为相对于边界框的坐标，并转换为毫米
                double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

                // 避免重复点
                if (point != lastPoint) {
                    content += point;
                    if (i == 0) {
                        firstPoint = point;
                    }
                    lastPoint = point;
                }
            }
        }
        // 多边形总是重复第一个点以闭合
        if (!firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        // 根据 fillColor 属性设置填充类型
        if (polygon.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}
QString SymbolGraphicsGenerator::generatePolyline(const SymbolPolyline& polyline) const {
    QString content;
    double strokeWidth = pxToMm(polyline.strokeWidth);

    // 解析点数据
    QStringList points = polyline.points.split(" ");
    // 过滤掉空字符串
    points.removeAll("");

    // 至少需要2 个有效的点（4 个坐标值）
    if (points.size() >= 4) {
        content += "    (polyline\n";
        content += "      (pts";
        // 存储第一个点
        QString firstPoint;
        QString lastPoint;  // 用于检测重复点
        for (int i = 0; i < points.size(); i += 2) {
            if (i + 1 < points.size()) {
                // 转换为相对于边界框的坐标，并转换为毫米
                double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

                // 避免重复点
                if (point != lastPoint) {
                    content += point;
                    if (i == 0) {
                        firstPoint = point;
                    }
                    lastPoint = point;
                }
            }
        }
        // 只有fillColor 为 true 时才重复第一个点
        if (polyline.fillColor && !firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        // 填充类型fillColor 决定
        if (polyline.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}
QString SymbolGraphicsGenerator::generatePath(const SymbolPath& path) const {
    QString content;

    // 使用SvgPathParser解析SVG路径
    QList<QPointF> points = SvgPathParser::parsePath(path.paths);

    // 生成polyline
    if (!points.isEmpty()) {
        content += "    (polyline\n";
        content += "      (pts";

        QString lastPoint;
        for (const QPointF& pt : points) {
            // 转换为相对于边界框的坐标，并转换为毫米
            double x = pxToMm(pt.x() - m_currentBBox.x);
            double y = -pxToMm(pt.y() - m_currentBBox.y);  // Y 轴翻转

            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

            // 避免重复点
            if (point != lastPoint) {
                content += point;
                lastPoint = point;
            }
        }

        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(pxToMm(path.strokeWidth), 0, 'f', 3);

        // 填充类型fillColor 决定
        if (path.fillColor) {
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

QString SymbolGraphicsGenerator::generateText(const SymbolText& text) const {
    QString content;

    // V6 使用毫米单位
    double x = pxToMm(text.posX - m_currentBBox.x);
    double y = -pxToMm(text.posY - m_currentBBox.y);  // Y 轴翻转

    // 计算字体大小（从pt转换为mm）
    double fontSize = text.textSize * 0.352778;  // 1pt = 0.352778mm

    // 处理粗体和斜
    QString fontStyle = "";
    if (text.bold) {
        fontStyle += "bold ";
    }
    if (text.italic == "1" || text.italic == "Italic" || text.italic == "italic") {
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

    // 处理可见
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

double SymbolGraphicsGenerator::pxToMil(double px) const {
    return 10.0 * px;
}

double SymbolGraphicsGenerator::pxToMm(double px) const {
    return GeometryUtils::convertToMm(px);
}

QString SymbolGraphicsGenerator::pinTypeToKicad(PinType pinType) const {
    switch (pinType) {
        case PinType::Unspecified:
            return "unspecified";
        case PinType::Input:
            return "input";
        case PinType::Output:
            return "output";
        case PinType::Bidirectional:
            return "bidirectional";
        case PinType::Power:
            return "power_in";
        default:
            return "unspecified";
    }
}

QString SymbolGraphicsGenerator::pinStyleToKicad(PinStyle pinStyle) const {
    switch (pinStyle) {
        case PinStyle::Line:
            return "line";
        case PinStyle::Inverted:
            return "inverted";
        case PinStyle::Clock:
            return "clock";
        case PinStyle::InvertedClock:
            return "inverted_clock";
        case PinStyle::InputLow:
            return "input_low";
        case PinStyle::ClockLow:
            return "clock_low";
        case PinStyle::OutputLow:
            return "output_low";
        case PinStyle::EdgeClockHigh:
            return "edge_clock_high";
        case PinStyle::NonLogic:
            return "non_logic";
        default:
            return "line";
    }
}

QString SymbolGraphicsGenerator::rotationToKicadOrientation(int rotation) const {
    switch (rotation) {
        case 0:
            return "right";
        case 90:
            return "up";
        case 180:
            return "left";
        case 270:
            return "down";
        default:
            return "right";
    }
}

SymbolBBox SymbolGraphicsGenerator::calculatePartBBox(const SymbolPart& part) const {
    SymbolBBox bbox;
    bbox.x = 0.0;
    bbox.y = 0.0;
    bbox.width = 0.0;
    bbox.height = 0.0;

    // 如果子部分有坐标原点，使用它作为基准
    if (part.originX != 0.0 || part.originY != 0.0) {
        bbox.x = part.originX;
        bbox.y = part.originY;
    }

    // 计算图形元素的边界
    double minX = bbox.x;
    double minY = bbox.y;
    double maxX = bbox.x;
    double maxY = bbox.y;

    // 遍历所有图形元素，计算边界
    auto updateBounds = [&](double x, double y, double w, double h) {
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x + w);
        maxY = qMax(maxY, y + h);
    };

    // 处理矩形
    for (const auto& rect : part.rectangles) {
        updateBounds(rect.posX, rect.posY, rect.width, rect.height);
    }

    // 处理圆
    for (const auto& circle : part.circles) {
        double radius = circle.radius;
        updateBounds(circle.centerX - radius, circle.centerY - radius, radius * 2, radius * 2);
    }

    // 处理椭圆
    for (const auto& ellipse : part.ellipses) {
        updateBounds(ellipse.centerX - ellipse.radiusX,
                     ellipse.centerY - ellipse.radiusY,
                     ellipse.radiusX * 2,
                     ellipse.radiusY * 2);
    }

    // 处理圆弧（使用路径点）
    for (const auto& arc : part.arcs) {
        for (const auto& point : arc.path) {
            minX = qMin(minX, point.x());
            minY = qMin(minY, point.y());
            maxX = qMax(maxX, point.x());
            maxY = qMax(maxY, point.y());
        }
    }

    // 处理引脚
    for (const auto& pin : part.pins) {
        minX = qMin(minX, pin.settings.posX);
        minY = qMin(minY, pin.settings.posY);
        maxX = qMax(maxX, pin.settings.posX);
        maxY = qMax(maxY, pin.settings.posY);
    }

    // 处理文本
    for (const auto& text : part.texts) {
        minX = qMin(minX, text.posX);
        minY = qMin(minY, text.posY);
        maxX = qMax(maxX, text.posX);
        maxY = qMax(maxY, text.posY);
    }

    // 更新边界框
    bbox.x = minX;
    bbox.y = minY;
    bbox.width = maxX - minX;
    bbox.height = maxY - minY;

    qDebug() << "Part BBox - x:" << bbox.x << "y:" << bbox.y << "width:" << bbox.width << "height:" << bbox.height;

    return bbox;
}

}  // namespace EasyKiConverter
