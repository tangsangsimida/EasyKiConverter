#include "ExporterSymbol.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "src/core/utils/GeometryUtils.h"

namespace EasyKiConverter
{

    ExporterSymbol::ExporterSymbol(QObject *parent)
        : QObject(parent)
    {
    }

    ExporterSymbol::~ExporterSymbol()
    {
    }

    bool ExporterSymbol::exportSymbol(const SymbolData &symbolData, const QString &filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qWarning() << "Failed to open file for writing:" << filePath;
            return false;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        // 生成符号内容（不包含库头，仅单个符号定义）
        QString content = generateSymbolContent(symbolData, "");

        out << content;
        file.close();

        qDebug() << "Symbol exported to:" << filePath;
        return true;
    }

    bool ExporterSymbol::exportSymbolLibrary(const QList<SymbolData> &symbols, const QString &libName, const QString &filePath, bool appendMode)
    {
        qDebug() << "=== Export Symbol Library ===";
        qDebug() << "Library name:" << libName;
        qDebug() << "Output path:" << filePath;
        qDebug() << "Symbol count:" << symbols.count();
        qDebug() << "KiCad version: V6";
        qDebug() << "Append mode:" << appendMode;

        QList<SymbolData> finalSymbols = symbols;

        // 如果启用追加模式且文件已存在，读取现有符号名称
        if (appendMode && QFile::exists(filePath))
        {
            qDebug() << "Existing library found, reading existing symbol names...";
            QSet<QString> existingSymbolNames;

            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QString content = QTextStream(&file).readAll();
                file.close();

                // 提取现有符号名称
                QRegularExpression symbolRegex("\\(symbol\\s+\"([^\"]+)\"\\s");
                QRegularExpressionMatchIterator it = symbolRegex.globalMatch(content);
                while (it.hasNext())
                {
                    QRegularExpressionMatch match = it.next();
                    QString symbolName = match.captured(1);
                    existingSymbolNames.insert(symbolName);
                    qDebug() << "Found existing symbol:" << symbolName;
                }
            }

            qDebug() << "Existing symbols count:" << existingSymbolNames.count();

            // 过滤掉已存在的符号
            QList<SymbolData> filteredSymbols;
            for (const SymbolData &symbol : symbols)
            {
                if (!existingSymbolNames.contains(symbol.info().name))
                {
                    filteredSymbols.append(symbol);
                }
                else
                {
                    qDebug() << "Symbol already exists, skipping:" << symbol.info().name;
                }
            }

            finalSymbols = filteredSymbols;
            qDebug() << "Filtered symbols count (new only):" << finalSymbols.count();
        }

        QFile file(filePath);
        bool fileExists = file.exists();

        // 追加模式：如果文件存在，读取现有内容
        QString existingContent;
        if (appendMode && fileExists)
        {
            qDebug() << "Reading existing library content for append mode...";
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                existingContent = QTextStream(&file).readAll();
                file.close();
                qDebug() << "Read" << existingContent.length() << "bytes from existing library";
            }
        }

        // 打开文件进行写入（覆盖模式）
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qWarning() << "Failed to open file for writing:" << filePath;
            return false;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        // 生成头部（仅当文件不存在或非追加模式时）
        if (!fileExists || !appendMode)
        {
            qDebug() << "Generating header...";
            out << generateHeader(libName);
        }
        else
        {
            qDebug() << "Reusing existing header (append mode)";
            // 写入现有内容（去掉最后的闭合括号）
            if (!existingContent.isEmpty())
            {
                // 找到最后一个闭合括号的位置
                int lastParenIndex = existingContent.lastIndexOf(')');
                if (lastParenIndex >= 0)
                {
                    out << existingContent.left(lastParenIndex);
                }
                else
                {
                    // 如果没有找到闭合括号，直接写入全部内容
                    out << existingContent;
                }
            }
        }

        // 生成所有符号
        int index = 0;
        for (const SymbolData &symbol : finalSymbols)
        {
            qDebug() << "Exporting symbol" << (++index) << "of" << finalSymbols.count() << ":" << symbol.info().name;
            out << generateSymbolContent(symbol, libName);
        }

        // 生成尾部（仅在非追加模式或文件不存在时添加闭合括号）
        if (!appendMode || !fileExists)
        {
            out << ")\n"; // 闭合 kicad_symbol_lib
        }
        else
        {
            // 追加模式：添加闭合括号
            out << "\n)\n"; // 闭合 kicad_symbol_lib
        }

        file.close();
        qDebug() << "Symbol library exported successfully to:" << filePath;
        return true;
    }

    QString ExporterSymbol::generateHeader(const QString &libName) const
    {
        Q_UNUSED(libName);
        return QString("(kicad_symbol_lib\n"
                       "  (version 20211014)\n"
                       "  (generator https://github.com/tangsangsimida/EasyKiConverter)\n");
    }

    QString ExporterSymbol::generateSymbolContent(const SymbolData &symbolData, const QString &libName) const
    {
        QString content;

        // V6 格式 - 主符号定义（包含属性）
        QString cleanSymbolName = symbolData.info().name;
        cleanSymbolName.replace(" ", "_"); // 替换空格为下划线
        content += QString("  (symbol \"%1\"\n").arg(cleanSymbolName);
        content += "    (in_bom yes)\n";
        content += "    (on_board yes)\n";

        // 设置当前边界框，用于图形元素的相对坐标计算
        m_currentBBox = symbolData.bbox();
        qDebug() << "BBox - x:" << m_currentBBox.x << "y:" << m_currentBBox.y;

        // 计算 y_high 和 y_low（使用引脚坐标，与 Python 版本保持一致）
        double yHigh = 0.0;
        double yLow = 0.0;
        QList<SymbolPin> pins = symbolData.pins();
        if (!pins.isEmpty())
        {
            qDebug() << "Pin coordinates calculation:";
            for (const SymbolPin &pin : pins)
            {
                double pinY = -pxToMm(pin.settings.posY - m_currentBBox.y);
                qDebug() << "  Pin" << pin.settings.spicePinNumber << "- raw Y:" << pin.settings.posY << "converted Y:" << pinY;
                yHigh = qMax(yHigh, pinY);
                yLow = qMin(yLow, pinY);
            }
        }

        qDebug() << "Final yHigh:" << yHigh << "yLow:" << yLow;

        // 生成属性
        double fieldOffset = 5.08; // FIELD_OFFSET_START
        double fontSize = 1.27;    // PROPERTY_FONT_SIZE

        // 辅助函数：转义属性值
        auto escapePropertyValue = [](const QString &value) -> QString
        {
            QString escaped = value;
            escaped.replace("\"", "\\\""); // 转义引号
            escaped.replace("\n", " ");    // 移除换行符
            escaped.replace("\t", " ");    // 移除制表符
            return escaped.trimmed();
        };

        // Reference 属性
        QString refPrefix = symbolData.info().prefix;
        refPrefix.replace("?", ""); // 移除 "?" 后缀
        content += QString("    (property\n");
        content += QString("      \"Reference\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(refPrefix));
        content += "      (id 0)\n";
        content += QString("      (at 0 %1 0)\n").arg(yHigh + fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) )\n").arg(fontSize, 0, 'f', 2).arg(fontSize, 0, 'f', 2);
        content += "    )\n";

        // Value 属性
        content += QString("    (property\n");
        content += QString("      \"Value\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().name));
        content += "      (id 1)\n";
        content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) )\n").arg(fontSize, 0, 'f', 2).arg(fontSize, 0, 'f', 2);
        content += "    )\n";

        // Footprint 属性
        if (!symbolData.info().package.isEmpty())
        {
            fieldOffset += 2.54; // FIELD_OFFSET_INCREMENT
            content += QString("    (property\n");
            content += QString("      \"Footprint\"\n");
            // 添加库前缀：libName:package
            QString footprintPath = QString("%1:%2").arg(libName, symbolData.info().package);
            content += QString("      \"%1\"\n").arg(escapePropertyValue(footprintPath));
            content += "      (id 2)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n").arg(fontSize, 0, 'f', 2).arg(fontSize, 0, 'f', 2);
            content += "    )\n";
        }

        // Datasheet 属性
        if (!symbolData.info().datasheet.isEmpty())
        {
            fieldOffset += 2.54;
            content += QString("    (property\n");
            content += QString("      \"Datasheet\"\n");
            content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().datasheet));
            content += "      (id 3)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n").arg(fontSize, 0, 'f', 2).arg(fontSize, 0, 'f', 2);
            content += "    )\n";
        }

        // Manufacturer 属性
        if (!symbolData.info().manufacturer.isEmpty())
        {
            fieldOffset += 2.54;
            content += QString("    (property\n");
            content += QString("      \"Manufacturer\"\n");
            content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().manufacturer));
            content += "      (id 4)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n").arg(fontSize, 0, 'f', 2).arg(fontSize, 0, 'f', 2);
            content += "    )\n";
        }

        // LCSC Part 属性
        if (!symbolData.info().lcscId.isEmpty())
        {
            fieldOffset += 2.54;
            content += QString("    (property\n");
            content += QString("      \"LCSC Part\"\n");
            content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().lcscId));
            content += "      (id 5)\n";
            content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
            content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n").arg(fontSize, 0, 'f', 2).arg(fontSize, 0, 'f', 2);
            content += "    )\n";
        }

        // 生成子符号（包含图形元素和引脚）
        content += QString("    (symbol \"%1_0_1\"\n").arg(cleanSymbolName);

        // 生成图形元素（矩形、圆等）
        qDebug() << "=== Symbol Export Debug ===";
        qDebug() << "Symbol name:" << symbolData.info().name;
        qDebug() << "Rectangle count:" << symbolData.rectangles().count();
        const auto rectangles = symbolData.rectangles();
        if (!rectangles.isEmpty())
        {
            qDebug() << "First rectangle - width:" << rectangles.first().width
                     << "height:" << rectangles.first().height;
        }
        qDebug() << "BBox - x:" << m_currentBBox.x << "y:" << m_currentBBox.y;
        qDebug() << "Pin count:" << pins.count();
        qDebug() << "yHigh:" << yHigh << "yLow:" << yLow;

        // 只使用原始矩形数据（如果有）
        for (const SymbolRectangle &rect : rectangles)
        {
            qDebug() << "Rectangle - posX:" << rect.posX << "posY:" << rect.posY
                     << "width:" << rect.width << "height:" << rect.height;
            content += generateRectangle(rect);
        }

        const auto circles = symbolData.circles();
        qDebug() << "Circle count:" << circles.count();
        // 生成圆
        for (const SymbolCircle &circle : circles)
        {
            content += generateCircle(circle);
        }

        const auto arcs = symbolData.arcs();
        qDebug() << "Arc count:" << arcs.count();
        // 生成圆弧
        for (const SymbolArc &arc : arcs)
        {
            content += generateArc(arc);
        }

        const auto ellipses = symbolData.ellipses();
        qDebug() << "Ellipse count:" << ellipses.count();
        // 生成椭圆
        for (const SymbolEllipse &ellipse : ellipses)
        {
            content += generateEllipse(ellipse);
        }

        const auto polygons = symbolData.polygons();
        qDebug() << "Polygon count:" << polygons.count();
        // 生成多边形
        for (const SymbolPolygon &polygon : polygons)
        {
            content += generatePolygon(polygon);
        }

        const auto polylines = symbolData.polylines();
        qDebug() << "Polyline count:" << polylines.count();
        // 生成多段线
        for (const SymbolPolyline &polyline : polylines)
        {
            content += generatePolyline(polyline);
        }

        const auto paths = symbolData.paths();
        qDebug() << "Path count:" << paths.count();
        // 生成路径
        for (const SymbolPath &path : paths)
        {
            content += generatePath(path);
        }

        qDebug() << "Generating" << pins.count() << "pins...";

        // 生成引脚
        for (const SymbolPin &pin : symbolData.pins())
        {
            content += generatePin(pin, symbolData.bbox());
        }

        content += "    )\n"; // 结束子符号
        content += "  )\n";   // 结束主符号

        return content;
    }

    QString ExporterSymbol::generatePin(const SymbolPin &pin, const SymbolBBox &bbox) const
    {
        QString content;

        // 使用边界框偏移量计算相对坐标（Python 版本的做法）
        double x = pxToMm(pin.settings.posX - bbox.x);
        double y = -pxToMm(pin.settings.posY - bbox.y); // Y 轴翻转

        // 改进引脚长度计算方法（参考 lckiconverter 的实现）
        // 通过计算路径两点距离获取引脚长度，而非从字符串中提取
        QString path = pin.pinPath.path;
        double length = 0;

        // 解析路径字符串，提取点坐标
        QStringList tokens = path.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
        QList<QPointF> points;

        // 简化的路径解析（支持 M、L、H、V 命令）
        double currentX = 0.0;
        double currentY = 0.0;

        for (int i = 0; i < tokens.size(); ) {
            QString token = tokens[i];

            if (token == "M" || token == "m") {
                if (i + 2 < tokens.size()) {
                    double px = tokens[i + 1].toDouble();
                    double py = tokens[i + 2].toDouble();
                    if (token == "m") {
                        px += currentX;
                        py += currentY;
                    }
                    currentX = px;
                    currentY = py;
                    points.append(QPointF(px, py));
                    i += 3;
                } else {
                    i++;
                }
            }
            else if (token == "L" || token == "l") {
                if (i + 2 < tokens.size()) {
                    double px = tokens[i + 1].toDouble();
                    double py = tokens[i + 2].toDouble();
                    if (token == "l") {
                        px += currentX;
                        py += currentY;
                    }
                    currentX = px;
                    currentY = py;
                    points.append(QPointF(px, py));
                    i += 3;
                } else {
                    i++;
                }
            }
            else if (token == "H" || token == "h") {
                if (i + 1 < tokens.size()) {
                    double px = tokens[i + 1].toDouble();
                    if (token == "h") {
                        px += currentX;
                    }
                    currentX = px;
                    points.append(QPointF(currentX, currentY));
                    i += 2;
                } else {
                    i++;
                }
            }
            else if (token == "V" || token == "v") {
                if (i + 1 < tokens.size()) {
                    double py = tokens[i + 1].toDouble();
                    if (token == "v") {
                        py += currentY;
                    }
                    currentY = py;
                    points.append(QPointF(currentX, currentY));
                    i += 2;
                } else {
                    i++;
                }
            }
            else {
                i++;
            }
        }

        // 计算引脚长度（第一点和最后一点的距离）
        if (points.size() >= 2) {
            double dx = points[0].x() - points[points.size() - 1].x();
            double dy = points[0].y() - points[points.size() - 1].y();
            length = std::sqrt(dx * dx + dy * dy);
            length = pxToMm(length);
        } else {
            // 回退到字符串解析方法
            int hIndex = path.indexOf('h');
            if (hIndex >= 0) {
                QString lengthStr = path.mid(hIndex + 1);
                length = lengthStr.toDouble();
                length = pxToMm(length);
            }
        }

        // 确保引脚长度不为 0
        if (length < 0.01) {
            length = 2.54; // 默认引脚长度（100mil）
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

        // 处理引脚名称和编号（去除空格）
        QString pinName = pin.name.text.trimmed();
        QString pinNumber = pin.settings.spicePinNumber.trimmed();

        // 如果引脚名称为空，使用引脚编号
        if (pinName.isEmpty()) {
            pinName = pinNumber;
        }

        double orientation = pin.settings.rotation;

        // Python 版本使用 (180 + orientation) % 360 计算方向
        // 注意：orientation 是 0, 90, 180, 270 的整数
        double kicadOrientation = (180.0 + orientation);
        while (kicadOrientation >= 360.0)
            kicadOrientation -= 360.0;

        content += QString("    (pin %1 %2\n").arg(kicadPinType, kicadPinStyle);
        content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(kicadOrientation, 0, 'f', 0);
        content += QString("      (length %1)\n").arg(length, 0, 'f', 2);
        content += QString("      (name \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinName);
        content += QString("      (number \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinNumber);
        content += "    )\n";

        return content;
    }

    QString ExporterSymbol::generateRectangle(const SymbolRectangle &rect) const
    {
        QString content;

        // V6 使用毫米单位
        // 使用原始矩形的坐标和尺寸
        double x0 = pxToMm(rect.posX - m_currentBBox.x);
        double y0 = -pxToMm(rect.posY - m_currentBBox.y); // Y 轴翻转
        double x1 = pxToMm(rect.posX + rect.width - m_currentBBox.x);
        double y1 = -pxToMm(rect.posY + rect.height - m_currentBBox.y); // Y 轴翻转

        content += "    (rectangle\n";
        content += QString("      (start %1 %2)\n").arg(x0, 0, 'f', 2).arg(y0, 0, 'f', 2);
        content += QString("      (end %1 %2)\n").arg(x1, 0, 'f', 2).arg(y1, 0, 'f', 2);
        content += "      (stroke (width 0.254) (type default))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";

        return content;
    }

    QString ExporterSymbol::generateCircle(const SymbolCircle &circle) const
    {
        QString content;

        // V6 使用毫米单位
        double cx = pxToMm(circle.centerX - m_currentBBox.x);
        double cy = -pxToMm(circle.centerY - m_currentBBox.y); // Y 轴翻转
        double radius = pxToMm(circle.radius);

        content += "    (circle\n";
        content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
        content += QString("      (radius %1)\n").arg(radius, 0, 'f', 2);
        content += "      (stroke (width 0.127) (type default))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";

        return content;
    }

    QString ExporterSymbol::generateArc(const SymbolArc &arc) const
    {
        QString content;

        // KiCad V6 使用三点法定义圆弧：start、mid、end
        if (arc.path.size() >= 3)
        {
            // 提取起点、中点、终点
            const auto path = arc.path; // 避免临时对象detach
            QPointF startPoint = path.first();
            QPointF endPoint = path.last();

            // 计算中点（取中间的点）
            int midIndex = path.size() / 2;
            QPointF midPoint = path[midIndex];

            // 转换为相对于边界框的坐标，并转换为毫米
            double startX = pxToMm(startPoint.x() - m_currentBBox.x);
            double startY = -pxToMm(startPoint.y() - m_currentBBox.y); // Y 轴翻转
            double midX = pxToMm(midPoint.x() - m_currentBBox.x);
            double midY = -pxToMm(midPoint.y() - m_currentBBox.y); // Y 轴翻转
            double endX = pxToMm(endPoint.x() - m_currentBBox.x);
            double endY = -pxToMm(endPoint.y() - m_currentBBox.y); // Y 轴翻转

            content += "    (arc\n";
            content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
            content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
            content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
            content += "      (stroke (width 0.127) (type default))\n";
            content += "      (fill (type none))\n";
            content += "    )\n";
        }
        else if (arc.path.size() == 2)
        {
            // 只有两个点，计算中点
            const auto path = arc.path; // 避免临时对象detach
            QPointF startPoint = path.first();
            QPointF endPoint = path.last();
            QPointF midPoint = (startPoint + endPoint) / 2;

            // 转换为相对于边界框的坐标，并转换为毫米
            double startX = pxToMm(startPoint.x() - m_currentBBox.x);
            double startY = -pxToMm(startPoint.y() - m_currentBBox.y); // Y 轴翻转
            double midX = pxToMm(midPoint.x() - m_currentBBox.x);
            double midY = -pxToMm(midPoint.y() - m_currentBBox.y); // Y 轴翻转
            double endX = pxToMm(endPoint.x() - m_currentBBox.x);
            double endY = -pxToMm(endPoint.y() - m_currentBBox.y); // Y 轴翻转

            content += "    (arc\n";
            content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
            content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
            content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
            content += "      (stroke (width 0.127) (type default))\n";
            content += "      (fill (type none))\n";
            content += "    )\n";
        }
        else
        {
            // 点数不足，跳过或生成一个默认的arc
            qDebug() << "Warning: Arc has insufficient points (" << arc.path.size() << "), skipping";
        }

        return content;
    }

    QString ExporterSymbol::generateEllipse(const SymbolEllipse &ellipse) const
    {
        QString content;

        // V6 使用毫米单位
        double cx = pxToMm(ellipse.centerX - m_currentBBox.x);
        double cy = -pxToMm(ellipse.centerY - m_currentBBox.y); // Y 轴翻转
        double radiusX = pxToMm(ellipse.radiusX);
        double radiusY = pxToMm(ellipse.radiusY);

        // 如果是圆形（radiusX ≈ radiusY），使用 circle 元素
        if (qAbs(radiusX - radiusY) < 0.01)
        {
            content += "    (circle\n";
            content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
            content += QString("      (radius %1)\n").arg(radiusX, 0, 'f', 2);
            content += "      (stroke (width 0.127) (type default))\n";
            content += "      (fill (type none))\n";
            content += "    )\n";
        }
        else
        {
            // 椭圆：转换为路径
            // 使用 32 段折线近似椭圆
            content += "    (polyline\n";
            content += "      (pts";

            const int segments = 32;
            for (int i = 0; i <= segments; ++i)
            {
                double angle = 2.0 * M_PI * i / segments;
                double x = cx + radiusX * std::cos(angle);
                double y = cy + radiusY * std::sin(angle);
                content += QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
            }

            content += ")\n";
            content += "      (stroke (width 0.127) (type default))\n";

            // 根据 fillColor 属性设置填充类型
            if (ellipse.fillColor)
            {
                content += "      (fill (type background))\n";
            }
            else
            {
                content += "      (fill (type none))\n";
            }
            content += "    )\n";
        }

        return content;
    }

    QString ExporterSymbol::generatePolygon(const SymbolPolygon &polygon) const
    {
        QString content;

        // 解析点数据
        QStringList points = polygon.points.split(" ");
        // 过滤掉空字符串
        points.removeAll("");

        // 至少需要 2 个有效的点（4 个坐标值）
        if (points.size() >= 4)
        {
            // KiCad V6 不支持 polygon 元素，使用 polyline 代替
            content += "    (polyline\n";
            content += "      (pts";
            // 存储第一个点以便在最后重复
            QString firstPoint;
            QString lastPoint; // 用于检测重复点
            for (int i = 0; i < points.size(); i += 2)
            {
                if (i + 1 < points.size())
                {
                    // 转换为相对于边界框的坐标，并转换为毫米
                    double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                    double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                    QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

                    // 避免重复点
                    if (point != lastPoint)
                    {
                        content += point;
                        if (i == 0)
                        {
                            firstPoint = point;
                        }
                        lastPoint = point;
                    }
                }
            }
            // 多边形总是重复第一个点以闭合
            if (!firstPoint.isEmpty() && firstPoint != lastPoint)
            {
                content += firstPoint;
            }
            content += ")\n";
            content += "      (stroke (width 0.254) (type default))\n";
            // 根据 fillColor 属性设置填充类型（与 Python 版本一致）
            if (polygon.fillColor)
            {
                content += "      (fill (type background))\n";
            }
            else
            {
                content += "      (fill (type none))\n";
            }
            content += "    )\n";
        }

        return content;
    }
    QString ExporterSymbol::generatePolyline(const SymbolPolyline &polyline) const
    {
        QString content;

        // 解析点数据
        QStringList points = polyline.points.split(" ");
        // 过滤掉空字符串
        points.removeAll("");

        // 至少需要 2 个有效的点（4 个坐标值）
        if (points.size() >= 4)
        {
            content += "    (polyline\n";
            content += "      (pts";
            // 存储第一个点
            QString firstPoint;
            QString lastPoint; // 用于检测重复点
            for (int i = 0; i < points.size(); i += 2)
            {
                if (i + 1 < points.size())
                {
                    // 转换为相对于边界框的坐标，并转换为毫米
                    double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                    double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                    QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

                    // 避免重复点
                    if (point != lastPoint)
                    {
                        content += point;
                        if (i == 0)
                        {
                            firstPoint = point;
                        }
                        lastPoint = point;
                    }
                }
            }
            // 只有当 fillColor 为 true 时才重复第一个点（与 Python 版本一致）
            if (polyline.fillColor && !firstPoint.isEmpty() && firstPoint != lastPoint)
            {
                content += firstPoint;
            }
            content += ")\n";
            content += "      (stroke (width 0.254) (type default))\n";
            // 填充类型由 fillColor 决定（与 Python 版本一致）
            if (polyline.fillColor)
            {
                content += "      (fill (type background))\n";
            }
            else
            {
                content += "      (fill (type none))\n";
            }
            content += "    )\n";
        }

        return content;
    }
    QString ExporterSymbol::generatePath(const SymbolPath &path) const
    {
        QString content;

        // 解析 SVG 路径（支持 M、L、Z 命令，与 Python 版本一致）
        QStringList rawPts = path.paths.split(" ");
        // 过滤掉空字符串
        rawPts.removeAll("");

        QList<double> xPoints;
        QList<double> yPoints;

        // 解析路径命令
        for (int i = 0; i < rawPts.size(); ++i)
        {
            QString token = rawPts[i];

            if (token == "M" || token == "L")
            {
                // MoveTo 或 LineTo 命令
                if (i + 2 < rawPts.size())
                {
                    double x = pxToMm(rawPts[i + 1].toDouble() - m_currentBBox.x);
                    double y = -pxToMm(rawPts[i + 2].toDouble() - m_currentBBox.y); // Y 轴翻转
                    xPoints.append(x);
                    yPoints.append(y);
                    i += 2; // 跳过已处理的坐标
                }
            }
            else if (token == "Z" || token == "z")
            {
                // ClosePath 命令：重复第一个点以闭合路径
                if (!xPoints.isEmpty() && !yPoints.isEmpty())
                {
                    xPoints.append(xPoints.first());
                    yPoints.append(yPoints.first());
                }
            }
            else if (token == "C")
            {
                // Bezier 曲线（暂不支持，跳过）
                // TODO: 添加 Bezier 曲线支持
                i += 6; // 跳过 Bezier 控制点和终点
            }
        }

        // 生成 polyline
        if (!xPoints.isEmpty() && !yPoints.isEmpty())
        {
            content += "    (polyline\n";
            content += "      (pts";

            QString lastPoint;
            for (int i = 0; i < qMin(xPoints.size(), yPoints.size()); ++i)
            {
                QString point = QString(" (xy %1 %2)")
                                    .arg(xPoints[i], 0, 'f', 2)
                                    .arg(yPoints[i], 0, 'f', 2);

                // 避免重复点
                if (point != lastPoint)
                {
                    content += point;
                    lastPoint = point;
                }
            }

            content += ")\n";
            content += "      (stroke (width 0.127) (type default))\n";

            // 填充类型由 fillColor 决定
            if (path.fillColor)
            {
                content += "      (fill (type background))\n";
            }
            else
            {
                content += "      (fill (type none))\n";
            }

            content += "    )\n";
        }
        else
        {
            // 如果没有有效点，生成占位符
            content += "    (polyline (pts (xy 0 0))\n";
            content += "      (stroke (width 0.127) (type default))\n";
            content += "      (fill (type none))\n";
            content += "    )\n";
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
        switch (pinType)
        {
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
        switch (pinStyle)
        {
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
        switch (rotation)
        {
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
