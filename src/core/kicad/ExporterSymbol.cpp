#include "ExporterSymbol.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "src/core/utils/GeometryUtils.h"

namespace EasyKiConverter {

ExporterSymbol::ExporterSymbol(QObject *parent)
    : QObject(parent)
    , m_kicadVersion(KicadVersion::V6)
{
}

ExporterSymbol::~ExporterSymbol()
{
}

void ExporterSymbol::setKicadVersion(KicadVersion version)
{
    m_kicadVersion = version;
}

ExporterSymbol::KicadVersion ExporterSymbol::getKicadVersion() const
{
    return m_kicadVersion;
}

bool ExporterSymbol::exportSymbol(const SymbolData &symbolData, const QString &filePath, KicadVersion version)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 生成符号内容（不包含库头，仅单个符号定义）
    QString content = generateSymbolContent(symbolData, "", version);

    out << content;
    file.close();

    qDebug() << "Symbol exported to:" << filePath;
    return true;
}

bool ExporterSymbol::exportSymbolLibrary(const QList<SymbolData> &symbols, const QString &libName, const QString &filePath, KicadVersion version)
{
    qDebug() << "=== Export Symbol Library ===";
    qDebug() << "Library name:" << libName;
    qDebug() << "Output path:" << filePath;
    qDebug() << "Symbol count:" << symbols.count();
    qDebug() << "KiCad version:" << (version == KicadVersion::V6 ? "V6" : "V5");
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 生成头部
    qDebug() << "Generating header...";
    out << generateHeader(libName, version);

    // 生成所有符号
    int index = 0;
    for (const SymbolData &symbol : symbols) {
        qDebug() << "Exporting symbol" << (++index) << "of" << symbols.count() << ":" << symbol.info().name;
        out << generateSymbolContent(symbol, libName, version);
    }
    // 生成尾部
    if (version == KicadVersion::V6) {
        out << ")\n"; // 闭合 kicad_symbol_lib
    }

    file.close();

    qDebug() << "Symbol library exported successfully to:" << filePath;
    return true;
}

QString ExporterSymbol::generateHeader(const QString &libName, KicadVersion version) const
{
    Q_UNUSED(libName);
    if (version == KicadVersion::V6) {
        return QString("(kicad_symbol_lib\n"
                       "  (version 20211014)\n"
                       "  (generator https://github.com/tangsangsimida/EasyKiConverter)");
    } else {
        return QString("EESchema-LIBRARY Version 2.4\n"
                       "#encoding utf-8\n"
                       "#\n# %1\n#\n").arg(libName);
    }
}

QString ExporterSymbol::generateSymbolContent(const SymbolData &symbolData, const QString &libName, KicadVersion version) const
{
    QString content;

    if (version == KicadVersion::V6) {
        // V6 格式 - 主符号定义（包含属性）
        content += QString("  (symbol \"%1\"\n").arg(symbolData.info().name);
        content += "    (in_bom yes)\n";
        content += "    (on_board yes)\n";

        // 设置当前边界框，用于图形元素的相对坐标计算
        m_currentBBox = symbolData.bbox();

        qDebug() << "BBox - x:" << m_currentBBox.x << "y:" << m_currentBBox.y;

        // 计算 y_high 和 y_low（使用引脚坐标，与 Python 版本保持一致）
        double yHigh = 0.0;
        double yLow = 0.0;
        QList<SymbolPin> pins = symbolData.pins();
        if (!pins.isEmpty()) {
            qDebug() << "Pin coordinates calculation:";
            for (const SymbolPin &pin : pins) {
                double pinY = -pxToMm(pin.settings.posY - m_currentBBox.y);
                qDebug() << "  Pin" << pin.settings.spicePinNumber << "- raw Y:" << pin.settings.posY << "converted Y:" << pinY;
                yHigh = qMax(yHigh, pinY);
                yLow = qMin(yLow, pinY);
            }
        }

        qDebug() << "Final yHigh:" << yHigh << "yLow:" << yLow;

        // 生成属性
        double fieldOffset = 5.08; // FIELD_OFFSET_START
        double fontSize = 1.27; // PROPERTY_FONT_SIZE

        // 辅助函数：转义属性值
        auto escapePropertyValue = [](const QString &value) -> QString {
            QString escaped = value;
            escaped.replace("\"", "\\\"");  // 转义引号
            escaped.replace("\n", " ");     // 移除换行符
            escaped.replace("\t", " ");     // 移除制表符
            return escaped.trimmed();
        };

        // Reference 属性
        QString refPrefix = symbolData.info().prefix;
        refPrefix.replace("?", "");  // 移除 "?" 后缀
// Reference 属性
        content += QString("    (property\n");
        content += QString("      \"Reference\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(refPrefix));
        content += "      (id 0)\n";
        content += QString("      (at 0 %1 0)\n").arg(yHigh + fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) ) )\n").arg(fontSize).arg(fontSize);
        content += "    )\n";

        // Value 属性
        content += QString("    (property\n");
        content += QString("      \"Value\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().name));
        content += "      (id 1)\n";
        content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) ) )\n").arg(fontSize).arg(fontSize);
        content += "    )\n";

        // Footprint 属性
        if (!symbolData.info().package.isEmpty()) {
            fieldOffset += 2.54; // FIELD_OFFSET_INCREMENT
            content += QString("    (property\n");
            content += QString("      \"Footprint\"\n");
            // 添加库前缀：libName:package
            QString footprintPath = QString("%1:%2").arg(libName, symbolData.info().package);
            content += QString("      \"%1\"\n").arg(escapePropertyValue(footprintPath));
            content += "      (id 2)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) ) hide)\n").arg(fontSize).arg(fontSize);
            content += "    )\n";
        }

        // Datasheet 属性
        if (!symbolData.info().datasheet.isEmpty()) {
            fieldOffset += 2.54;
            content += QString("    (property\n");
            content += QString("      \"Datasheet\"\n");
            content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().datasheet));
            content += "      (id 3)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) ) hide)\n").arg(fontSize).arg(fontSize);
            content += "    )\n";
        }

        // Manufacturer 属性
        if (!symbolData.info().manufacturer.isEmpty()) {
            fieldOffset += 2.54;
            content += QString("    (property\n");
            content += QString("      \"Manufacturer\"\n");
            content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().manufacturer));
            content += "      (id 4)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) ) hide)\n").arg(fontSize).arg(fontSize);
            content += "    )\n";
        }

        // LCSC Part 属性
        if (!symbolData.info().lcscId.isEmpty()) {
            fieldOffset += 2.54;
            content += QString("    (property\n");
            content += QString("      \"LCSC Part\"\n");
            content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().lcscId));
            content += "      (id 5)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) ) hide)\n").arg(fontSize).arg(fontSize);
            content += "    )\n";
        }

        // 生成子符号（包含图形元素和引脚）
        content += QString("    (symbol \"%1_0_1\"\n").arg(symbolData.info().name);

        // 生成图形元素（矩形、圆等）
        qDebug() << "=== Symbol Export Debug ===";
        qDebug() << "Symbol name:" << symbolData.info().name;
        qDebug() << "Rectangle count:" << symbolData.rectangles().count();
        if (!symbolData.rectangles().isEmpty()) {
            qDebug() << "First rectangle - width:" << symbolData.rectangles().first().width 
                     << "height:" << symbolData.rectangles().first().height;
        }
        qDebug() << "BBox - x:" << m_currentBBox.x << "y:" << m_currentBBox.y;
        qDebug() << "Pin count:" << pins.count();
        qDebug() << "yHigh:" << yHigh << "yLow:" << yLow;
        
        // 只使用原始矩形数据（如果有）
        for (const SymbolRectangle &rect : symbolData.rectangles()) {
            qDebug() << "Rectangle - posX:" << rect.posX << "posY:" << rect.posY 
                     << "width:" << rect.width << "height:" << rect.height;
            content += generateRectangle(rect, version);
        }

        qDebug() << "Circle count:" << symbolData.circles().count();
        // 生成圆
        for (const SymbolCircle &circle : symbolData.circles()) {
            content += generateCircle(circle, version);
        }

        qDebug() << "Arc count:" << symbolData.arcs().count();
        // 生成圆弧
        for (const SymbolArc &arc : symbolData.arcs()) {
            content += generateArc(arc, version);
        }

        qDebug() << "Ellipse count:" << symbolData.ellipses().count();
        // 生成椭圆
        for (const SymbolEllipse &ellipse : symbolData.ellipses()) {
            content += generateEllipse(ellipse, version);
        }

        qDebug() << "Polygon count:" << symbolData.polygons().count();
        // 生成多边形
        for (const SymbolPolygon &polygon : symbolData.polygons()) {
            content += generatePolygon(polygon, version);
        }

        qDebug() << "Polyline count:" << symbolData.polylines().count();
        // 生成多段线
        for (const SymbolPolyline &polyline : symbolData.polylines()) {
            content += generatePolyline(polyline, version);
        }

        qDebug() << "Path count:" << symbolData.paths().count();
        // 生成路径
        for (const SymbolPath &path : symbolData.paths()) {
            content += generatePath(path, version);
        }

        qDebug() << "Generating" << pins.count() << "pins...";

        // 生成引脚
        for (const SymbolPin &pin : symbolData.pins()) {
            content += generatePin(pin, symbolData.bbox(), version);
        }

        content += "    )\n"; // 结束子符号
        content += "  )\n"; // 结束主符号
    } else {
        // V5 格式
        content += QString("#\n# %1\n#\n").arg(symbolData.info().name);
        content += QString("DEF %1 %2 0 0 Y Y 1 F N\n").arg(symbolData.info().name).arg(symbolData.info().prefix);
        content += "F0 \"\" 0 0 50 H I C CNN\n";
        content += "F1 \"\" 0 0 50 H I C CNN\n";
        content += "DRAW\n";

        // 生成引脚
        for (const SymbolPin &pin : symbolData.pins()) {
            content += generatePin(pin, symbolData.bbox(), version);
        }

        // 生成矩形
        for (const SymbolRectangle &rect : symbolData.rectangles()) {
            content += generateRectangle(rect, version);
        }

        // 生成圆
        for (const SymbolCircle &circle : symbolData.circles()) {
            content += generateCircle(circle, version);
        }

        // 生成圆弧
        for (const SymbolArc &arc : symbolData.arcs()) {
            content += generateArc(arc, version);
        }

        // 生成椭圆
        for (const SymbolEllipse &ellipse : symbolData.ellipses()) {
            content += generateEllipse(ellipse, version);
        }

        // 生成多边形
        for (const SymbolPolygon &polygon : symbolData.polygons()) {
            content += generatePolygon(polygon, version);
        }

        // 生成多段线
        for (const SymbolPolyline &polyline : symbolData.polylines()) {
            content += generatePolyline(polyline, version);
        }

        // 生成路径
        for (const SymbolPath &path : symbolData.paths()) {
            content += generatePath(path, version);
        }

        content += "ENDDRAW\n";
        content += "ENDDEF\n";
    }

    return content;
}

QString ExporterSymbol::generatePin(const SymbolPin &pin, const SymbolBBox &bbox, KicadVersion version) const
{
    QString content;

    // 使用边界框偏移量计算相对坐标（Python 版本的做法）
    double x = pxToMm(pin.settings.posX - bbox.x);
    double y = -pxToMm(pin.settings.posY - bbox.y); // Y 轴翻转

    // 从路径中提取引脚长度（格式如 "M0 0h100"）
    QString path = pin.pinPath.path;
    int hIndex = path.indexOf('h');
    double length = 0;
    if (hIndex >= 0) {
        QString lengthStr = path.mid(hIndex + 1);
        length = lengthStr.toDouble();
        length = pxToMm(length);
    }

    // 确保引脚长度不为 0
    if (length < 0.01) {
        length = 2.54; // 默认引脚长度（100mil）
    }

    QString kicadPinType = pinTypeToKicad(pin.settings.type);

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

    // 处理引脚名称和编号（去除空格）
    QString pinName = pin.name.text.trimmed();
    QString pinNumber = pin.settings.spicePinNumber.trimmed();

    // 如果引脚名称为空，使用引脚编号
    if (pinName.isEmpty()) {
        pinName = pinNumber;
    }

    double orientation = pin.settings.rotation;

    if (version == KicadVersion::V6) {
        // Python 版本使用 (180 + orientation) % 360 计算方向
        // 注意：orientation 是 0, 90, 180, 270 的整数
        double kicadOrientation = (180.0 + orientation);
        while (kicadOrientation >= 360.0) kicadOrientation -= 360.0;

        content += QString("    (pin %1 %2\n").arg(kicadPinType).arg(kicadPinStyle);
        content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(kicadOrientation, 0, 'f', 0);
        content += QString("      (length %1)\n").arg(length, 0, 'f', 2);
        content += QString("      (name \"%1\" (effects (font (size 1.27 1.27))))\n").arg(pinName);
        content += QString("      (number \"%1\" (effects (font (size 1.27 1.27))))\n").arg(pinNumber);
        content += "    )\n";
    } else {
        // V5 格式
        content += QString("X %1 %2 %3 %4 %5 %6 1 1 %7\n")
            .arg(pinName)
            .arg(pinNumber)
            .arg(x)
            .arg(y)
            .arg(length)
            .arg(orientation)
            .arg(kicadPinType);
    }

    return content;
}

QString ExporterSymbol::generateRectangle(const SymbolRectangle &rect, KicadVersion version) const
{
    QString content;

    double x0, y0, x1, y1;

    if (version == KicadVersion::V6) {
        // V6 使用毫米单位
        // 使用原始矩形的坐标和尺寸
        x0 = pxToMm(rect.posX - m_currentBBox.x);
        y0 = -pxToMm(rect.posY - m_currentBBox.y); // Y 轴翻转
        x1 = pxToMm(rect.posX + rect.width - m_currentBBox.x);
        y1 = -pxToMm(rect.posY + rect.height - m_currentBBox.y); // Y 轴翻转

        content += "    (rectangle\n";
        content += QString("      (start %1 %2)\n").arg(x0, 0, 'f', 2).arg(y0, 0, 'f', 2);
        content += QString("      (end %1 %2)\n").arg(x1, 0, 'f', 2).arg(y1, 0, 'f', 2);
        content += "      (stroke (width 0) (type default) (color 0 0 0 0))\n";
        content += "      (fill (type background))\n";
        content += "    )\n";
    } else {
        // V5 使用 mil 单位
        x0 = pxToMil(rect.posX - m_currentBBox.x);
        y0 = pxToMil(rect.posY - m_currentBBox.y);
        x1 = pxToMil(rect.width) + x0;
        y1 = pxToMil(rect.height) + y0;

        content += QString("S %1 %2 %3 %4 0 1 0 f\n")
            .arg(x0).arg(y0).arg(x1).arg(y1);
    }

    return content;
}

QString ExporterSymbol::generateCircle(const SymbolCircle &circle, KicadVersion version) const
{
    QString content;

    double cx, cy, radius;

    if (version == KicadVersion::V6) {
        // V6 使用毫米单位
        cx = pxToMm(circle.centerX - m_currentBBox.x);
        cy = -pxToMm(circle.centerY - m_currentBBox.y); // Y 轴翻转
        radius = pxToMm(circle.radius);

        content += "    (circle\n";
        content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
        content += QString("      (radius %1)\n").arg(radius, 0, 'f', 2);
        content += "      (stroke (width 0.127) (type default) (color 0 0 0 0))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        // V5 使用 mil 单位
        cx = pxToMil(circle.centerX - m_currentBBox.x);
        cy = pxToMil(circle.centerY - m_currentBBox.y);
        radius = pxToMil(circle.radius);

        content += QString("C %1 %2 %3 0 1 0 N\n")
            .arg(cx).arg(cy).arg(radius);
    }

    return content;
}

QString ExporterSymbol::generateArc(const SymbolArc &arc, KicadVersion version) const
{
    Q_UNUSED(arc);
    QString content;

    // 圆弧需要计算起点和终点
    // 这里简化处理，实际需要根据路径数据计算
    if (version == KicadVersion::V6) {
        content += "    (arc (start 0 0) (end 0 0)\n";
        content += "      (radius 0)\n";
        content += "      (start-angle 0)\n";
        content += "      (end-angle 0)\n";
        content += "      (stroke (width 0.127) (type default))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        content += "A 0 0 0 0 0 0 1 0 N\n";
    }

    return content;
}

QString ExporterSymbol::generateEllipse(const SymbolEllipse &ellipse, KicadVersion version) const
{
    QString content;

    if (version == KicadVersion::V6) {
        // V6 使用毫米单位
        double cx = pxToMm(ellipse.centerX - m_currentBBox.x);
        double cy = -pxToMm(ellipse.centerY - m_currentBBox.y); // Y 轴翻转
        double radiusX = pxToMm(ellipse.radiusX);
        
        // KiCad V6 没有直接的椭圆，用圆近似
        content += "    (circle\n";
        content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
        content += QString("      (radius %1)\n").arg(radiusX, 0, 'f', 2);
        content += "      (stroke (width 0.127) (type default) (color 0 0 0 0))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        // V5 使用 mil 单位
        double cx = pxToMil(ellipse.centerX);
        double cy = pxToMil(ellipse.centerY);
        double radiusX = pxToMil(ellipse.radiusX);
        Q_UNUSED(ellipse.radiusY);

        content += QString("C %1 %2 %3 0 1 0 N\n")
            .arg(cx).arg(cy).arg(radiusX);
    }

    return content;
}

QString ExporterSymbol::generatePolygon(const SymbolPolygon &polygon, KicadVersion version) const
{
    QString content;

    // 解析点数据
    QStringList points = polygon.points.split(" ");
    // 过滤掉空字符串
    points.removeAll("");
    
    // 至少需要 2 个有效的点（4 个坐标值）
    if (points.size() >= 4) {
        if (version == KicadVersion::V6) {
            // KiCad V6 不支持 polygon 元素，使用 polyline 代替
            content += "    (polyline\n";
            content += "      (pts";
            // 存储第一个点以便在最后重复
            QString firstPoint;
            for (int i = 0; i < points.size(); i += 2) {
                if (i + 1 < points.size()) {
                    // 转换为相对于边界框的坐标，并转换为毫米
                    double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                    double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                    QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
                    content += point;
                    if (i == 0) {
                        firstPoint = point;
                    }
                }
            }
            // 多边形总是重复第一个点以闭合
            if (!firstPoint.isEmpty()) {
                content += firstPoint;
            }
            content += ")\n";
            content += "      (stroke (width 0) (type default) (color 0 0 0 0))\n";
            // 多边形总是使用 background 填充（因为重复了第一个点）
            content += "      (fill (type background))\n";
            content += "    )\n";
        } else {
            content += QString("P %1").arg(points.size() / 2);
            for (int i = 0; i < points.size(); i += 2) {
                if (i + 1 < points.size()) {
                    double x = pxToMil(points[i].toDouble());
                    double y = pxToMil(points[i + 1].toDouble());
                    content += QString(" %1 %2").arg(x).arg(y);
                }
            }
            content += " 0 1 0 f\n"; // V5 使用 f 表示填充
        }
    }

    return content;
}

QString ExporterSymbol::generatePolyline(const SymbolPolyline &polyline, KicadVersion version) const
{
    QString content;

    // 解析点数据
    QStringList points = polyline.points.split(" ");
    // 过滤掉空字符串
    points.removeAll("");
    
    // 至少需要 2 个有效的点（4 个坐标值）
    if (points.size() >= 4) {
        if (version == KicadVersion::V6) {
            content += "    (polyline\n";
            content += "      (pts";
            // 存储第一个点
            QString firstPoint;
            for (int i = 0; i < points.size(); i += 2) {
                if (i + 1 < points.size()) {
                    // 转换为相对于边界框的坐标，并转换为毫米
                    double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                    double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                    QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
                    content += point;
                    if (i == 0) {
                        firstPoint = point;
                    }
                }
            }
            // 只有当 fillColor 为 true 时才重复第一个点
            if (polyline.fillColor && !firstPoint.isEmpty()) {
                content += firstPoint;
            }
            content += ")\n";
            content += "      (stroke (width 0) (type default) (color 0 0 0 0))\n";
            // 填充类型由 fillColor 决定
            if (polyline.fillColor) {
                content += "      (fill (type background))\n";
            } else {
                content += "      (fill (type none))\n";
            }
            content += "    )\n";
        } else {
            content += QString("P %1").arg(points.size() / 2);
            for (int i = 0; i < points.size(); i += 2) {
                if (i + 1 < points.size()) {
                    double x = pxToMil(points[i].toDouble());
                    double y = pxToMil(points[i + 1].toDouble());
                    content += QString(" %1 %2").arg(x).arg(y);
                }
            }
            // V5 填充类型由 fillColor 决定
            if (polyline.fillColor) {
                content += " 0 1 0 f\n";
            } else {
                content += " 0 1 0 N\n";
            }
        }
    }

    return content;
}

QString ExporterSymbol::generatePath(const SymbolPath &path, KicadVersion version) const
{
    Q_UNUSED(path);
    QString content;

    // 路径需要特殊处理，这里简化处理
    if (version == KicadVersion::V6) {
        content += "    (polyline (pts (xy 0 0))\n";
        content += "      (stroke (width 0.127) (type default))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        content += "P 2 0 0 0 N\n";
    }

    return content;
}

double ExporterSymbol::pxToMil(double px) const
{
    return 10.0 * px;
}

double ExporterSymbol::pxToMm(double px) const
{
    return GeometryUtils::convertToMm(px);
}

QString ExporterSymbol::pinTypeToKicad(PinType pinType) const
{
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

QString ExporterSymbol::pinStyleToKicad(PinStyle pinStyle) const
{
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

QString ExporterSymbol::rotationToKicadOrientation(int rotation) const
{
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

} // namespace EasyKiConverter